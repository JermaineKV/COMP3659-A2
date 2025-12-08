/**
 * @file queue.c
 * @brief Thread-safe request queue implementation
 *
 * Implements a bounded buffer queue for the producer-consumer pattern.
 * The main thread (producer) enqueues client connections, and worker
 * threads (consumers) dequeue and process them.
 *
 * Synchronization:
 *   - mutex: Protects all queue operations
 *   - not_empty: Signals workers when queue has items
 *   - not_full: Signals producer when queue has space
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "queue.h"
#include "globals.h"

/**
 * @brief Initialize the request queue
 * @param q Pointer to Shared_Queue structure
 * @param max_size Maximum number of requests in queue
 * @details
 *   - Sets head/tail to NULL, queue_count to 0
 *   - Stores max_size for bounded buffer limit
 *   - Initializes mutex and both condition variables
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
 * @brief Add a client socket to the queue (producer operation)
 * @param q Pointer to Shared_Queue structure
 * @param client_socket The client socket file descriptor
 * @details
 *   - Allocates new Client_Request with malloc()
 *   - Locks mutex, waits on not_full if queue is full
 *   - Adds request to tail of linked list
 *   - Increments queue_count, signals not_empty
 *   - Checks shutdown flag to avoid blocking forever
 */
void enqueue_request(Shared_Queue *q, int client_socket) {
    if (q == NULL) return;
    
    // Don't enqueue if shutdown is requested
    if (global_server.config.shutdown_requested) {
        close(client_socket);
        return;
    }
    
    // create new request object
    Client_Request *request = (Client_Request*)malloc(sizeof(Client_Request));
    if (request == NULL) {
        write(STDOUT_FILENO, "Error: Failed to allocate memory for request\n", 45);
        close(client_socket);
        return;
    }
    
    request->client_socket = client_socket;
    request->request_time = time(NULL);
    request->next = NULL;
    request->worker_id = -1;
    
    pthread_mutex_lock(&q->mutex);
    
    // wait while queue is full (with shutdown check)
    while (q->queue_count >= q->max_size) {
        if (global_server.config.shutdown_requested) {
            pthread_mutex_unlock(&q->mutex);
            close(client_socket);
            free(request);
            return;
        }
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
 * @brief Remove and return next request from queue (consumer operation)
 * @param q Pointer to Shared_Queue structure
 * @return Pointer to Client_Request, or NULL on shutdown
 * @details
 *   - Locks mutex, waits on not_empty if queue is empty
 *   - Removes request from head of linked list
 *   - Decrements queue_count, signals not_full
 *   - Returns NULL if shutdown requested while waiting
 */
Client_Request* dequeue_request(Shared_Queue *q) {
    if (q == NULL) return NULL; // invalid queue pointer
    
    // critical section
    pthread_mutex_lock(&q->mutex);
    
    // wait while queue is empty
    while (q->queue_count == 0) {
        // check for shutdown condition before and after wait
        if (global_server.config.shutdown_requested) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
        }
        pthread_cond_wait(&q->not_empty, &q->mutex);
        
        // Re-check shutdown after waking up
        if (global_server.config.shutdown_requested) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
        }
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
