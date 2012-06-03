#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Create server socket, listen on *service*.  This function returns
 * file descriptor of server socket if it succeeds, or returns -1.
 */
int create_listen_socket(const char *service) {
	struct addrinfo hints, *res, *rp;
	int sfd = -1;
	int r;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	r = getaddrinfo(0, service, &hints, &res);
	if(r != 0) {
		fprintf(stderr, "getaddrinfo: %s", gai_strerror(r));
		return -1;
	}
	for(rp = res; rp; rp = rp->ai_next) {
		int val = 1;
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sfd == -1) {
			continue;
		}
		if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &val, (socklen_t)sizeof(val)) == -1) {
			continue;
		}
		if(bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	freeaddrinfo(res);
	if(listen(sfd, 16) == -1) {
		perror("listen");
		close(sfd);
		return -1;
	}
	return sfd;
}

/*
 * Makes file descriptor *fd* non-blocking mode.
 * This function returns 0, or returns -1.
 */
int make_non_block(int fd) {
	int flags, r;
	while((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR);
	if(flags == -1) {
		perror("fcntl");
		return -1;
	}
	while((r = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR);
	if(r == -1) {
		perror("fcntl");
		return -1;
	}
	return 0;
}
