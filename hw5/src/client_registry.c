#include <semaphore.h>

#include "client_registry.h"
#include "csapp.h"
#include "debug.h"

typedef struct client_registry {
	int fd_list[FD_SETSIZE];
	int total_num;
    sem_t lock;
    pthread_mutex_t mutex;
}CLIENT_REGISTRY;

CLIENT_REGISTRY *creg_init() {
	CLIENT_REGISTRY *new = Malloc(sizeof(CLIENT_REGISTRY));
	for (int i=0; i<FD_SETSIZE; i++) {
		new->fd_list[i] = 0;
	}
	new->total_num = 0;
	Sem_init(&(new->lock), 0, 0);
	pthread_mutex_init(&(new->mutex), NULL);
	debug("INTIALIZED CLIENT_REGISTRY");
	return new;
}

void creg_fini(CLIENT_REGISTRY *cr)
{
	debug("FINI!!");
    pthread_mutex_destroy(&(cr->mutex));
    sem_destroy(&(cr->lock));
    free(cr);
}

int creg_register(CLIENT_REGISTRY *cr, int fd) {
	debug("Registering: %d", fd);
	pthread_mutex_lock(&(cr->mutex));
	for (int i=0;i<FD_SETSIZE; i++){
		if (cr->fd_list[i] == 0) {
			cr->fd_list[i] = fd;
			pthread_mutex_unlock(&(cr->mutex));
			cr->total_num += 1;
			return 0;
		}
	}
	pthread_mutex_unlock(&(cr->mutex));
	return -1; //reached max
}

int creg_unregister(CLIENT_REGISTRY *cr, int fd) {
	debug("Unregistering: %d", fd);
	pthread_mutex_lock(&(cr->mutex));
	if (cr->total_num == 0) {
		pthread_mutex_unlock(&(cr->mutex));
		// sem_post(&(cr->lock));
		return -1;
	}
	for (int i=0;i<FD_SETSIZE; i++){
		if (cr->fd_list[i] == fd) {
			cr->fd_list[i] = 0;
			cr->total_num -= 1;
			break;
		}
	}
	pthread_mutex_unlock(&(cr->mutex));
	// if (cr->total_num == 0) {
	// 	sem_post(&(cr->lock));
	// }
	return 0;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
	debug("WAIITING FOR EMPTY!!");
	if (cr->total_num != 0) {
		sem_wait(&(cr->lock));
	}
	return;
}

void creg_shutdown_all(CLIENT_REGISTRY *cr) {
	debug("STARTING SHUTDOWN!!");
	pthread_mutex_lock(&(cr->mutex));
	if (cr->total_num > 0) {
		for (int i=0; i<FD_SETSIZE; i++) {
			if (cr->fd_list[i] != 0) {
				debug("shuttinig down %d", cr->fd_list[i]);
				shutdown(cr->fd_list[i], SHUT_RD);
			}
		}
	}
	pthread_mutex_unlock(&(cr->mutex));
	cr->total_num = 0;
	return;
}
