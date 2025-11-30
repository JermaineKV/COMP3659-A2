#ifndef WORKER_H
#define WORKER_H

#define MAX_WORKERS 10

typedef struct Master_Thread {
    pthread_t thread_id;             // master thread identifier
    int num_workers;                 // number of worker threads it's managing

    Worker_Thread* worker;           // array of worker threads
} Master_Thread;

typedef struct Worker_Thread {
    pthread_t thread_id;             // thread identifier
    int worker_id;                   // worker identifier
    int is_active;                   // status flag if thread is active 0 = inactive or 1 = active
    int num_requests;                // number of requests handled
    time_t last_active;              // timestamp of last thread activity
    Client_Request* current_request; // pointer to current request being processed
} Worker_Thread;

void worker_function(void* arg);
Client_Request* dequeue_request (Shared_Queue* queue);
int init_thread_pool(Master_Thread* master, int num_workers);
void clean_thread_pool(Master_Thread* master);
int parse_http_request(const char* request_buffer, char* filename, size_t filename_size);

#endif