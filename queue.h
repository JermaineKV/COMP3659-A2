#ifndef QUEUE_H
#define QUEUE_H

#include "globals.h"

void queue_init(Shared_Queue* queue, int max_size);
void enqueue_request(Shared_Queue* queue, int client_socket);
Client_Request* dequeue_request(Shared_Queue* queue);

#endif
