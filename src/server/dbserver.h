#ifndef DBSERVER_H
#define DBSERVER_H

#define DBSERVER_HOST "127.0.0.1"
#define DBSERVER_PORT 3713

int connect_to_dbserver();
uint32_t db_register_session(int fd, upload_info_rec * upload_info);
transfer_info_rec * db_get_transfer_info(int fd, uint32_t session_id);

#endif
