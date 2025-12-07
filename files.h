/*
 * files.h - File operations and HTTP response handling
 *
 * Declares functions for reading files from disk and sending HTTP responses
 * to clients.
 *
 * Key functions:
 *   - read_file(): Load file contents into memory
 *   - serve_file(): Send file with HTTP 200 response
 *   - send_http_error(): Send HTTP error response (404, 500, etc.)
 */

#ifndef FILES_H
#define FILES_H

#include <sys/types.h>

int read_file(const char* filepath, char** buffer, ssize_t* file_size);
int serve_file(int client_socket, const char* filepath, int worker_id);
void send_http_error(int client_socket, int status_code, const char* message, int worker_id);

#endif
