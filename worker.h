/**
 * @file worker.h
 * @brief Worker thread pool management
 *
 * Declares functions for managing the worker thread pool that processes
 * HTTP requests from the shared queue.
 */

#ifndef WORKER_H
#define WORKER_H

#include "globals.h"

/**
 * @brief Entry point for worker threads
 * @param arg Pointer to Worker_Thread structure
 * @return NULL on thread exit
 */
void *worker_function(void *arg);

/**
 * @brief Initialize thread pool and create worker threads
 * @param pool Pointer to Thread_Pool structure
 * @param num_workers Number of worker threads to create
 * @return 0 on success, -1 on error
 */
int init_thread_pool(Thread_Pool *pool, int num_workers);

/**
 * @brief Shutdown thread pool and free resources
 * @param pool Pointer to Thread_Pool structure
 */
void clean_thread_pool(Thread_Pool *pool);

/**
 * @brief Parse HTTP request and extract file path
 * @param request_buffer The raw HTTP request string
 * @param filename Output buffer for extracted path
 * @param filename_size Size of filename buffer
 * @return 0 on success, -1 if malformed request
 */
int parse_http_request(const char* request_buffer, char* filename, size_t filename_size);

#endif
