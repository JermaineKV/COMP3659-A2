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

/**
 * ============================================================================
 * FUNCTION: parse_http_request()
 * ============================================================================
 * 
 * Parses an HTTP request to extract the requested file path. Handles the format 
 * "METHOD /path HTTP/version" and extracts the path.
 * 
 * PARAMETERS:
 *   - request_buffer:  The raw HTTP request data (may contain
 *                      full headers). Must be null-terminated.
 *                      Example: "GET /index.html HTTP/1.1\r\n..."
 *   - filename:        Output buffer where the extracted path is stored
 *   - filename_size:   Size of the filename output buffer in bytes
 *                      Path is shortened if larger than this size
 * 
 * RETURN VALUE:
 *   - 0: Success. filename buffer contains the extracted path
 *   - -1: Failure. Invalid HTTP request format. Possible reasons:
 *      > No space found after HTTP method
 *      > No space or newline found after path
 * 
 * NOTES:
 *   - Only extracts the path; does not validate HTTP version or method
 *   - Works with any HTTP method (GET, POST, HEAD, etc.)
 *   - Does not decode URL-encoded characters (e.g., %20 for space)
 * 
 * ============================================================================
 */
int parse_http_request(const char* request_buffer, char* filename, size_t filename_size) {
    // find the start of the request line
    const char* method_start = request_buffer;
    
    // skip leading whitespace
    while (*method_start == ' ' || *method_start == '\t') {
        method_start++;
    }
    
    // find space after the method (GET, POST, etc.)
    const char* path_start = strchr(method_start, ' ');
    if (path_start == NULL) {
        return -1; // invalid request format
    }
    
    // skip spaces between method and path
    path_start++;
    while (*path_start == ' ') {
        path_start++;
    }
    
    // find the space after the path (before HTTP version)
    const char* path_end = strchr(path_start, ' ');
    if (path_end == NULL) {
        // try to find newline instead
        path_end = strchr(path_start, '\r');
        if (path_end == NULL) {
            path_end = strchr(path_start, '\n');
        }
        if (path_end == NULL) {
            return -1; // invalid request format
        }
    }
    
    // calculate the length of the path
    size_t path_len = path_end - path_start;
    
    // check if filename buffer is large enough
    if (path_len >= filename_size) {
        path_len = filename_size - 1; // truncate to fit
    }
    
    // copy the path to the filename buffer
    strncpy(filename, path_start, path_len);
    filename[path_len] = '\0'; // null terminate
    
    // if path is "/", serve index.html by default
    if (strcmp(filename, "/") == 0) {
        strncpy(filename, "/index.html", filename_size);
    }
    
    return 0; // success
}

/**
 * ============================================================================
 * FUNCTION: worker_function()
 * ============================================================================
 * 
 * Main execution function for each worker thread. Implements the core request
 * processing loop: dequeue requests, parse HTTP requests, serve files, and
 * handle errors. The function runs continuously until shutdown is requested.
 * 
 * PARAMETERS:
 *   - arg:     Pointer to a Worker_Thread structure containing the
 *              worker's ID and state information. Cast internally to
 *              (Worker_Thread*) for use
 * 
 * RETURN VALUE:
 *   - NULL:    Always returns NULL. Return occurs when shutdown_requested 
 *              flag is set
 * 
 * BEHAVIOR:
 *   1. Enters infinite loop until global_server.config.shutdown_requested is set
 *   2. Calls dequeue_request() to wait for a client request (blocking)
 *   3. Updates worker state to "active" with mutex protection
 *   4. Sets socket timeout (5 seconds) to prevent indefinite blocking
 *   5. Reads HTTP request data from client socket
 *   6. On successful read:
 *      - Calls parse_http_request() to extract file path
 *      - Constructs full file path combining document root + requested path
 *      - Calls serve_file() to handle file serving or send errors
 *   7. On parse failure: sends HTTP 400 Bad Request error
 *   8. On read failure (0 bytes or error): handles gracefully
 *   9. Updates worker state to "inactive"
 *   10. Increments request counter
 *   11. Closes client socket and frees request memory
 *   12. Loops back to wait for next request
 * 
 * ============================================================================
 */
