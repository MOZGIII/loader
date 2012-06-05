#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

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

#include <sys/stat.h> // for file operations

#include <wslay/wslay.h>

#include "http_handshake.h"
#include "utils.h"
#include "upload_info.h"
#include "transfer_info.h"
#include "dbserver.h"


// Protocol errors

#define PROTOCOL_ERROR_INTERNAL          1
#define PROTOCOL_ERROR_UNVALID_OPERATION 2


// Connection roles
 
#define ROLE_UNDEFINED   0  // for when role is not defined yet
#define ROLE_CONTROLLER  1  // provides session_id and useful info to client
#define ROLE_TRANSMITTER 2  // transfers all the data payload

// Connection states

#define STATE_INIT 0 // init state
#define STATE_SESSION_OPENED 1 // session opened
#define STATE_SESSION_CLOSED 2 // session closed
#define STATE_ACCEPTING_PAYLOAD 3 // ready to write into a file

// Types

struct Session {
	int fd;
	uint32_t session_id; // real session id
	
	int role;
	int state; // states matter only after role is defined
	
	int accepting_payload; // non-zero if we are ready to accept file data chunk
	
	upload_info_rec * upload_info; // initialized when session is opened in controller
	transfer_info_rec * transfer_info; // initialized when session is opened in controller and  transmitter
	
	int chunk_number; // used for tracking the number of currently uploading chunk in transmitter
};

int dbserver_socket;

ssize_t send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
	struct Session *session = (struct Session*)user_data;
	ssize_t r;
	int sflags = 0;
#ifdef MSG_MORE
	if(flags & WSLAY_MSG_MORE) {
		sflags |= MSG_MORE;
	}
#endif // MSG_MORE
	while((r = send(session->fd, data, len, sflags)) == -1 && errno == EINTR);
	if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	}
	return r;
}

ssize_t recv_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, int flags, void *user_data) {
	struct Session *session = (struct Session*)user_data;
	ssize_t r;
	while((r = recv(session->fd, buf, len, 0)) == -1 && errno == EINTR);
	if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	} else if(r == 0) {
		/* Unexpected EOF is also treated as an error */
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		r = -1;
	}
	return r;
}    

void close_with_reason(wslay_event_context_ptr ctx, uint16_t status, char * text) {
	wslay_event_queue_close(ctx, status, (unsigned char *) text, strlen(text));
	wslay_event_send(ctx);
}

void close_without_reason(wslay_event_context_ptr ctx, uint16_t status) {
	wslay_event_queue_close(ctx, status, NULL, 0);
	wslay_event_send(ctx);
}

void send_ok(wslay_event_context_ptr ctx) {
	const char * oktext = "ok";
	struct wslay_event_msg msgarg = {
		WSLAY_TEXT_FRAME, (unsigned char *)oktext, strlen(oktext)
	};
	wslay_event_queue_msg(ctx, &msgarg);
}

