#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "recvline.h"

#define CONNECTION_REQUEST_QUEUE_BACKLOG_SIZE 10
#define MAX_STRING_SIZE 512

typedef struct server_rec {
	int socket;
	int port; // the port used to listen
} server_rec;

typedef struct header_info_rec {
	char origin[MAX_STRING_SIZE];
	
} header_info_rec;

server_rec server;


void set_reuseaddress(int socket, int value) {
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof value);
}

void init_structures() {
	// nothing here yet
}

void init_listening_socket() {
	struct sockaddr_in server_address;
	
	if((server.socket = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	set_reuseaddress(server.socket, 1);
		
	server_address.sin_family = PF_INET;
	server_address.sin_port = htons(server.port);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(server.socket, (struct sockaddr *)(&server_address), sizeof server_address) == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
	listen(server.socket, CONNECTION_REQUEST_QUEUE_BACKLOG_SIZE);
	
	printf("Listening on 0.0.0.0:%d\n", server.port);
}

void handle_new_connection(int client_socket, struct sockaddr_in client_address) {
	char *buff;
	size_t len;
	header_info_rec headers;
	int in_handshake = 1;
	
	#define is_header(string) (strncasecmp(buff, (string), sizeof (string)) == 0)
	#define copy_header(where) strncpy((where), strstr(buff, ": "), sizeof (where));
		
	// Websocket handshake here
	
	while(in_handshake) {
		if(!recvline(client_socket, (unsigned char**)&buff, &len)) {
			printf("Client disconnected in handshake!\n");
			exit(EXIT_SUCCESS);
		}
	
		if(len == 0) {
			in_handshake = 0;
			free(buff);
			break;
		}
		
		printf("%s\n", buff);
		if(is_header("Origin: ")) {
			printf(":: %s\n", buff);
			copy_header(headers.origin);
		}
		
		free(buff);
	}
	
	printf("Hi, websocket!\n");
	printf("Origin: %s\n", headers.origin);
}

void run_server_loop() {
	int client_socket;
	struct sockaddr_in client_address;
	socklen_t client_socklen;
	int forkpid;

	while(1){
		if((client_socket = accept(server.socket, (struct sockaddr *)(&client_address), &client_socklen)) == -1) {
			perror("accept");
			exit(1);
		}
		printf("Client %s:%d connected on socket %d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), client_socket);
		
		// do some fork();
		if((forkpid = fork()) < 0) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if(forkpid == 0) {
			close(server.socket);
			handle_new_connection(client_socket, client_address);
			shutdown(client_socket, SHUT_WR);
			close(client_socket);
			exit(EXIT_SUCCESS);
		}
		close(client_socket);
	}
}

int main(int argc, char **argv){
	if(argc != 2) {
		printf("Usage: %s port\n", argv[0]);
		printf("Creates a server listening at port\n");
		exit(EXIT_FAILURE);
	}
	
	init_structures();
	
	server.port = atoi(argv[1]);
	
	init_listening_socket();
	
	run_server_loop();

	return 0;
}
