#include "server.h"
#include "client_registry.h"
#include "protocol.h"
#include "transaction.h"
#include "protocol.h"
#include "csapp.h"
#include "debug.h"
#include "data.h"
#include "store.h"
#include <time.h>

CLIENT_REGISTRY *client_registry;
/*
 * Thread function for the thread that handles client requests.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 */

void prepare_pkt(XACTO_PACKET *pkt, TRANS_STATUS status) {
    struct timespec clock;
    clock_gettime( CLOCK_MONOTONIC, &clock);
    pkt->status = status;
    pkt->size = 0;
    pkt->null = 0;
    pkt->timestamp_sec = clock.tv_sec;
    pkt->timestamp_nsec = clock.tv_nsec;
    if (pkt->type == XACTO_REPLY_PKT) {
        pkt->timestamp_sec = htonl(pkt->timestamp_sec);
        pkt->timestamp_nsec = htonl(pkt->timestamp_nsec);
    }
    if (pkt->type == XACTO_VALUE_PKT) {
        pkt->timestamp_sec = ntohl(pkt->timestamp_sec);
        pkt->timestamp_nsec = ntohl(pkt->timestamp_nsec);
    }
}

void *xacto_client_service(void *arg) {
	debug("Inside Xacto_client_service");
	int fd = *(int*)arg;
	pthread_detach(pthread_self());
    Free(arg);
    creg_register(client_registry, fd);
    TRANSACTION *new = trans_create();
    XACTO_PACKET *pkt = Malloc(sizeof(XACTO_PACKET));
	void **datap = Malloc(sizeof(void **));

    while(1) {
    	if (proto_recv_packet(fd, pkt, NULL) == -1) {
            break;
        }
		BLOB *key_blob, *value_blob;
    	KEY *key;
		if (pkt->type == XACTO_PUT_PKT) {
			debug("[%d] PUT packet received", fd);
			proto_recv_packet(fd, pkt, datap);

			//receive key
            pkt->size = ntohl(pkt->size);
			debug("Received key, size %d", pkt->size);
            key_blob = blob_create(*datap,pkt->size);
            key = key_create(key_blob);
            Free(*datap);

            //receive value
            proto_recv_packet(fd,pkt,datap);
            pkt->size = ntohl(pkt->size);
            debug("Received value, size %d",pkt->size);
            value_blob = blob_create(*datap, pkt->size);
            Free(*datap);
            TRANS_STATUS status = store_put(new, key, value_blob);
            debug("!!!done reading packet!!!");

            //send reply packet
            pkt->type = XACTO_REPLY_PKT;
            prepare_pkt(pkt, status);
            proto_send_packet(fd,pkt,NULL);
		}
        if (pkt->type == XACTO_GET_PKT) {
            debug("[%d] GET packet received", fd);
            proto_recv_packet(fd, pkt, datap);

            //receive key
            pkt->size = ntohl(pkt->size);
            debug("Received key, size %d", pkt->size);
            key_blob = blob_create(*datap,pkt->size);
            key = key_create(key_blob);
            Free(*datap);

            BLOB **temp_bp = Malloc(sizeof(BLOB **));
            TRANS_STATUS status = store_get(new,key,temp_bp);

            //send reply packet first
            pkt->type = XACTO_REPLY_PKT;
            prepare_pkt(pkt, status);
            proto_send_packet(fd,pkt,NULL);

            //send value packet
            pkt->type = XACTO_VALUE_PKT;
            prepare_pkt(pkt, status);
            value_blob = *temp_bp;

            if (value_blob == NULL) {
                debug("Such key does not exist");
                pkt->null = 1;
                proto_send_packet(fd,pkt,NULL);
            }
            else {
                pkt->size = ntohl(value_blob->size);
                proto_send_packet(fd,pkt,value_blob->content);
                char *why = "blob from store_get";
                blob_unref(value_blob,why);
            }
            Free(temp_bp);
        }
        if (pkt->type == XACTO_COMMIT_PKT) {
            debug("[%d] COMMIT packet received", fd);
            //commit the transaction
            TRANS_STATUS status = trans_commit(new);
            pkt->type = XACTO_REPLY_PKT;
            prepare_pkt(pkt,status);
            proto_send_packet(fd, pkt, NULL);
        }
    }
    Free(pkt);
    Free(datap);
    creg_unregister(client_registry, fd);
    Close(fd);
    return NULL;
}