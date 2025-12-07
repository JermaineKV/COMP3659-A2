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
 * ============================================================================
 * FUNCTION: send_http_error()
 * ============================================================================
 * 
 * Sends an HTTP error response with appropriate status code and error message
 * back to the client. Constructs proper HTTP headers and an HTML error page.
 * 
 * PARAMETERS:
 *   - client_socket:   Socket file descriptor of the client connection.
 *                      Used to write the error response back to client
 *   - status_code:     HTTP status code (e.g., 404, 500, 400)
 *   - message:         Readable error message (e.g., "Not Found")
 *                      Displayed in HTTP response and HTML body
 *   - worker_id:       ID of the worker thread processing this request
 *                      Used for logging/debugging purposes if needed
 * 
 * RETURN VALUE:
 *   - void: The function doesn't return a value. Errors are logged to STDOUT
 * 
 * ============================================================================
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
                             
    // send headers and body
    write(client_socket, header, header_len);
    write(client_socket, body, body_len);
    
    // log error
    char log_msg[512];
    int log_len = snprintf(log_msg, sizeof(log_msg),
                          "[Worker %d] Sent Error: %d %s\n",
                          worker_id, status_code, message);
    write(STDOUT_FILENO, log_msg, log_len); // output to terminal
}

/**
 * ============================================================================
 * FUNCTION: read_file()
 * ============================================================================
 * 
 * Reads the complete contents of a file into a dynamically allocated buffer.
 * Opens file in read only mode, determines size, allocates memory, and reads
 * all bytes into the provided buffer pointer.
 * 
 * PARAMETERS:
 *   - filepath:    Path to the file to be read (e.g., "./www/index.html").
 *                  Must be a valid null-terminated string
 *   - buffer:      Pointer to char pointer. Points to newly allocated memory 
 *                  containing file contents. Caller must free() this memory
 *   - file_size:   Pointer to ssize_t. Stores the number of bytes read from 
 *                  the file
 * 
 * RETURN VALUE:
 *   - 0: Success. File was read completely and buffer allocated.
 *   - -1: Failure.
 *      > File does not exist or cannot be opened
 *      > Cannot stat file (fstat failed)
 *      > Memory allocation failed
 *      > Read operation failed (bytes read != expected size)
 * 
 * NOTES:
 *   - Caller must free() the allocated buffer when done
 *   - No null terminator is added to buffer (raw file contents)
 *   - Large files may cause memory issues if malloc fails
 * 
 * ============================================================================
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
 * ============================================================================
 * FUNCTION: serve_file()
 * ============================================================================
 * 
 * Serves a file to a client over HTTP. Reads the file from disk, determines 
 * its file type, constructs proper HTTP response headers, and sends both 
 * headers and file contents to the client socket.
 * 
 * PARAMETERS:
 *   - client_socket:   Socket file descriptor of the connected client
 *                      Used to send HTTP response
 *   - filepath:        Full path to the file to serve (e.g., "./www/index.html")
 *                      Must be a valid null-terminated string
 *   - worker_id:       ID of the worker thread processing this request
 *                      Used for logging/debugging purposes
 * 
 * RETURN VALUE:
 *   - 0: Success. File was read and sent to client with "HTTP 200 OK" response.
 *   - -1: Failure.
 *      > File does not exist or cannot be read (sends "404 Not Found")
 *      > Socket write failed when sending headers (sends "500 Internal Server Error")
 *      > Socket write failed when sending file contents
 * 
 * NOTES:
 *   - Memory allocated by read_file() is freed before returning
 *   - Assumes client_socket is a valid, connected socket
 * 
 * ============================================================================
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
