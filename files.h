/**
 * @file files.h
 * @brief File operations and HTTP response handling
 *
 * Declares functions for reading files from disk and sending HTTP responses
 * to clients.
 */

#ifndef FILES_H
#define FILES_H

#include <sys/types.h>

/**
 * @brief Read entire file into buffer
 * @param filepath Path to the file to read
 * @param buffer Output pointer to allocated buffer (caller must free)
 * @param file_size Output pointer to store bytes read
 * @return 0 on success, -1 if file not found or read error
 */
int read_file(const char* filepath, char** buffer, ssize_t* file_size);

/**
 * @brief Serve a file over HTTP
 * @param client_socket The client socket file descriptor
 * @param filepath Path to the file to serve
 * @param worker_id The worker thread ID for logging
 * @return 0 on success, -1 on error
 */
int serve_file(int client_socket, const char* filepath, int worker_id);

/**
 * @brief Send HTTP error response (e.g., 404 Not Found)
 * @param client_socket The client socket file descriptor
 * @param status_code HTTP status code
 * @param message HTTP status message
 * @param worker_id The worker thread ID for logging
 */
void send_http_error(int client_socket, int status_code, const char* message, int worker_id);

#endif
