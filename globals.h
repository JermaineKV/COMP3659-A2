/**
 * @file globals.h
 * @brief Global data structures for multithreaded web server
 *
 * This header defines all shared data structures, constants, and function
 * prototypes used throughout the web server. It serves as the central
 * configuration point for the server architecture.
 *
 * Structures:
 *   - Web_Server: Main container holding config, thread pool, queue, and stats
 *   - Thread_Pool: Array of worker threads, pool size, and shutdown flag
 *   - Shared_Queue: Linked list queue with mutex and condition variables
 *   - Client_Request: Socket, address, buffer, and timestamp for one request
 *   - Worker_Thread: Thread ID, worker ID, active status, and request count
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

/* Server configuration constants */
#define DEFAULT_PORT                8080
#define DEFAULT_THREAD_POOL_SIZE    10
#define DEFAULT_QUEUE_SIZE          50
#define DEFAULT_DOCUMENT_ROOT       "./www"
#define DEFAULT_REQUEST_BUFFER      8192
#define MAX_WORKER_LIMIT            100
#define LISTEN_BACKLOG              128
#define SOCKET_TIMEOUT_SEC          5


/* Forward declarations */
typedef struct Web_Server Web_Server;
typedef struct Client_Request Client_Request;
typedef struct Shared_Queue Shared_Queue;
typedef struct Thread_Pool Thread_Pool;
typedef struct Worker_Thread Worker_Thread;
typedef struct server_config server_config_t;

/* Data structures */

/* Client request - node in the request queue linked list */
struct Client_Request {
    int client_socket;
    struct sockaddr_in client_addr;
    time_t request_time;
    char request_buffer[DEFAULT_REQUEST_BUFFER];
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
    pthread_mutex_t pool_mutex;
};

/* Server configuration and state */
struct server_config {
    int port;
    int server_socket;
    char document_root[512];
    int thread_pool_size;
    volatile int shutdown_requested;
};

/* Main server structure - contains all server state */
struct Web_Server {
    server_config_t config;
    Thread_Pool thread_pool;
    Shared_Queue request_queue;
};

/* Global server instance (defined in globals.c) */
extern Web_Server global_server;

/* MIME type mapping */
typedef struct {
    char *extension;
    char *mime_type;
} mime_type_t;

/* Function prototypes */

/** @brief Initialize all server structures with default values */
void initialize_server_globals(void);

/** @brief Clean up all server resources (free queue, destroy mutexes, close socket) */
void cleanup_server_globals(void);

/**
 * @brief Get MIME type based on file extension
 * @param filename The file to get MIME type for
 * @return MIME type string (e.g., "text/html") or "application/octet-stream"
 */
const char* get_mime_type(const char* filename);

#endif
