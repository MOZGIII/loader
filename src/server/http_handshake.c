#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <wslay/wslay.h>

/*
 * Calculates SHA-1 hash of *src*. The size of *src* is *src_length* bytes.
 * *dst* must be at least SHA1_DIGEST_SIZE.
 */
void sha1(uint8_t *dst, const uint8_t *src, size_t src_length) {
	SHA1((unsigned char *) src, (unsigned long) src_length, (unsigned char *) dst);
}

/* 
 * Calculates Base64 of *src*. The result is a string allocated using malloc.
 * You must free the result manually after you use it.
 */
char *base64(const unsigned char *input, int length) {
	BIO *bmem, *b64;
	BUF_MEM *bptr;
	char *buff;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	if((buff = malloc(bptr->length)) == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	memcpy(buff, bptr->data, bptr->length-1);
	buff[bptr->length-1] = 0;

	BIO_free_all(b64);

	return buff;
}

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/*
 * Create Server's accept key in *dst*.
 * *client_key* is the value of |Sec-WebSocket-Key| header field in
 * client's handshake and it must be length of 24.
 */
char * create_accept_key(const char *client_key) {
	uint8_t sha1buf[20], key_src[60];
	memcpy(key_src, client_key, 24);
	memcpy(key_src+24, WS_GUID, 36);
	sha1(sha1buf, key_src, sizeof(key_src));
	return base64(sha1buf, 20);
}

/* We parse HTTP header lines of the format
 *   \r\nfield_name: value1, value2, ... \r\n
 *
 * If the caller is looking for a specific value, we return a pointer to the
 * start of that value, else we simply return the start of values list.
 */
static char* http_header_find_field_value(char *header, char *field_name, char *value) {
	char *header_end,
		 *field_start,
		 *field_end,
		 *next_crlf,
		 *value_start;
	int field_name_len;

	/* Pointer to the last character in the header */
	header_end = header + strlen(header) - 1;

	field_name_len = strlen(field_name);

	field_start = header;

	do{
		field_start = strstr(field_start+1, field_name);

		field_end = field_start + field_name_len - 1;

		if(field_start != NULL
			 && field_start - header >= 2
			 && field_start[-2] == '\r'
			 && field_start[-1] == '\n'
			 && header_end - field_end >= 1
			 && field_end[1] == ':')
		{
			break; /* Found the field */
		}
		else
		{
			continue; /* This is not the one; keep looking. */
		}
	} while(field_start != NULL);

	if(field_start == NULL)
		return NULL;

	/* Find the field terminator */
	next_crlf = strstr(field_start, "\r\n");

	/* A field is expected to end with \r\n */
	if(next_crlf == NULL)
		return NULL; /* Malformed HTTP header! */

	/* If not looking for a value, then return a pointer to the start of values string */
	if(value == NULL)
		return field_end+2;

	value_start = strstr(field_start, value);

	/* Value not found */
	if(value_start == NULL)
		return NULL;

	/* Found the value we're looking for */
	if(value_start > next_crlf)
		return NULL; /* ... but after the CRLF terminator of the field. */

	/* The value we found should be properly delineated from the other tokens */
	if(isalnum(value_start[-1]) || isalnum(value_start[strlen(value)]))
		return NULL;

	return value_start;
}

/*
 * Performs HTTP handshake. *fd* is the file descriptor of the
 * connection to the client. This function returns 0 if it succeeds,
 * or returns -1.
 */
int http_handshake(int fd) {
	char header[16384], *accept_key, *keyhdstart, *keyhdend, res_header[256];
	size_t header_length = 0, res_header_sent = 0, res_header_length;
	ssize_t r;
	while(1) {
		while((r = read(fd, header+header_length,
							      sizeof(header)-header_length)) == -1 && errno == EINTR);
		if(r == -1) {
			perror("read");
			return -1;
		} else if(r == 0) {
			fprintf(stderr, "HTTP Handshake: Got EOF\n");
			return -1;
		} else {
			header_length += r;
			if(header_length >= 4 &&
				 memcmp(header+header_length-4, "\r\n\r\n", 4) == 0) {
				break;
			} else if(header_length == sizeof(header)) {
				fprintf(stderr, "HTTP Handshake: Too large HTTP headers\n");
				return -1;
			}
		}
	}

	if(http_header_find_field_value(header, "Upgrade", "websocket") == NULL ||
		 http_header_find_field_value(header, "Connection", "Upgrade") == NULL ||
		 (keyhdstart = http_header_find_field_value(header, "Sec-WebSocket-Key", NULL)) == NULL) {
		fprintf(stderr, "HTTP Handshake: Missing required header fields\n");
		return -1;
	}
	for(; *keyhdstart == ' '; ++keyhdstart);
	keyhdend = keyhdstart;
	for(; *keyhdend != '\r' && *keyhdend != ' '; ++keyhdend);
	if(keyhdend-keyhdstart != 24) {
		printf("%s\n", keyhdstart);
		fprintf(stderr, "HTTP Handshake: Invalid value in Sec-WebSocket-Key\n");
		return -1;
	}
	accept_key = create_accept_key(keyhdstart);
	snprintf(res_header, sizeof(res_header),
					 "HTTP/1.1 101 Switching Protocols\r\n"
					 "Upgrade: websocket\r\n"
					 "Connection: Upgrade\r\n"
					 "Sec-WebSocket-Accept: %s\r\n"
					 "\r\n", accept_key);
	free(accept_key);
	res_header_length = strlen(res_header);
	while(res_header_sent < res_header_length) {
		while((r = write(fd, res_header+res_header_sent, res_header_length-res_header_sent)) == -1 && errno == EINTR);
		if(r == -1) {
			perror("write");
			return -1;
		} else {
			res_header_sent += r;
		}
	}
	return 0;
}