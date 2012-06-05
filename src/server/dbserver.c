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
#include "recvline.h"

int connect_to_dbserver() {
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
}

int send_message(int socket, char ** lines, int lines_length) {
	int i;
	int j;
	char crlf[] = "\r\n";	
	for(i = 0; i < lines_length; i++) {		
		if((j = send(socket, lines[i], strlen(lines[i]), 0)) == -1) {
			perror("send-db1");
			return 0;
		}		
		if((j = send(socket, crlf, strlen(crlf), 0)) == -1) {
			perror("send-db2");
			return 0;
		}
	}	
	if((j = send(socket, crlf, strlen(crlf), 0)) == -1) {
		perror("send-db3");
		return 0;
	}
	
	return 1;
}

uint32_t db_register_session(int fd, upload_info_rec * upload_info) {
	char * lines[4];
	
	char tmp1[64];
	snprintf(tmp1, sizeof tmp1, "%lld", upload_info->file_size);
	
	char tmp2[64];
	snprintf(tmp2, sizeof tmp2, "%lld", upload_info->chunk_size);
	
	lines[0] = "UPLOAD";
	lines[1] = upload_info->file_name;
	lines[2] = tmp1;
	lines[3] = tmp2;
	
	if(!send_message(fd, lines, sizeof lines / sizeof *lines)) {
		log("Could not send message to a database!\n");
		perror("send");
		return 0;
	}
	
	//log("Request to db sent!\n");
	
	char * line;
	size_t len;
	
	if(!recvline(fd, &line, &len) || strncmp("ACCEPTED", line, len) != 0) {
		log("Unable to get valid response header from db server!\n");
		if(line) free(line);
		return 1;
	}
	free(line);	
	
	int session_id;
	if(!recvline(fd, &line, &len)) {
		log("Unable to get session id from the control db\n");
		free(line);
		return 1;
	}
	session_id = atoi(line);
	free(line);
	
	if(!recvline(fd, &line, &len) || len != 0) {
		log("Incomplete packet!\n");
		free(line);
		return 1;
	}
	free(line);
	
	return session_id;
}

transfer_info_rec * db_get_transfer_info(int fd, uint32_t session_id) {
	char * lines[2];
	
	char tmp1[64];
	snprintf(tmp1, sizeof tmp1, "%d", session_id);
		
	lines[0] = "UPINFO";
	lines[1] = tmp1;
	
	if(!send_message(fd, lines, sizeof lines / sizeof *lines)) {
		log("Could not send message to a database!\n");
		perror("send");
		return 0;
	}
	
	transfer_info_rec * val;	
	allocate(val, sizeof(transfer_info_rec));
	
	char * line;
	size_t len;
	
	if(!recvline(fd, &line, &len) || strncmp("OK", line, len) != 0) {
		log("Unable to get valid response header from db server!\n");
		if(line) free(line);
		return NULL;
	}
	free(line);		
	
	if(!recvline(fd, &line, &len)) {
		log("Unable to get session id from the control db\n");
		free(line);
		return NULL;
	}
	val->chunk_size = atoll(line);
	free(line);
	
	if(!recvline(fd, &val->real_file_name, (unsigned int *)&val->real_file_name_length)) {
		log("Unable to get real file name from the control db\n");
		free(val->real_file_name);
		val->real_file_name_length = 0;
		return NULL;
	}
	// no free here
		
	if(!recvline(fd, &line, &len) || len != 0) {
		log("Incomplete packet!\n");
		free(line);
		return NULL;
	}
	free(line);
		
	return val;
}

