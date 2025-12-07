#ifndef FILES_H
#define FILES_H

#include <sys/types.h>

int read_file(const char* filepath, char** buffer, ssize_t* file_size);
int serve_file(int client_socket, const char* filepath, int worker_id);
void send_http_error(int client_socket, int status_code, const char* message, int worker_id);

#endif
