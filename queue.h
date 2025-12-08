/**
 * @file queue.h
 * @brief Thread-safe request queue (producer-consumer)
 *
 * Declares functions for the bounded request queue used to pass client
 * connections from the main thread to worker threads.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "globals.h"

/**
 * @brief Initialize the request queue
 * @param queue Pointer to Shared_Queue structure
 * @param max_size Maximum queue capacity
 */
void queue_init(Shared_Queue* queue, int max_size);

/**
 * @brief Add client socket to queue (producer operation)
 * @param queue Pointer to Shared_Queue structure
 * @param client_socket The client socket file descriptor
 * @details Blocks if queue is full, signals not_empty when done.
 */
void enqueue_request(Shared_Queue* queue, int client_socket);

/**
 * @brief Remove and return next request (consumer operation)
 * @param queue Pointer to Shared_Queue structure
 * @return Pointer to Client_Request, or NULL on shutdown
 * @details Blocks if queue is empty, signals not_full when done.
 */
Client_Request* dequeue_request(Shared_Queue* queue);

#endif