void on_msg_recv_callback(wslay_event_context_ptr ctx,
							const struct wslay_event_on_msg_recv_arg *arg,
							void *user_data) {
	struct Session * session = user_data;	
	
	/* Operate with non-control messages */
	if(!wslay_is_ctrl_frame(arg->opcode)) {
		// printf("%02X %d\n", arg->opcode, arg->msg_length);
		
		if(session->role == ROLE_UNDEFINED) {
			// Accept only text messages
			if(arg->opcode == WSLAY_TEXT_FRAME) {
				if(strncmp("CONTROLLER", (char *) arg->msg, arg->msg_length) == 0) {
					session->role = ROLE_CONTROLLER;
					session->state = STATE_INIT; // force init state
					send_ok(ctx);
				} else if(strncmp("TRANSMITTER", (char *) arg->msg, arg->msg_length) == 0) {
					session->role = ROLE_TRANSMITTER;
					session->state = STATE_INIT; // force init state
					send_ok(ctx);
				} else {
					close_with_reason(ctx, PROTOCOL_ERROR_UNVALID_OPERATION, "You must choose connection role at this point.");
				}
			} else {
				close_with_reason(ctx, PROTOCOL_ERROR_UNVALID_OPERATION, "You can only transfer text messages yet.");
			}
		} else if(session->role == ROLE_CONTROLLER) {
			if(session->state == STATE_INIT) {
				if(strncmp("INFO\n", (char *) arg->msg, min(arg->msg_length, 5)) == 0) { // payload check is really unsafe here, but I dont care right now....
					upload_info_rec * upload_info = uinfo_create_from_wsmsg((char *) arg->msg, arg->msg_length);
					
					log("Got file upload request, file %s [%lld bytes], %d chunks by %lld bytes\n", upload_info->file_name, upload_info->file_size, upload_info->total_chunks, upload_info->chunk_size);
					session->upload_info = upload_info;
					
					// Here we should communicate to external server (database?) and request confirmation, session_id, etc...
					session->session_id = db_register_session(dbserver_socket, upload_info);
					
					// If ok
					if(session->session_id > 0) {
					
						session->transfer_info = db_get_transfer_info(dbserver_socket, session->session_id);
						if(session->transfer_info) {
						
							if(unlink(session->transfer_info->real_file_name) == 0 || errno == ENOENT) {
															
								char session_id[32];
								snprintf(session_id, sizeof session_id, "%d", session->session_id);
								struct wslay_event_msg msgarg = {
									WSLAY_TEXT_FRAME, (unsigned char *) session_id, strlen(session_id)
								};
								wslay_event_queue_msg(ctx, &msgarg);
								
								session->state = STATE_SESSION_OPENED;
							}
						}
					}
				}
				
				if(session->state == STATE_INIT)
					close_without_reason(ctx, PROTOCOL_ERROR_INTERNAL);
			} else {
				close_without_reason(ctx, PROTOCOL_ERROR_INTERNAL);
			}
		} else if(session->role == ROLE_TRANSMITTER) {
			if(session->state == STATE_INIT) {
				if(arg->msg_length <= 5*1024) { // block overload
					char tmp[arg->msg_length + 1];
					memcpy(tmp, arg->msg, arg->msg_length);
					tmp[arg->msg_length] = '\0';
					
					session->session_id = atoi(tmp);
					if(session->session_id > 0) {
						
						session->transfer_info = db_get_transfer_info(dbserver_socket, session->session_id);
						if(session->transfer_info) {					
							send_ok(ctx);
							session->state = STATE_SESSION_OPENED;						
						}
					}
				}				
				
				if(session->state == STATE_INIT)
					close_without_reason(ctx, PROTOCOL_ERROR_INTERNAL);
			} else if(session->state == STATE_SESSION_OPENED) {
			
				if(arg->msg_length <= 5*1024) { // block overload
					char tmp[arg->msg_length + 1];
					memcpy(tmp, arg->msg, arg->msg_length);
					tmp[arg->msg_length] = '\0';
					session->chunk_number = atoi(tmp);
					
					// ask for acceptence, ill just skip it for now
					
					send_ok(ctx);
					session->state = STATE_ACCEPTING_PAYLOAD;
				}
				
				if(session->state == STATE_SESSION_OPENED)
					close_without_reason(ctx, PROTOCOL_ERROR_INTERNAL);
			} else if(session->state == STATE_ACCEPTING_PAYLOAD) {
				
				// writing to a file
				int target;
				long long offset = session->transfer_info->chunk_size * session->chunk_number;

				if((target = open(session->transfer_info->real_file_name,
					O_WRONLY | O_APPEND | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO)) > 0) {
					
					log("Writing chunk #%d (offset %lld) to a file %s\n", session->chunk_number, offset, session->transfer_info->real_file_name);
					
					//log("File offset set to %lld...\n", lseek64(target, 0, SEEK_CUR));
					lseek64(target, offset, SEEK_SET);
					//log("... after seek %lld...\n", lseek64(target, 0, SEEK_CUR));
					write(target, arg->msg, arg->msg_length);
					//log("... and to %lld after write!\n", lseek64(target, 0, SEEK_CUR));
					close(target);
					
					send_ok(ctx);
					session->state = STATE_SESSION_OPENED;
				
				} else {
					log("Unable to open the destanation file %s\n", session->transfer_info->real_file_name);
					close_without_reason(ctx, PROTOCOL_ERROR_INTERNAL);					
				}
				
				if(session->state == STATE_ACCEPTING_PAYLOAD)
					close_without_reason(ctx, PROTOCOL_ERROR_INTERNAL);
			} else {
				close_without_reason(ctx, PROTOCOL_ERROR_INTERNAL);
			}			
		}
	}
}

