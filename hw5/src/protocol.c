#include "protocol.h"
#include "csapp.h"
#include "debug.h"

void convert(XACTO_PACKET *pkt, char val) {
	//if h -> htonl
	//if n -> ntohl
	if (val == 'h') {
		pkt->size = htonl(pkt->size);
		pkt->serial = htonl(pkt->serial);
		pkt->timestamp_sec = htonl(pkt->timestamp_sec);
		pkt->timestamp_nsec = htonl(pkt->timestamp_nsec);
	}
	else if (val == 'n') {
		pkt->size = ntohl(pkt->size);
		pkt->serial = ntohl(pkt->serial);
		pkt->timestamp_sec = ntohl(pkt->timestamp_sec);
		pkt->timestamp_nsec = ntohl(pkt->timestamp_nsec);
	}
}

int proto_send_packet(int fd, XACTO_PACKET *pkt, void *data) {
	debug("inside send\n");
	int payload_size = pkt->size;
	convert(pkt, 'h');
	if (pkt->type == XACTO_NO_PKT) {
		return -1;
	}
	if (rio_writen(fd, pkt, sizeof(XACTO_PACKET)) != sizeof(XACTO_PACKET)) {
		//errno is set accordingly in rio_writen
		return -1;
	}
	if(payload_size != 0 || pkt->null != 0 || data != NULL) { //if there is payload
        if(rio_writen(fd,data,payload_size) != payload_size){
            return -1;
        }
    }
	return 0;
}

int proto_recv_packet(int fd, XACTO_PACKET *pkt, void **datap) {
	debug("inside recieve\n");
	int payload_size = pkt->size;
	convert(pkt, 'n');
	if (pkt->type == XACTO_NO_PKT) {
		return -1;
	}
	if (rio_readn(fd, pkt, sizeof(XACTO_PACKET)) != sizeof(XACTO_PACKET)) {
		//errno is set accordingly in rio_readn
		return -1;
	}
	if(payload_size != 0 || datap != NULL) { //if there is payload
		char *data = Malloc(payload_size);
		*datap = data;
        if(rio_readn(fd,data,payload_size) != payload_size){
        	free(data);
            return -1;
        }
    }
	return 0;
}