void *worker_function(void* arg) {
    Worker_Thread* worker = (Worker_Thread*)arg;

    while (!global_server.config.shutdown_requested) {

        Client_Request* request = dequeue_request(&global_server.request_queue);
        
        if (request == NULL) {
            // if shutdown requested, we might get NULL or wake up with empty queue
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
        
        // set timeout for socket operations (e.g., 5 seconds)
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(request->client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        setsockopt(request->client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
        
        // read data from the socket
        ssize_t bytes_read = read(request->client_socket, request->request_buffer, 
                                 sizeof(request->request_buffer) - 1);
        
        if (bytes_read > 0) {
            request->request_buffer[bytes_read] = '\0'; // null terminate
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
                
                // construct full path
                char full_path[2048];
                snprintf(full_path, sizeof(full_path), "%s%s", 
                         global_server.config.document_root, filename);
                
                // serve the file
                serve_file(request->client_socket, full_path, worker->worker_id);
                
            } else {
                // failed to parse request
                send_http_error(request->client_socket, 400, "Bad Request", worker->worker_id);
            }
        } else if (bytes_read == 0) {
            // client closed connection
            write(STDOUT_FILENO, "[Worker] Client closed connection\n", 36);
        } else {
            // read error
            write(STDOUT_FILENO, "[Worker] Error: Failed to read from socket\n", 45);
        }

        // critical section to update worker status as inactive
        pthread_mutex_lock(&global_server.thread_pool.pool_mutex);
        worker->is_active = 0;
        worker->current_request = NULL;
        worker->num_requests++;
        global_server.thread_pool.active_workers--;
        pthread_mutex_unlock(&global_server.thread_pool.pool_mutex);

        // close the socket
        close(request->client_socket);
        
        free(request); // free the processed request
    }
    
    return NULL;
}

/**
 * ============================================================================
 * FUNCTION: init_thread_pool
 * ============================================================================
 * 
 * Initializes the thread pool by allocating memory for worker thread structures
 * and creating the specified number of worker threads. Each thread begins
 * executing worker_function() and waits for requests via the request queue.
 * 
 * PARAMETERS:
 *   - pool:            Pointer to the Thread_Pool structure to initialize
 *                      Pool must exist
 *   - num_workers:     Number of worker threads to create
 *                      Must be > 0 and <= 100 (hard limit)
 * 
 * RETURN VALUE:
 *   - 0: Success. All worker threads created and running.
 *   - -1: Failure. Reasons include:
 *      > num_workers > 100 (exceeds maximum)
 *      > malloc() fails to allocate worker array
 *      > pthread_create() fails for any thread
 * 
 * BEHAVIOR:
 *   1. Validates num_workers is within acceptable range (0 < num_workers <= 100)
 *   2. Allocates heap memory for Worker_Thread array (num_workers entries)
 *   3. Initializes pool structure fields:
 *      - pool_size = num_workers
 *      - active_workers = 0
 *      - shutdown_requested = 0
 *   4. Creates pthread_mutex for pool synchronization
 *   5. For each worker (0 to num_workers-1):
 *      - Sets worker_id, is_active=0, num_requests=0, last_active=now()
 *      - Creates new thread executing worker_function()
 *      - Returns -1 immediately on pthread_create() failure
 *   6. Returns 0 on complete success
 * 
 * NOTES:
 *   - Hard limit of 100 workers prevents resource exhaustion
 *   - All workers share access to global_server global structure
 * 
 * ============================================================================
 */
int init_thread_pool(Thread_Pool* pool, int num_workers) {
    if (num_workers > 100) { // hard limit check
        write(STDOUT_FILENO, "Error: Exceeded maximum number of workers\n", 40);
        return -1; // too many workers
    }

    pool->workers = (Worker_Thread*)malloc(sizeof(Worker_Thread) * num_workers); // allocate memory for worker threads
    if (pool->workers == NULL) return -1;
    
    // initialize the thread pool structure
    pool->pool_size = num_workers;
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

/**
 * ============================================================================
 * FUNCTION: clean_thread_pool
 * ============================================================================
 * 
 * Initiates shutdown of all worker threads and cleans up pool resources. 
 * Sets shutdown flag, wakes all waiting threads, joins all threads, and frees 
 * allocated memory.
 * 
 * PARAMETERS:
 *   - pool:    Pointer to the Thread_Pool structure previously initialized 
 *              by init_thread_pool().
 * 
 * RETURN VALUE:
 *   - void:    This is a cleanup function and assumes pool is valid and in a 
 *              consistent state.
 * 
 * BEHAVIOR:
 *   1. Sets pool->shutdown_requested = 1 to signal threads to exit
 *   2. Calls pthread_cond_broadcast() on request_queue.not_empty to wake
 *      all worker threads waiting in dequeue_request()
 *   3. Iterates through all workers (0 to pool_size-1):
 *      - Calls pthread_join() on each worker thread
 *      - Blocks until that thread exits worker_function()
 *   4. After all threads exit:
 *      - Calls free() on pool->workers to release allocated memory
 *      - Calls pthread_mutex_destroy() on pool->pool_mutex
 * 
 * NOTES:
 *   - Should be called from main thread only, not from worker threads
 *   - Blocking operation: waits for all worker threads to terminate
 * 
 * ============================================================================
 */
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
