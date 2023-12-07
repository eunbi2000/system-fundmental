#include "protocol.h"
#include "csapp.h"
#include "debug.h"

int proto_send_packet(int fd, XACTO_PACKET *pkt, void *data) {
	debug("inside send");
	uint32_t payload_size = htonl(pkt->size);

	if (rio_writen(fd, pkt, sizeof(XACTO_PACKET)) != sizeof(XACTO_PACKET)) {
		//errno is set accordingly in rio_writen
		return -1;
	}
	if(payload_size > 0) { //if there is payload
        if(rio_writen(fd,data,payload_size) != payload_size){
            return -1;
        }
    }
	return 0;
}

int proto_recv_packet(int fd, XACTO_PACKET *pkt, void **datap) {
	debug("inside receive");

	if (rio_readn(fd, pkt, sizeof(XACTO_PACKET)) != sizeof(XACTO_PACKET)) {
		//errno is set accordingly in rio_readn
		return -1;
	}

	uint32_t payload_size = ntohl(pkt->size);
	// debug("payload_size: %d", payload_size);

	if(payload_size > 0) { //if there is payload
		void *data = Malloc(payload_size+1);
        if(rio_readn(fd,data,payload_size) != payload_size){
        	free(data);
            return -1;
        }
        *datap = data;
    }
	return 0;
}