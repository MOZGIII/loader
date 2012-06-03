#ifndef UPLOAD_INFO_H
#define UPLOAD_INFO_H

#define STRINGS_DELIMITER "\n"

typedef struct upload_info_rec {
	char * file_name;
	int file_name_length;
	long long file_size;
	long long chunk_size;
	int total_chunks;
} upload_info_rec;

upload_info_rec * uinfo_create_from_wsmsg(char * msg, int msg_length);

#endif
