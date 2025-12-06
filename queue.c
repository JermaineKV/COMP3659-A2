#include "queue.h"
#include "globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/*
 * ========================================================================
 * FUNCTION: queue_init
 * ========================================================================
 * 
 * Initializes the shared request queue structure.
 * 
 * ========================================================================
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

/*
 * ========================================================================
 * FUNCTION: enqueue_request
 * ========================================================================
 * 
 * Adds a request to the tail of the queue.
 * Thread-safe: Locks mutex, waits if full, signals not_empty.
 * 
 * ========================================================================
 */
void enqueue_request(Shared_Queue *q, int client_socket) {
    if (q == NULL) return;
    
    // Create new request object
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
    
    // Wait while queue is full
    while (q->queue_count >= q->max_size) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    
    // Add to tail
    if (q->tail == NULL) {
        q->head = request;
        q->tail = request;
    } else {
        q->tail->next = request;
        q->tail = request;
    }
    
    q->queue_count++;
    
    // Signal that queue is not empty
    pthread_cond_signal(&q->not_empty);
    
    pthread_mutex_unlock(&q->mutex);
}

/*
 * ========================================================================
 * FUNCTION: dequeue_request
 * ========================================================================
 * 
 * Removes and returns a request from the head of the queue.
 * Thread-safe: Locks mutex, waits if empty, signals not_full.
 * 
 * ========================================================================
 */
Client_Request* dequeue_request(Shared_Queue *q) {
    if (q == NULL) return NULL;
    
    pthread_mutex_lock(&q->mutex);
    
    // Wait while queue is empty
    while (q->queue_count == 0) {
        // Check for shutdown condition
        if (global_server.config.shutdown_requested) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
        }
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    
    Client_Request *request = q->head;
    if (request != NULL) {
        q->head = request->next;
        if (q->head == NULL) {
            q->tail = NULL;
        }
        q->queue_count--;
        
        // Signal that queue is not full
        pthread_cond_signal(&q->not_full);
    }
    
    pthread_mutex_unlock(&q->mutex);
    
    return request;
}
