#include "globals.h"
#include <string.h>
#include <stdlib.h>

// Global server instance
web_server_t g_server;

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
    g_server.config.port = DEFAULT_PORT;
    g_server.config.server_socket = -1;
    strcpy(g_server.config.document_root, DEFAULT_DOCUMENT_ROOT);
    g_server.config.max_connections = DEFAULT_MAX_CONNECTIONS;
    g_server.config.thread_pool_size = DEFAULT_THREAD_POOL_SIZE;
    g_server.config.request_buffer_size = DEFAULT_REQUEST_BUFFER;
    g_server.config.response_buffer_size = DEFAULT_RESPONSE_BUFFER;
    g_server.config.max_request_queue_size = DEFAULT_QUEUE_SIZE;
    g_server.config.is_running = 0;
    g_server.config.shutdown_requested = 0;
    strcpy(g_server.config.log_file, "server.log");
    g_server.config.log_level = 1; // INFO level
    
    // Initialize request queue
    g_server.request_queue.head = NULL;
    g_server.request_queue.tail = NULL;
    g_server.request_queue.count = 0;
    g_server.request_queue.max_size = DEFAULT_QUEUE_SIZE;
    
    // Initialize thread pool
    g_server.thread_pool.workers = NULL;
    g_server.thread_pool.pool_size = DEFAULT_THREAD_POOL_SIZE;
    g_server.thread_pool.active_workers = 0;
    g_server.thread_pool.shutdown_requested = 0;
    
    // Initialize statistics
    reset_server_stats();
    
    // Initialize signal handling
    g_server.signal_received = 0;
    
    // Initialize mutexes and condition variables
    pthread_mutex_init(&g_server.config.log_mutex, NULL);
    pthread_mutex_init(&g_server.request_queue.mutex, NULL);
    pthread_cond_init(&g_server.request_queue.not_empty, NULL);
    pthread_cond_init(&g_server.request_queue.not_full, NULL);
    pthread_mutex_init(&g_server.thread_pool.pool_mutex, NULL);
    pthread_mutex_init(&g_server.stats.stats_mutex, NULL);
}

// Clean up all server global structures
void cleanup_server_globals(void) {
    // Clean up worker threads array if allocated
    if (g_server.thread_pool.workers) {
        free(g_server.thread_pool.workers);
        g_server.thread_pool.workers = NULL;
    }
    
    // Clean up any remaining requests in queue
    pthread_mutex_lock(&g_server.request_queue.mutex);
    client_request_t *current = g_server.request_queue.head;
    while (current) {
        client_request_t *next = current->next;
        free(current);
        current = next;
    }
    g_server.request_queue.head = NULL;
    g_server.request_queue.tail = NULL;
    g_server.request_queue.count = 0;
    pthread_mutex_unlock(&g_server.request_queue.mutex);
    
    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&g_server.config.log_mutex);
    pthread_mutex_destroy(&g_server.request_queue.mutex);
    pthread_cond_destroy(&g_server.request_queue.not_empty);
    pthread_cond_destroy(&g_server.request_queue.not_full);
    pthread_mutex_destroy(&g_server.thread_pool.pool_mutex);
    pthread_mutex_destroy(&g_server.stats.stats_mutex);
    
    // Close server socket if open
    if (g_server.config.server_socket >= 0) {
        close(g_server.config.server_socket);
        g_server.config.server_socket = -1;
    }
}

// Reset server statistics
void reset_server_stats(void) {
    pthread_mutex_lock(&g_server.stats.stats_mutex);
    
    g_server.stats.total_connections = 0;
    g_server.stats.active_connections = 0;
    g_server.stats.max_concurrent_connections = 0;
    g_server.stats.total_requests = 0;
    g_server.stats.successful_requests = 0;
    g_server.stats.not_found_requests = 0;
    g_server.stats.error_requests = 0;
    g_server.stats.server_start_time = time(NULL);
    g_server.stats.last_request_time = 0;
    
    pthread_mutex_unlock(&g_server.stats.stats_mutex);
}

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