#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include "node_states.h"

void exit_on_error_custom(const char *title, const char *detail) {
	fprintf(stderr, "%s:%s\n", title, detail);
	exit(1);
}

int main(int argc, char *argv[]) {

    states currentState =  Q1;

	if(argc != 3)
		exit_on_error_custom("Parameters"," <tracker address> <tracker port>");
	
	char *trackerIp = (char*)malloc((strlen(argv[1])+1)*sizeof(char));
	int trackerPort = atoi(argv[2]);
	
	strcpy(trackerIp, argv[1]);
	
	printf("Connecting to tracker: [%s:%d]\n", trackerIp, trackerPort);

    node n = {};

    n.addr = calloc(1, sizeof(struct sockaddr_in));
    n.predecessor = calloc(1, sizeof(struct sockaddr_in));
    n.predecessor->sin_addr.s_addr = 0;
    n.successor = calloc(1, sizeof(struct sockaddr_in));
    n.successor->sin_addr.s_addr = 0;
    n.listeningPort = calloc(1, sizeof(int));

    n.tracker.sin_family = AF_INET;
    int result = inet_pton(AF_INET, trackerIp, &(n.tracker.sin_addr));
    if(result < 1) {
        perror("inet_pton");
    }
    n.tracker.sin_port = htons(trackerPort);

    n.sockets = calloc(4, sizeof(*n.sockets));
    n.sockets[0] = (struct pollfd){-1, POLLIN}; //A
    n.sockets[1] = (struct pollfd){-1, POLLIN}; //B
    n.sockets[2] = (struct pollfd){-1, POLLIN}; //C
    n.sockets[3] = (struct pollfd){-1, POLLIN}; //D

    n.socketBuffers = calloc(4, sizeof(socket_buffer));

    for (int i = 0; i < 4; ++i) {
        n.socketBuffers[i].buffer = calloc(BUFF_SIZE, sizeof(char));
    }

    state* stateMachine = node_states_get_state_machine();

    while(currentState != EXIT) {
        currentState = stateMachine[currentState].handler(&n);
    }

    free(trackerIp);
    free(n.addr);
    free(n.predecessor);
    free(n.successor);
    free(n.listeningPort);
    free(n.sockets);
    for (int i = 0; i < 4; ++i) {
        free(n.socketBuffers[i].buffer);
    }
    free(n.socketBuffers);

    if (n.table){
        hash_table_destroy(n.table);
    }

    if(n.lastPdu) {
        free(n.lastPdu);
    }
}