#ifndef UTILS_H
#define UTILS_H

int make_non_block(int fd);
int create_listen_socket(const char *service);


// Helpers

#define log(...) fprintf(stderr, __VA_ARGS__)
#define min(a, b) (a < b ? a : b)
#define allocate(dst, bytes) do { if((dst = malloc(bytes)) == NULL){ perror("allocate"); exit(3); } } while(0)


#endif
