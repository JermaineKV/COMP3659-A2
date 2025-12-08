/**
 * @file files.c
 * @brief File operations and HTTP response handling
 *
 * Implements file I/O and HTTP response generation. Reads static files from
 * the document root and sends them to clients with proper HTTP headers.
 *
 * HTTP response format:
 *   HTTP/1.1 <status> <message>\r\n
 *   Content-Type: <mime-type>\r\n
 *   Content-Length: <size>\r\n
 *   Connection: close\r\n
 *   \r\n
 *   <body>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "files.h"
#include "globals.h"

/**
 * @brief Send an HTTP error response to the client
 * @param client_socket The client socket file descriptor
 * @param status_code HTTP status code (e.g., 404, 500)
 * @param message HTTP status message (e.g., "Not Found")
 * @param worker_id The worker thread ID for logging
 * @details
 *   - Creates HTML body: "<h1>404 Not Found</h1>"
 *   - Builds HTTP response header with Content-Type: text/html
 *   - Writes header and body to socket
 *   - Logs error to stdout
 */
void send_http_error(int client_socket, int status_code, const char* message, int worker_id) {
    char header[512]; // HTTP response header buffer
    char body[512];   // HTML body buffer
    
    // create HTML body
    int body_len = snprintf(body, sizeof(body),
                           "<html><body><h1>%d %s</h1></body></html>",
                           status_code, message);
                           
    // create HTTP headers - general practice
    int header_len = snprintf(header, sizeof(header),
                             "HTTP/1.1 %d %s\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: %d\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             status_code, message, body_len);
                             
    // send headers and body (ignore errors - client may have disconnected)
    if (write(client_socket, header, header_len) < 0) return;
    if (write(client_socket, body, body_len) < 0) return;
    
    // log error
    char log_msg[512];
    int log_len = snprintf(log_msg, sizeof(log_msg),
                          "[Worker %d] Sent Error: %d %s\n",
                          worker_id, status_code, message);
    write(STDOUT_FILENO, log_msg, log_len); // output to terminal
}

/**
 * @brief Read entire file into buffer
 * @param filepath Path to the file to read
 * @param buffer Output pointer to allocated buffer (caller must free)
 * @param file_size Output pointer to store file size
 * @return 0 on success, -1 on error
 * @details
 *   - Opens file with open() in read-only mode
 *   - Gets file size with fstat()
 *   - Allocates buffer with malloc()
 *   - Reads entire file into buffer
 */
int read_file(const char* filepath, char** buffer, ssize_t* file_size) {
    struct stat file_stat;
    
    // open the file in read-only mode
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        return -1; // File not found or cannot open
    }
    
    // get file information (size, permissions, etc.)
    if (fstat(fd, &file_stat) == -1) {
        close(fd);
        return -1;
    }
    
    // store the file size
    *file_size = file_stat.st_size;
    
    // allocate buffer for file contents
    *buffer = (char*)malloc(*file_size);
    if (*buffer == NULL) {
        close(fd);
        return -1; // memory allocation failed
    }
    
    // read the entire file into the buffer
    ssize_t bytes_read = read(fd, *buffer, *file_size);
    if (bytes_read != *file_size) {
        free(*buffer);
        *buffer = NULL;
        close(fd);
        return -1; // error
    }
    
    close(fd);
    return 0; // success
}

/**
 * @brief Serve a file over HTTP
 * @param client_socket The client socket file descriptor
 * @param filepath Path to the file to serve
 * @param worker_id The worker thread ID for logging
 * @return 0 on success, -1 on error
 * @details
 *   - Calls read_file() to load file contents
 *   - If file not found, calls send_http_error(404)
 *   - Calls get_mime_type() for Content-Type header
 *   - Builds and sends HTTP 200 response with file body
 *   - Logs success with file size and MIME type
 */
int serve_file(int client_socket, const char* filepath, int worker_id) {
    char* file_buffer = NULL;
    ssize_t file_size = 0;
    
    // read the file contents
    if (read_file(filepath, &file_buffer, &file_size) != 0) {
        // file not found or read error
        send_http_error(client_socket, 404, "Not Found", worker_id);
        return -1;
    }
    
    // get the content type based on file extension
    const char* content_type = get_mime_type(filepath);
    
    // create HTTP response headers
    char header[512];
    int header_len = snprintf(header, sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %ld\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             content_type, file_size);
    
    // send HTTP headers (ignore partial writes - client may have disconnected)
    ssize_t bytes_sent = write(client_socket, header, header_len);
    if (bytes_sent < 0) {
        free(file_buffer);
        return -1; // Don't try to send error if write failed
    }
    
    // send file contents
    bytes_sent = write(client_socket, file_buffer, file_size);
    if (bytes_sent < 0) {
        free(file_buffer);
        return -1;
    }
    
    // log success
    char log_msg[512];
    int log_len = snprintf(log_msg, sizeof(log_msg),
                          "[Worker %d] Served file: %s (%ld bytes, %s)\n",
                          worker_id, filepath, file_size, content_type);
    write(STDOUT_FILENO, log_msg, log_len);
    
    free(file_buffer);
    return 0; // Success
}
