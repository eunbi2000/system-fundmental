#include "debug.h"
#include "client_registry.h"
#include "transaction.h"
#include "store.h"
#include "server.h"
#include "csapp.h"

#include <string.h>

static void terminate(int status);
void sigHandler(int sig, siginfo_t *info, void* context);

CLIENT_REGISTRY *client_registry;
int listenfd;
int *connfd;

// void *client_test(void *arg) {
//     int num = (intptr_t) arg;
//     if (creg_register(client_registry, num) != 0) {
//         debug("failed register");
//     }
//     sleep(1);
//     if (creg_unregister(client_registry, num) != 0) {
//         debug("failed unregister");
//     }
//     debug("!!!SUCCESS!!!");
//     pthread_exit(NULL);
//     return NULL;
// }

int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    struct sigaction action = {0};
    action.sa_sigaction = sigHandler;
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &action, NULL);

    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char *port;
    // char *hostname;
    int hide = 0;
    if (argc < 3) {
        return EXIT_FAILURE;
    }
    int iter = 1;
    while (iter < argc) {
        if (strcmp(argv[iter], "-p")==0) {
            if (iter+1 == argc) {
                return EXIT_FAILURE;
            }
            port = argv[iter+1];
            iter++;
        }
        else if (strcmp(argv[iter], "-h")==0) {
            if (iter+1 == argc) {
                return EXIT_FAILURE;
            }
            // hostname = argv[iter+1];
            iter++;
        }
        else if (strcmp(argv[iter], "-q")==0) {
            hide = 1;
        }
        iter++;
    }
    printf("port: %s, hide: %d \n", port, hide);

    // Perform required initializations of the client_registry,
    // transaction manager, and object store.
    client_registry = creg_init();
    trans_init();
    store_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function xacto_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    listenfd = Open_listenfd(port);
    // pthread_t threads[10];
    pthread_t tid;
    debug("start");
    while (1) {
        debug("inside");
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        // for (int i =0; i<10; i++) {
        //     Pthread_create(&threads[i], NULL, client_test, (void *)(intptr_t)i);
        // }
        Pthread_create(&tid, NULL, xacto_client_service, connfd);
        sleep(10);
        pthread_kill(tid, SIGHUP);
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the Xacto server will function.\n");

    terminate(EXIT_FAILURE);
}

void sigHandler(int sig, siginfo_t *info, void* context){
    if (sig == SIGHUP) {
        terminate(EXIT_SUCCESS);
    }
}
/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    trans_fini();
    store_fini();

    //get rid of malloc stuff and close
    // shutdown(listenfd,SHUT_RD);
    // close(listenfd);
    // free(connfd);

    debug("Xacto server terminating");
    exit(status);
}
