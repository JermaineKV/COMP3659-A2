/*
 * worker.h - Worker thread pool management
 *
 * Declares functions for managing the worker thread pool that processes
 * HTTP requests from the shared queue.
 *
 * Key functions:
 *   - init_thread_pool(): Create and start worker threads
 *   - clean_thread_pool(): Shutdown workers and free resources
 *   - worker_function(): Main loop for each worker thread
 *   - parse_http_request(): Extract file path from HTTP request
 */

#ifndef WORKER_H
#define WORKER_H

#include "globals.h"

void *worker_function(void *arg);
int init_thread_pool(Thread_Pool *pool, int num_workers);
void clean_thread_pool(Thread_Pool *pool);
int parse_http_request(const char* request_buffer, char* filename, size_t filename_size);

#endif
