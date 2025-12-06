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
 * Send an HTTP error response
 */
void send_http_error(int client_socket, int status_code, const char* message, int worker_id) {
    char header[512];
    char body[512];
    
    // Create HTML body
    int body_len = snprintf(body, sizeof(body),
                           "<html><body><h1>%d %s</h1></body></html>",
                           status_code, message);
                           
    // Create HTTP headers
    int header_len = snprintf(header, sizeof(header),
                             "HTTP/1.1 %d %s\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: %d\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             status_code, message, body_len);
                             
    // Send headers and body
    write(client_socket, header, header_len);
    write(client_socket, body, body_len);
    
    // Log error
    char log_msg[512];
    int log_len = snprintf(log_msg, sizeof(log_msg),
                          "[Worker %d] Sent Error: %d %s\n",
                          worker_id, status_code, message);
    write(STDOUT_FILENO, log_msg, log_len);
}

/**
 * Read a file into a dynamically allocated buffer
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
        return -1; // Memory allocation failed
    }
    
    // read the entire file into the buffer
    ssize_t bytes_read = read(fd, *buffer, *file_size);
    if (bytes_read != *file_size) {
        free(*buffer);
        *buffer = NULL;
        close(fd);
        return -1; // read error
    }
    
    close(fd);
    return 0; // success
}

/**
 * Serve a file over a socket with proper HTTP headers
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
    
    // send HTTP headers
    ssize_t bytes_sent = write(client_socket, header, header_len);
    if (bytes_sent != header_len) {
        free(file_buffer);
        send_http_error(client_socket, 500, "Internal Server Error", worker_id);
        return -1;
    }
    
    // send file contents
    bytes_sent = write(client_socket, file_buffer, file_size);
    if (bytes_sent != file_size) {
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
