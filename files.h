#ifndef FILES_H
#define FILES_H

#include <sys/types.h>

// Function prototypes for file operations

/**
 * Read a file into a buffer
 */
int read_file(const char* filepath, char** buffer, ssize_t* file_size);

/**
 * Serve a file over a socket with HTTP headers
 */
int serve_file(int client_socket, const char* filepath, int worker_id);

/**
 * Send an HTTP error response
 */
void send_http_error(int client_socket, int status_code, const char* message, int worker_id);

#endif
