#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "upload_info.h"
#include "transfer_info.h"
#include "dbserver.h"
#include "utils.h"

int connect_to_dbserver() {
	/*
	struct sockaddr_in address;
	uint16_t port = DBSERVER_PORT;
	int sock;
	struct hostent * host;
	
	if((host = gethostbyname(DBSERVER_HOST)) == NULL) {
		fprintf(stderr, "Wrong db host %s!\n", DBSERVER_HOST);
		exit(EXIT_FAILURE);
	}

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	bzero((char *)&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	bcopy((char *)host->h_addr, (char *)&address.sin_addr.s_addr, host->h_length);

	if(connect(sock, (struct sockaddr *) &address, sizeof(address))) {
		perror("connect");
		exit(EXIT_FAILURE);
	}
	
	return sock;
	*/
	return 5; // FTW!
}

uint32_t db_register_session(int fd, upload_info_rec * upload_info) {
	return 2;
}

transfer_info_rec * db_get_transfer_info(int fd, uint32_t session_id) {
	transfer_info_rec * val;	
	allocate(val, sizeof(transfer_info_rec));
	
	char tmp[] = "testfile.dat";
	
	val->real_file_name_length = strlen(tmp);
	allocate(val->real_file_name, val->real_file_name_length + 1);
	
	strcpy(val->real_file_name, tmp);
	val->real_file_name[val->real_file_name_length] = '\0';
	
	val->chunk_size = 1024*1024*5; // chunk size for testing
	
	return val;
}

