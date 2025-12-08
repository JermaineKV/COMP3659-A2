/**
 * @file globals.c
 * @brief Global server state and initialization
 *
 * Implements the global server instance and provides initialization/cleanup
 * functions. Also defines MIME type mappings for HTTP Content-Type headers.
 */

#include "globals.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

/* Global server instance */
Web_Server global_server;

/* Global server running flag */
volatile sig_atomic_t server_running = 1;

/* MIME type mappings for Content-Type header */
mime_type_t g_mime_types[] = {
    {".html", "text/html"},
    {".htm",  "text/html"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".json", "application/json"},
    {".txt",  "text/plain"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".ico",  "image/x-icon"},
    {".pdf",  "application/pdf"},
    {".zip",  "application/zip"},
    {NULL,    "application/octet-stream"} /* default */
};

int g_mime_types_count = sizeof(g_mime_types) / sizeof(mime_type_t) - 1;

/**
 * @brief Initialize all server structures with default values
 * @details
 *   - Sets all config values to defaults (port 8080, pool size 10, etc.)
 *   - Initializes request queue, thread pool, and stats structures
 *   - Creates all mutexes and condition variables
 */
void initialize_server_globals(void) {
    /* Config defaults */
    global_server.config.port = DEFAULT_PORT;
    global_server.config.server_socket = -1;
    strcpy(global_server.config.document_root, DEFAULT_DOCUMENT_ROOT);
    global_server.config.thread_pool_size = DEFAULT_THREAD_POOL_SIZE;
    global_server.config.shutdown_requested = 0;
    
    /* Request queue - initialize with mutex and condition variables */
    global_server.request_queue.head = NULL;
    global_server.request_queue.tail = NULL;
    global_server.request_queue.queue_count = 0;
    global_server.request_queue.max_size = DEFAULT_QUEUE_SIZE;
    pthread_mutex_init(&global_server.request_queue.mutex, NULL);
    pthread_cond_init(&global_server.request_queue.not_empty, NULL);
    pthread_cond_init(&global_server.request_queue.not_full, NULL);
    
    /* Thread pool */
    global_server.thread_pool.workers = NULL;
    global_server.thread_pool.pool_size = DEFAULT_THREAD_POOL_SIZE;
    global_server.thread_pool.active_workers = 0;
}

/**
 * @brief Clean up all server resources
 * @details
 *   - Frees any remaining requests in the queue
 *   - Destroys all mutexes and condition variables
 *   - Closes the server socket if still open
 */
void cleanup_server_globals(void) {
    /* Free remaining requests in queue */
    pthread_mutex_lock(&global_server.request_queue.mutex);
    Client_Request *current = global_server.request_queue.head;
    while (current) {
        Client_Request *next = current->next;
        free(current);
        current = next;
    }
    global_server.request_queue.head = NULL;
    global_server.request_queue.tail = NULL;
    global_server.request_queue.queue_count = 0;
    pthread_mutex_unlock(&global_server.request_queue.mutex);
    
    /* Destroy mutexes and condition variables */
    pthread_mutex_destroy(&global_server.request_queue.mutex);
    pthread_cond_destroy(&global_server.request_queue.not_empty);
    pthread_cond_destroy(&global_server.request_queue.not_full);
    
    /* Close server socket */
    if (global_server.config.server_socket >= 0) {
        close(global_server.config.server_socket);
        global_server.config.server_socket = -1;
    }
}

/**
 * @brief Get MIME type based on file extension
 * @param filename The filename to check for extension
 * @return MIME type string (e.g., "text/html") or "application/octet-stream"
 * @details
 *   - Extracts file extension from filename using strrchr()
 *   - Searches g_mime_types[] array for matching extension
 */
const char* get_mime_type(const char* filename) {
    if (!filename) return "application/octet-stream";
    
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    
    for (int i = 0; i < g_mime_types_count; i++) {
        if (strcasecmp(ext, g_mime_types[i].extension) == 0) {
            return g_mime_types[i].mime_type;
        }
    }
    
    return "application/octet-stream";
}

/**
 * @brief Signal handler for SIGINT and SIGTERM
 * @param sig The signal number received
 * @details Sets server_running = 0 and shutdown_requested = 1.
 *          Closes server socket to unblock accept().
 */
void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        write(STDOUT_FILENO, "\nShutting down server...\n", 26);
        server_running = 0;
        global_server.config.shutdown_requested = 1; // set global shutdown flag
        
        // close the server socket to break the accept loop if possible
        if (global_server.config.server_socket > 0) {
            close(global_server.config.server_socket);
            global_server.config.server_socket = -1;
        }
    }
}
