#ifndef QUEUE_H
#define QUEUE_H

#include "globals.h"

/**
 * Initialize the shared queue
 */
void queue_init(Shared_Queue* queue, int max_size);

/**
 * Add a client socket to the queue
 */
void enqueue_request(Shared_Queue* queue, int client_socket);

/**
 * Remove a request from the queue
 */
Client_Request* dequeue_request(Shared_Queue* queue);

#endif
