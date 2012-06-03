#ifndef DBSERVER_H
#define DBSERVER_H

#define DBSERVER_HOST "127.0.0.1"
#define DBSERVER_PORT 3713

int connect_to_dbserver();
uint32_t db_register_session(int fd, upload_info_rec * upload_info);
char * db_get_session_upload_path(int fd, uint32_t session_id);

#endif
