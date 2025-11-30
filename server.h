#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define THREAD_POOL_SIZE 5
#define QUEUE_SIZE 100

// The job queue structure
typedef struct {
    int client_sockets[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
} job_queue_t;

// Function prototypes
void queue_init(job_queue_t *q);
void enqueue(job_queue_t *q, int client_socket);
int dequeue(job_queue_t *q);
void *worker_routine(void *arg);

#endif