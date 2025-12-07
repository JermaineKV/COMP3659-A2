/*
 * globals.h - Global data structures for multithreaded web server
 *
 * This header defines all shared data structures, constants, and function
 * prototypes used throughout the web server. It serves as the central
 * configuration point for the server architecture.
 *
 * Key structures:
 *   - Web_Server: Main server state container
 *   - Thread_Pool: Manages worker threads
 *   - Shared_Queue: Thread-safe request queue (producer-consumer)
 *   - Client_Request: Individual HTTP request data
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <signal.h>

/* Server configuration constants */
#define DEFAULT_PORT                8080
#define DEFAULT_THREAD_POOL_SIZE    10
#define DEFAULT_MAX_CONNECTIONS     100
#define DEFAULT_QUEUE_SIZE          50
#define DEFAULT_DOCUMENT_ROOT       "./www"
#define DEFAULT_REQUEST_BUFFER      8192
#define DEFAULT_RESPONSE_BUFFER     65536

/* HTTP status codes */
#define HTTP_OK                     200
#define HTTP_NOT_FOUND              404
#define HTTP_INTERNAL_ERROR         500


/* Forward declarations */
typedef struct Web_Server Web_Server;
typedef struct Client_Request Client_Request;
typedef struct Shared_Queue Shared_Queue;
typedef struct Thread_Pool Thread_Pool;
typedef struct Worker_Thread Worker_Thread;
typedef struct server_stats server_stats_t;
typedef struct server_config server_config_t;

/* Data structures */

/* Client request - node in the request queue linked list */
struct Client_Request {
    int client_socket;
    struct sockaddr_in client_addr;
    time_t request_time;
    char request_buffer[8192];
    size_t request_size;
    int worker_id;
    struct Client_Request *next;
};

/* Thread-safe request queue (producer-consumer pattern) */
struct Shared_Queue {
    Client_Request *head;
    Client_Request *tail;
    int queue_count;
    int max_size;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

/* Individual worker thread info */
struct Worker_Thread {
    pthread_t thread_id;
    int worker_id;
    int is_active;
    int num_requests;
    time_t last_active;
    Client_Request* current_request;
};

/* Thread pool manager */
struct Thread_Pool {
    Worker_Thread* workers;
    int pool_size;
    int active_workers;
    int shutdown_requested;
    pthread_mutex_t pool_mutex;
};

/* Server statistics */
struct server_stats {
    int total_connections;
    int active_connections;
    int max_concurrent_connections;
    int total_requests;
    int successful_requests;
    int not_found_requests;
    int error_requests;
    time_t server_start_time;
    time_t last_request_time;
    pthread_mutex_t stats_mutex;
};

/* Server configuration and state */
struct server_config {
    int port;
    int server_socket;
    char document_root[512];
    int max_connections;
    int thread_pool_size;
    int request_buffer_size;
    int response_buffer_size;
    int max_request_queue_size;
    volatile int is_running;
    volatile int shutdown_requested;
    char log_file[256];
    int log_level;
    pthread_mutex_t log_mutex;
};

/* Main server structure - contains all server state */
struct Web_Server {
    server_config_t config;
    Thread_Pool thread_pool;
    Shared_Queue request_queue;
    server_stats_t stats;
    pthread_t master_thread;
    volatile sig_atomic_t signal_received;
};

/* Global server instance (defined in globals.c) */
extern Web_Server global_server;

/* MIME type mapping */
typedef struct {
    char *extension;
    char *mime_type;
} mime_type_t;

/* Function prototypes */
void initialize_server_globals(void);
void cleanup_server_globals(void);
const char* get_mime_type(const char* filename);

#endif
