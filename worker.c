#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "globals.h"
#include "worker.h"
#include "queue.h"
#include "files.h"

/* 
* Module that contains thread management functions for worker threads.
*/

// FUNCTION: parse_http_request() - parses HTTP request to extract the filename
int parse_http_request(const char* request_buffer, char* filename, size_t filename_size) {
    // Find the start of the request line
    const char* method_start = request_buffer;
    
    // Skip leading whitespace
    while (*method_start == ' ' || *method_start == '\t') {
        method_start++;
    }
    
    // Find the space after the method (GET, POST, etc.)
    const char* path_start = strchr(method_start, ' ');
    if (path_start == NULL) {
        return -1; // Invalid request format
    }
    
    // Skip spaces between method and path
    path_start++;
    while (*path_start == ' ') {
        path_start++;
    }
    
    // Find the space after the path (before HTTP version)
    const char* path_end = strchr(path_start, ' ');
    if (path_end == NULL) {
        // Try to find newline instead
        path_end = strchr(path_start, '\r');
        if (path_end == NULL) {
            path_end = strchr(path_start, '\n');
        }
        if (path_end == NULL) {
            return -1; // Invalid request format
        }
    }
    
    // Calculate the length of the path
    size_t path_len = path_end - path_start;
    
    // Check if filename buffer is large enough
    if (path_len >= filename_size) {
        path_len = filename_size - 1; // Truncate to fit
    }
    
    // Copy the path to the filename buffer
    strncpy(filename, path_start, path_len);
    filename[path_len] = '\0'; // Null terminate
    
    // If path is "/", serve index.html
    if (strcmp(filename, "/") == 0) {
        strncpy(filename, "/index.html", filename_size);
    }
    
    return 0; // Success
}

// Worker thread function that each thread will execute
void *worker_function(void* arg) {
    Worker_Thread* worker = (Worker_Thread*)arg;

    while (!global_server.config.shutdown_requested) {

        Client_Request* request = dequeue_request(&global_server.request_queue);
        
        if (request == NULL) {
            // If shutdown requested, we might get NULL or wake up with empty queue
            if (global_server.config.shutdown_requested) break;
            continue;
        }

        // critical section to update worker status as active
        pthread_mutex_lock(&global_server.thread_pool.pool_mutex);
        worker->is_active = 1; // mark worker as active
        worker->current_request = request; // assign current request
        worker->last_active = time(NULL); // update last active time
        global_server.thread_pool.active_workers++;
        pthread_mutex_unlock(&global_server.thread_pool.pool_mutex);

        request->worker_id = worker->worker_id; // assign worker ID to request
        
        // Process HTTP requests ***********************************************
        
        // Set timeout for socket operations (e.g., 5 seconds)
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(request->client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        setsockopt(request->client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
        
        // Read data from the socket
        ssize_t bytes_read = read(request->client_socket, request->request_buffer, 
                                 sizeof(request->request_buffer) - 1);
        
        if (bytes_read > 0) {
            request->request_buffer[bytes_read] = '\0'; // Null terminate
            request->request_size = bytes_read;
            
            // parse the HTTP request to extract the filename
            char filename[1024];
            if (parse_http_request(request->request_buffer, filename, sizeof(filename)) == 0) {
                // successfully parsed the filename
                // log the parsed request (can be removed in production)
                char log_msg[2048];
                int log_len = snprintf(log_msg, sizeof(log_msg), 
                                      "[Worker %d] Parsed request: %s\n", 
                                      worker->worker_id, filename);
                write(STDOUT_FILENO, log_msg, log_len);
                
                // Construct full path
                char full_path[2048];
                snprintf(full_path, sizeof(full_path), "%s%s", 
                         global_server.config.document_root, filename);
                
                // Serve the file
                serve_file(request->client_socket, full_path, worker->worker_id);
                
            } else {
                // failed to parse request
                send_http_error(request->client_socket, 400, "Bad Request", worker->worker_id);
            }
        } else if (bytes_read == 0) {
            // client closed connection
            // write(STDOUT_FILENO, "[Worker] Client closed connection\n", 36);
        } else {
            // read error
            // write(STDOUT_FILENO, "[Worker] Error: Failed to read from socket\n", 45);
        }

        // critical section to update worker status as inactive
        pthread_mutex_lock(&global_server.thread_pool.pool_mutex);
        worker->is_active = 0;
        worker->current_request = NULL;
        worker->num_requests++;
        global_server.thread_pool.active_workers--;
        pthread_mutex_unlock(&global_server.thread_pool.pool_mutex);

        // Close the socket
        close(request->client_socket);
        
        free(request); // free the processed request
    }
    
    return NULL;
}

// FUNCTION: init_thread_pool() - initializes the thread pool for worker threads
int init_thread_pool(Thread_Pool* pool, int num_workers) {
    if (num_workers > 100) { // Hard limit check
        write(STDOUT_FILENO, "Error: Exceeded maximum number of workers\n", 40);
        return -1; // error: too many workers
    }

    pool->workers = (Worker_Thread*)malloc(sizeof(Worker_Thread) * num_workers); // allocate memory for worker threads
    if (pool->workers == NULL) return -1;
    
    pool->pool_size = num_workers; // set the number of workers
    pool->active_workers = 0;
    pool->shutdown_requested = 0;
    
    pthread_mutex_init(&pool->pool_mutex, NULL);

    // initialize each worker thread
    for (int i = 0; i < num_workers; i++) {
        pool->workers[i].worker_id = i;
        pool->workers[i].is_active = 0;
        pool->workers[i].num_requests = 0;
        pool->workers[i].last_active = time(NULL);
        pool->workers[i].current_request = NULL;

        if (pthread_create(&pool->workers[i].thread_id, NULL, worker_function, (void*)&pool->workers[i]) != 0) {
            write(STDOUT_FILENO, "Error: Failed to create worker thread\n", 38);
            return -1; // error: thread creation failed
        }
        
    }
    return 0; // success
}

// FUNCTION: clean_thread_pool() - cleans up the thread pool and frees resources
void clean_thread_pool(Thread_Pool* pool) {
    pool->shutdown_requested = 1;
    
    // Signal all workers to wake up
    pthread_cond_broadcast(&global_server.request_queue.not_empty);

    for (int i = 0; i < pool->pool_size; i++) {
        pthread_join(pool->workers[i].thread_id, NULL); // wait for thread to finish
    }
    
    free(pool->workers); // free allocated memory
    pthread_mutex_destroy(&pool->pool_mutex);
}
