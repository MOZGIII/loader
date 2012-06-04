#ifndef TRANSFER_INFO_H
#define TRANSFER_INFO_H

typedef struct transfer_info_rec {
	char * real_file_name;
	int real_file_name_length;
	long long chunk_size;
} transfer_info_rec;

#endif
