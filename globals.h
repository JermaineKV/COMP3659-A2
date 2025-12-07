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

// client request structure (Linked List Node)
struct Client_Request {
    int client_socket;                    // socket file descriptor
    struct sockaddr_in client_addr;       // client address info
    time_t request_time;                  // when request was received
    char request_buffer[8192];            // raw HTTP request data
    size_t request_size;                  // size of request data
    int worker_id;                        // which worker is handling this
    struct Client_Request *next;          // for linked list queue
};

// thread safe request queue (Producer-Consumer)
struct Shared_Queue {
    Client_Request *head;                 // queue head
    Client_Request *tail;                 // queue tail
    int queue_count;                      // current number of requests in queue
    int max_size;                         // maximum queue capacity
    
    pthread_mutex_t mutex;                // queue access mutex
    pthread_cond_t not_empty;             // signal when queue has items
    pthread_cond_t not_full;              // signal when queue has space
};

// worker thread structure
struct Worker_Thread {
    pthread_t thread_id;             // thread identifier
    int worker_id;                   // worker identifier
    int is_active;                   // status flag: 0 = inactive, 1 = active
    int num_requests;                // number of requests handled
    time_t last_active;              // timestamp of last thread activity
    Client_Request* current_request; // pointer to current request being processed
};

// thread pool structure
struct Thread_Pool {
    Worker_Thread* workers;               // array of worker threads
    int pool_size;                        // number of worker threads
    int active_workers;                   // currently active workers
    int shutdown_requested;               // flag to signal shutdown
    pthread_mutex_t pool_mutex;           // mutex for thread pool data
};

// server statistics
struct server_stats {
    // Connection statistics
    int total_connections;               // total connections since start
    int active_connections;              // currently active connections
    int max_concurrent_connections;      // peak concurrent connections
    
    // Request statistics
    int total_requests;                  // total HTTP requests served
    int successful_requests;             // 200 OK responses
    int not_found_requests;              // 404 Not Found responses
    int error_requests;                  // 500 Server Error responses
    
    // Timing information
    time_t server_start_time;            // when server started
    time_t last_request_time;            // most recent request
    
    // Thread synchronization for stats
    pthread_mutex_t stats_mutex;         // protect statistics updates
};

// main server configuration and state
struct server_config {
    // Network configuration
    int port;                            // server port (e.g., 8080)
    int server_socket;                   // main server socket
    char document_root[512];             // path to serve files from
    int max_connections;                 // maximum concurrent connections
    
    // Threading configuration
    int thread_pool_size;                // number of worker threads
    
    // Buffer and size limits
    int request_buffer_size;             // size of request buffers
    int response_buffer_size;            // size of response buffers
    int max_request_queue_size;          // max pending requests
    
    // Server state
    volatile int is_running;             // server running flag
    volatile int shutdown_requested;     // graceful shutdown flag
    
    // Logging
    char log_file[256];                  // log file path
    int log_level;                       // logging verbosity
    pthread_mutex_t log_mutex;           // thread-safe logging
};

// web server structure
struct Web_Server {
    server_config_t config;                // server configuration
    Thread_Pool thread_pool;               // worker thread pool
    Shared_Queue request_queue;            // request queue
    server_stats_t stats;                  // server statistics
    
    // Main server thread
    pthread_t master_thread;               // main accept() thread
    
    // Signal handling
    volatile sig_atomic_t signal_received; // for signal handling
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// main global server instance
extern Web_Server global_server;

// common MIME types array
typedef struct {
    char *extension;                     // file extension (e.g., ".html")
    char *mime_type;                     // MIME type (e.g., "text/html")
} mime_type_t;

void initialize_server_globals(void);
void cleanup_server_globals(void);
const char* get_mime_type(const char* filename);

#endif
