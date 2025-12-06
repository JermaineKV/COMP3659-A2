#include "globals.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h> // for close()

// Define the global server instance
Web_Server global_server;


// Common MIME types for static file serving
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
    {NULL,    "application/octet-stream"} // Default for unknown types
};

int g_mime_types_count = sizeof(g_mime_types) / sizeof(mime_type_t) - 1;

// Initialize all server global structures
void initialize_server_globals(void) {
    // Initialize configuration with defaults
    global_server.config.port = DEFAULT_PORT;
    global_server.config.server_socket = -1;
    strcpy(global_server.config.document_root, DEFAULT_DOCUMENT_ROOT);
    global_server.config.max_connections = DEFAULT_MAX_CONNECTIONS;
    global_server.config.thread_pool_size = DEFAULT_THREAD_POOL_SIZE;
    global_server.config.request_buffer_size = DEFAULT_REQUEST_BUFFER;
    global_server.config.response_buffer_size = DEFAULT_RESPONSE_BUFFER;
    global_server.config.max_request_queue_size = DEFAULT_QUEUE_SIZE;
    global_server.config.is_running = 0;
    global_server.config.shutdown_requested = 0;
    strcpy(global_server.config.log_file, "server.log");
    global_server.config.log_level = 1; // INFO level
    
    // Initialize request queue
    global_server.request_queue.head = NULL;
    global_server.request_queue.tail = NULL;
    global_server.request_queue.queue_count = 0;
    global_server.request_queue.max_size = DEFAULT_QUEUE_SIZE;
    
    // Initialize thread pool
    global_server.thread_pool.workers = NULL;
    global_server.thread_pool.pool_size = DEFAULT_THREAD_POOL_SIZE;
    global_server.thread_pool.active_workers = 0;
    global_server.thread_pool.shutdown_requested = 0;
    
    // Initialize statistics
    global_server.stats.total_connections = 0;
    global_server.stats.active_connections = 0;
    global_server.stats.max_concurrent_connections = 0;
    global_server.stats.total_requests = 0;
    global_server.stats.successful_requests = 0;
    global_server.stats.not_found_requests = 0;
    global_server.stats.error_requests = 0;
    global_server.stats.server_start_time = time(NULL);
    global_server.stats.last_request_time = 0;
    
    // Initialize signal handling
    global_server.signal_received = 0;
    
    // Initialize mutexes and condition variables
    pthread_mutex_init(&global_server.config.log_mutex, NULL);
    pthread_mutex_init(&global_server.request_queue.mutex, NULL);
    pthread_cond_init(&global_server.request_queue.not_empty, NULL);
    pthread_cond_init(&global_server.request_queue.not_full, NULL);
    pthread_mutex_init(&global_server.thread_pool.pool_mutex, NULL);
    pthread_mutex_init(&global_server.stats.stats_mutex, NULL);
}

// Clean up all server global structures
void cleanup_server_globals(void) {
    // Note: Thread pool cleanup is handled by clean_thread_pool() in worker.c
    // We do not free workers or destroy pool mutex here to avoid double free/destroy
    
    // Clean up any remaining requests in queue
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
    
    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&global_server.config.log_mutex);
    pthread_mutex_destroy(&global_server.request_queue.mutex);
    pthread_cond_destroy(&global_server.request_queue.not_empty);
    pthread_cond_destroy(&global_server.request_queue.not_full);
    // pthread_mutex_destroy(&global_server.thread_pool.pool_mutex); // Handled in clean_thread_pool
    pthread_mutex_destroy(&global_server.stats.stats_mutex);
    
    // Close server socket if open
    if (global_server.config.server_socket >= 0) {
        close(global_server.config.server_socket);
        global_server.config.server_socket = -1;
    }
}

// Statistics tracking is available for future use but not currently reported

// Get MIME type based on file extension
const char* get_mime_type(const char* filename) {
    if (!filename) {
        return "application/octet-stream";
    }
    
    // Find the last dot in filename
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    
    // Search through our MIME types array
    for (int i = 0; i < g_mime_types_count; i++) {
        if (strcasecmp(ext, g_mime_types[i].extension) == 0) {
            return g_mime_types[i].mime_type;
        }
    }
    
    // Default MIME type for unknown extensions
    return "application/octet-stream";
}
