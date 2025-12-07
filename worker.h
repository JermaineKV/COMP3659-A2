#ifndef WORKER_H
#define WORKER_H

#include "globals.h"

void *worker_function(void *arg);
int init_thread_pool(Thread_Pool *pool, int num_workers);
void clean_thread_pool(Thread_Pool *pool);
int parse_http_request(const char* request_buffer, char* filename, size_t filename_size);

#endif
