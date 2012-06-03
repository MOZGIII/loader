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

#include "upload_info.h"
#include "utils.h"

upload_info_rec * uinfo_create_from_wsmsg(char * msg, int msg_length) {
	char tmp_str[msg_length]; // using new C syntax
	char * tmp[4]; // temp array for 4 pieces of data
	upload_info_rec * upload_info;

	memcpy(tmp_str, msg, msg_length);	
	
	allocate(upload_info, sizeof upload_info);
	
	tmp[0] = strtok(tmp_str, STRINGS_DELIMITER); // INFO
	tmp[1] = strtok(NULL,    STRINGS_DELIMITER); // File name
	tmp[2] = strtok(NULL,    STRINGS_DELIMITER); // File size
	tmp[3] = strtok(NULL,    STRINGS_DELIMITER); // Chunk size

	allocate(upload_info, sizeof upload_info);
	upload_info->file_name_length = strlen(tmp[1]);
	allocate(upload_info->file_name, upload_info->file_name_length);
	memcpy(upload_info->file_name, tmp[1], upload_info->file_name_length);
	
	upload_info->file_size = atoll(tmp[2]);
	upload_info->chunk_size = atoll(tmp[3]);
	
	upload_info->total_chunks = ((upload_info->file_size + upload_info->chunk_size - 1) / upload_info->chunk_size);
	return upload_info;
}
