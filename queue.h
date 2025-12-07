/*
 * queue.h - Thread-safe request queue (producer-consumer)
 *
 * Declares functions for the bounded request queue used to pass client
 * connections from the main thread to worker threads.
 *
 * Key functions:
 *   - queue_init(): Initialize queue with mutexes and condition variables
 *   - enqueue_request(): Add client socket to queue (blocks if full)
 *   - dequeue_request(): Remove request from queue (blocks if empty)
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "globals.h"

void queue_init(Shared_Queue* queue, int max_size);
void enqueue_request(Shared_Queue* queue, int client_socket);
Client_Request* dequeue_request(Shared_Queue* queue);

#endif
