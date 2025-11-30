#include "server.h"
#include <stdio.h>
#include <stdlib.h>

// Initialize the queue 
void queue_init(job_queue_t *q){
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->condition, NULL);
}

// Add a client socket to the queue
void enqueue(job_queue_t *q, int client_socket){
    // lock the mutex
    pthread_mutex_lock(&q->mutex);

    // Check if queue is full 
    if (q->count >= QUEUE_SIZE){
        printf("Queue is full. Dropping connection.\n");
    }
    else{
        // Add socket to the queue
        q->client_sockets[q->rear] = client_socket;
        q->rear = (q->rear + 1) % QUEUE_SIZE;
        q->count ++; 

        // Signal worker threads that job is ready
        pthread_cond_signal(&q->condition);
    }

    pthread_mutex_unlock(&q->mutex);
}

// Remove and return a client socket from the queue (blocks if empty)
int dequeue(job_queue_t *q){
    pthread_mutex_lock(&q->mutex);

    while (q->count == 0){
        pthread_cond_wait(&q->condition, &q->mutex);
    }

    int client_socket = q->client_sockets[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;

    pthread_mutex_unlock(&q->mutex);
    return client_socket;
}