/*
 * Communicate with the client. This function performs HTTP handshake
 * and WebSocket data transfer until close handshake is done or an
 * error occurs. *fd* is the file descriptor of the connection to the
 * client. This function returns 0 if it succeeds, or returns 0.
 */
int communicate(int fd) {
	wslay_event_context_ptr ctx;
	struct wslay_event_callbacks callbacks = {
		recv_callback,
		send_callback,
		NULL,
		NULL,
		NULL,
		NULL,
		on_msg_recv_callback
	};
	struct Session session;
	int val = 1;
	struct pollfd event;
	int res = 0;
	
	// Session initialization
	session.fd = fd;
	session.session_id = 0;
	session.role = ROLE_UNDEFINED;
	session.state = STATE_INIT;
	session.upload_info = NULL;
	session.transfer_info = NULL;
	
	if(http_handshake(fd) == -1) {
		return -1;
	}
	if(make_non_block(fd) == -1) {
		return -1;
	}
	if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, (socklen_t)sizeof(val))
		 == -1) {
		perror("setsockopt: TCP_NODELAY");
		return -1;
	}
	
	// Connect to database
	dbserver_socket = connect_to_dbserver();
	
	memset(&event, 0, sizeof(struct pollfd));
	event.fd = fd;
	event.events = POLLIN;
	wslay_event_context_server_init(&ctx, &callbacks, &session);
	/*
	 * Event loop: basically loop until both wslay_event_want_read(ctx)
	 * and wslay_event_want_write(ctx) return 0.
	 */
	while(wslay_event_want_read(ctx) || wslay_event_want_write(ctx)) {
		int r;
		while((r = poll(&event, 1, -1)) == -1 && errno == EINTR);
		if(r == -1) {
			perror("poll");
			res = -1;
			break;
		}
		if(((event.revents & POLLIN) && wslay_event_recv(ctx) != 0) ||
			 ((event.revents & POLLOUT) && wslay_event_send(ctx) != 0) ||
			 (event.revents & (POLLERR | POLLHUP | POLLNVAL))) {
			/*
			 * If either wslay_event_recv() or wslay_event_send() return
			 * non-zero value, it means serious error which prevents wslay
			 * library from processing further data, so WebSocket connection
			 * must be closed.
			 */
			res = -1;
			break;
		}
		event.events = 0;
		if(wslay_event_want_read(ctx)) {
			event.events |= POLLIN;
		}
		if(wslay_event_want_write(ctx)) {
			event.events |= POLLOUT;
		}
	}
	return res;
}

/*
 *  *sfd* is the file descriptor of
 * the server socket.  when the incoming connection from the client is
 * accepted, this function forks another process and the forked
 * process communicates with client. The parent process goes back to
 * the loop and can accept another client.
 */
void serve(int sfd) {
	while(1) {
		int fd;
		while((fd = accept(sfd, NULL, NULL)) == -1 && errno == EINTR);
		if(fd == -1) {
			perror("accept");
		} else {
			int r = fork();
			if(r == -1) {
				perror("fork");
				close(fd);
			} else if(r == 0) {
				int r = communicate(fd);
				shutdown(fd, SHUT_WR);
				close(fd);
				if(r == 0) {
					exit(EXIT_SUCCESS);
				} else {
					exit(EXIT_FAILURE);
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
	struct sigaction act;
	int sfd;
	if(argc < 2) {
		fprintf(stderr, "Usage: %s PORT\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);

	sfd = create_listen_socket(argv[1]);
	if(sfd == -1) {
		fprintf(stderr, "Failed to create server socket\n");
		exit(EXIT_FAILURE);
	}
	printf("WebSocket file loader server, listening on %s\n", argv[1]);
	serve(sfd);
	return EXIT_SUCCESS;
}
