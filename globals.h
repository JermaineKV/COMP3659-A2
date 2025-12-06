// Global data structures for multi-threaded web server
#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <signal.h>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

typedef struct Web_Server Web_Server;
typedef struct Client_Request Client_Request;
typedef struct Shared_Queue Shared_Queue;
typedef struct Thread_Pool Thread_Pool;
typedef struct Worker_Thread Worker_Thread;
typedef struct server_stats server_stats_t;
typedef struct server_config server_config_t;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Client request structure (Linked List Node)
struct Client_Request {
    int client_socket;                    // Socket file descriptor
    struct sockaddr_in client_addr;       // Client address info
    time_t request_time;                  // When request was received
    char request_buffer[8192];            // Raw HTTP request data
    size_t request_size;                  // Size of request data
    int worker_id;                        // Which worker is handling this
    struct Client_Request *next;          // For linked list queue
};

// Thread-safe request queue (Producer-Consumer)
struct Shared_Queue {
    Client_Request *head;                 // Queue head
    Client_Request *tail;                 // Queue tail
    int queue_count;                      // Current number of requests in queue
    int max_size;                         // Maximum queue capacity
    
    pthread_mutex_t mutex;                // Queue access mutex
    pthread_cond_t not_empty;             // Signal when queue has items
    pthread_cond_t not_full;              // Signal when queue has space
};

// Worker Thread Structure
struct Worker_Thread {
    pthread_t thread_id;             // thread identifier
    int worker_id;                   // worker identifier
    int is_active;                   // status flag: 0 = inactive, 1 = active
    int num_requests;                // number of requests handled
    time_t last_active;              // timestamp of last thread activity
    Client_Request* current_request; // pointer to current request being processed
};

// Thread Pool Structure
struct Thread_Pool {
    Worker_Thread* workers;               // Array of worker threads
    int pool_size;                        // Number of worker threads
    int active_workers;                   // Currently active workers
    int shutdown_requested;               // Flag to signal shutdown
    pthread_mutex_t pool_mutex;           // Mutex for thread pool data
};

// Server statistics (thread-safe access required)
struct server_stats {
    // Connection statistics
    int total_connections;               // Total connections since start
    int active_connections;              // Currently active connections
    int max_concurrent_connections;      // Peak concurrent connections
    
    // Request statistics
    int total_requests;                  // Total HTTP requests served
    int successful_requests;             // 200 OK responses
    int not_found_requests;              // 404 Not Found responses
    int error_requests;                  // 500 Server Error responses
    
    // Timing information
    time_t server_start_time;            // When server started
    time_t last_request_time;            // Most recent request
    
    // Thread synchronization for stats
    pthread_mutex_t stats_mutex;         // Protect statistics updates
};

// Main server configuration and state
struct server_config {
    // Network configuration
    int port;                            // Server port (e.g., 8080)
    int server_socket;                   // Main server socket
    char document_root[512];             // Path to serve files from
    int max_connections;                 // Maximum concurrent connections
    
    // Threading configuration
    int thread_pool_size;                // Number of worker threads
    
    // Buffer and size limits
    int request_buffer_size;             // Size of request buffers
    int response_buffer_size;            // Size of response buffers
    int max_request_queue_size;          // Max pending requests
    
    // Server state
    volatile int is_running;             // Server running flag
    volatile int shutdown_requested;     // Graceful shutdown flag
    
    // Logging
    char log_file[256];                  // Log file path
    int log_level;                       // Logging verbosity
    pthread_mutex_t log_mutex;           // Thread-safe logging
};

// Web server structure
struct Web_Server {
    server_config_t config;                // Server configuration
    Thread_Pool thread_pool;              // Worker thread pool
    Shared_Queue request_queue;            // Request queue
    server_stats_t stats;                  // Server statistics
    
    // Main server thread
    pthread_t master_thread;               // Main accept() thread
    
    // Signal handling
    volatile sig_atomic_t signal_received; // For signal handling
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Main global server instance
extern Web_Server global_server;

// Common MIME types array
typedef struct {
    char *extension;                     // File extension (e.g., ".html")
    char *mime_type;                     // MIME type (e.g., "text/html")
} mime_type_t;

// MIME types array is private to globals.c

// ============================================================================
// CONSTANTS
// ============================================================================

#define DEFAULT_PORT                8080
#define DEFAULT_THREAD_POOL_SIZE    10
#define DEFAULT_MAX_CONNECTIONS     100
#define DEFAULT_QUEUE_SIZE          50
#define DEFAULT_DOCUMENT_ROOT       "./www"
#define DEFAULT_REQUEST_BUFFER      8192
#define DEFAULT_RESPONSE_BUFFER     65536

#define HTTP_OK                     200
#define HTTP_NOT_FOUND              404
#define HTTP_INTERNAL_ERROR         500

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void initialize_server_globals(void);
void cleanup_server_globals(void);
const char* get_mime_type(const char* filename);

#endif // GLOBALS_H
