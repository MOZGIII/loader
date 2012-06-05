#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "recvline.h"

int recvline(int socket, char **buffer, size_t *len) {
	char c;
	size_t buffsize = 32;
	int recvlen;
	
	*buffer = NULL;
	*len = 0;
	
	if((*buffer = malloc(buffsize)) == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	
	while(1) {
		if((recvlen = recv(socket, &c, 1, MSG_WAITALL)) != 1) {
			if(recvlen == 0) return 0;
			perror("recv");
			exit(EXIT_FAILURE);
		}
		
		(*buffer)[*len] = c;
		(*len) += 1;
		
		if(*len >= 2 && (*buffer)[*len - 2] == '\r' && (*buffer)[*len - 1] == '\n') {
			break;
		}
		
		if(*len >= buffsize) {
			buffsize += buffsize >> 1;
			if((*buffer = realloc(*buffer, buffsize)) == NULL) {
				perror("realloc");
				exit(EXIT_FAILURE);
			}
		}
	}

	// cut the string
	*len -= 2;
	(*buffer)[*len] = '\0';
	
	return 1;
}
   
