#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "queue.h"
#include "globals.h"

/**
 * ============================================================================
 * FUNCTION: queue_init()
 * ============================================================================
 * 
 * Initializes the shared request queue structure with empty state and creates
 * synchronization primitives (mutex and condition variables).
 * 
 * PARAMETERS:
 *   - q:           Pointer to the Shared_Queue structure to initialize
 *                  Must point to valid allocated memory
 *   - max_size:    Maximum number of requests the queue can hold
 * 
 * RETURN VALUE:
 *   - void:        No return value
 * 
 * NOTES:
 *   - Must be called once before any other queue functions
 * 
 * ============================================================================
 */
void queue_init(Shared_Queue *q, int max_size) {
    if (q == NULL) return;
    
    q->head = NULL;
    q->tail = NULL;
    q->queue_count = 0;
    q->max_size = max_size;
    
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

/**
 * ============================================================================
 * FUNCTION: enqueue_request()
 * ============================================================================
 * 
 * Adds a client request to the tail of the queue. Allocates memory for the
 * request, acquires mutex lock, waits if queue is full, adds request to queue, 
 * signals waiting worker threads, and releases lock.
 * 
 * PARAMETERS:
 *   - q:               Pointer to the Shared_Queue structure where request
 *                      will be added. Must be initialized via queue_init()
 *   - client_socket:   Socket file descriptor of the connected client
 *                      This is what worker threads will read from
 * 
 * RETURN VALUE:
 *   - void:            No return value
 * 
 * NOTES:
 *   - Blocking operation: can pause main thread if queue is full
 *   - Only signals one worker thread (pthread_cond_signal)
 *      - If multiple workers are sleeping, only one wakes up
 * 
 * ============================================================================
 */
void enqueue_request(Shared_Queue *q, int client_socket) {
    if (q == NULL) return;
    
    // create new request object
    Client_Request *request = (Client_Request*)malloc(sizeof(Client_Request));
    if (request == NULL) {
        perror("Failed to allocate memory for request");
        return;
    }
    
    request->client_socket = client_socket;
    request->request_time = time(NULL);
    request->next = NULL;
    request->worker_id = -1;
    
    pthread_mutex_lock(&q->mutex);
    
    // wait while queue is full
    while (q->queue_count >= q->max_size) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    
    // add to tail
    if (q->tail == NULL) {
        q->head = request;
        q->tail = request;
    } else {
        q->tail->next = request;
        q->tail = request;
    }
    
    q->queue_count++; // increment count
    
    // signal that queue is not empty
    pthread_cond_signal(&q->not_empty);
    
    pthread_mutex_unlock(&q->mutex);
}

/**
 * ============================================================================
 * FUNCTION: dequeue_request()
 * ============================================================================
 * 
 * Removes and returns a request from the head of the queue. Acquires mutex
 * lock, waits if queue is empty (blocking until request arrives), removes
 * request from head, signals backpressure relief if queue was full, and
 * releases lock.
 * 
 * PARAMETERS:
 *   - q:   Pointer to the Shared_Queue structure to dequeue from
 *          Must be initialized via queue_init(). Called by worker threads 
 *          in worker_function().
 * 
 * RETURN VALUE:
 *   - Client_Request*: Pointer to dequeued Client_Request structure
 *      > Non-NULL:     Success. Request contains client_socket and other info.
 *      > NULL:         Queue is empty and shutdown was requested
 * 
 * NOTES:
 *   - Blocking operation: worker threads sleep here when no requests
 *   - Caller must free() the returned Client_Request when done
 * 
 * ============================================================================
 */
Client_Request* dequeue_request(Shared_Queue *q) {
    if (q == NULL) return NULL; // invalid queue pointer
    
    // critical section
    pthread_mutex_lock(&q->mutex);
    
    // wait while queue is empty
    while (q->queue_count == 0) {
        // check for shutdown condition
        if (global_server.config.shutdown_requested) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
        }
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    
    // remove from head
    Client_Request *request = q->head;
    if (request != NULL) {
        q->head = request->next;
        if (q->head == NULL) {
            q->tail = NULL;
        }
        q->queue_count--;
        
        // signal that queue is not full
        pthread_cond_signal(&q->not_full);
    }
    
    pthread_mutex_unlock(&q->mutex); // end of critical section
    
    return request;
}
