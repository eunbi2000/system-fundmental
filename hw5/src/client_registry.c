#include <semaphore.h>

#include "client_registry.h"
#include "csapp.h"
#include "debug.h"

typedef struct client_registry {
	int fd_list[FD_SETSIZE];
	int total_num;
	int done;
    sem_t lock;
    pthread_mutex_t mutex;
}CLIENT_REGISTRY;

CLIENT_REGISTRY *creg_init() {
	CLIENT_REGISTRY *new = Malloc(sizeof(CLIENT_REGISTRY));
	for (int i=0; i<FD_SETSIZE; i++) {
		new->fd_list[i] = 0;
	}
	new->total_num = 0;
	new->done = 0;
	Sem_init(&(new->lock), 0, 0);
	pthread_mutex_init(&(new->mutex), NULL);
	debug("Initialize client_registry");
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
	debug("Registering: %d (total connected: %d)", fd, cr->total_num+1);
	pthread_mutex_lock(&(cr->mutex));
	for (int i=0;i<FD_SETSIZE; i++){
		if (cr->fd_list[i] == 0) {
			cr->fd_list[i] = fd;
			cr->total_num += 1;
			pthread_mutex_unlock(&(cr->mutex));
			// debug("total_num is %d",cr->total_num);
			return 0;
		}
	}
	pthread_mutex_unlock(&(cr->mutex));
	return -1; //reached max
}

int creg_unregister(CLIENT_REGISTRY *cr, int fd) {
	debug("Unregistering: %d", fd);
	pthread_mutex_lock(&(cr->mutex));
	for (int i=0;i<FD_SETSIZE; i++){
		if (cr->fd_list[i] == fd) {
			cr->fd_list[i] = 0;
			cr->total_num -= 1;
			break;
		}
	}
	if (cr->total_num == 0 && cr->done == 1) {
		cr->done = 1;
		sem_post(&(cr->lock));
		exit(0);
	}
	pthread_mutex_unlock(&(cr->mutex));
	return 0;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
	debug("WAIITING FOR EMPTY!!");
	if (cr->total_num != 0 && cr->done != 1) {
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
				debug("shutting down %d", cr->fd_list[i]);
				shutdown(cr->fd_list[i], SHUT_RD);
			}
		}
	}
	pthread_mutex_unlock(&(cr->mutex));
	cr->total_num = 0;
	cr->done = 1;
	return;
}
