#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "globals.h"
#include "worker.h"
/* 
* Module that contains thread management functions for worker threads.
*/

// Worker thread function that each thread will execute
void worker_function(void* arg) {
    Worker_Thread* worker = (Worker_Thread*)arg;

    while (!clean_thread_pool) {

        Client_Request* request = dequeue_request(&global_server.request_queue);
        
        if (request == NULL) {
            break; // no request to process
        }

        // critical section to update worker status as active
        pthread_mutex_lock(&global_server.thread_pool.pool_mutex);
        worker->is_active = 1; // mark worker as active
        worker->current_request = request; // assign current request
        worker->last_active = time(NULL); // update last active time
        global_server.thread_pool.active_workers++;
        pthread_mutex_unlock(&global_server.thread_pool.pool_mutex);

        request->worker_id = worker->worker_id; // assign worker ID to request
        
        // Process HTTP requests ***********************************************

        // critical section to update worker status as inactive
        pthread_mutex_lock(&global_server.thread_pool.pool_mutex);
        worker->is_active = 0;
        worker->current_request = NULL;
        worker->num_requests++;
        global_server.thread_pool.active_workers--;
        pthread_mutex_unlock(&global_server.thread_pool.pool_mutex);

        // Update server *******************************************************

        free(request); // free the processed request
    }
}

// FUNCTION: dequeue_request() - removes a request from the shared queue
Client_Request* dequeue_request (Shared_Queue* queue) {
    
    Client_Request* request = NULL; // pointer to hold dequeued request

    // critical section to access shared queue
    pthread_mutex_lock(&queue->mutex);

    // while queue is empty and server is running - WAIT
    while (queue->queue_count == 0 && !global_server.config.shutdown_requested) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    // FIFO - remove request from head of queue
    request = queue->head;
    if (request != NULL) {
        queue->head = request->next; // move head to next request
        
        // if queue is empty, update tail to NULL
        if (queue->head == NULL) {
            queue->tail = NULL;
        }

        queue->queue_count--;

        pthread_cond_signal(&queue->not_full); // signal that queue is not full
    }
    pthread_mutex_unlock(&queue->mutex);
    
    return request;
}

// FUNCTION: thread_pool() - initializes the thread pool for worker threads
// all start out inactive and waiting for work from the master thread
int init_thread_pool(Master_Thread* master, int num_workers) {
    if (master->num_workers > MAX_WORKERS) {
        write(STDOUT_FILENO, "Error: Exceeded maximum number of workers\n", 40);
        return -1; // error: too many workers
    }

    master->worker = (Worker_Thread*)malloc(sizeof(Worker_Thread) * num_workers); // allocate memory for worker threads
    master->num_workers = num_workers; // set the number of workers

    // initialize each worker thread
    for (int i = 0; i < num_workers; i++) {
        master->worker[i].worker_id = i;
        master->worker[i].is_active = 0;
        master->worker[i].num_requests = 0;
        master->worker[i].last_active = time(NULL);

        if (pthread_create(&master->worker[i].thread_id, NULL, worker_function, (void*)&master->worker[i]) != 0) {
            write(STDOUT_FILENO, "Error: Failed to create worker thread\n", 38);
            return -1; // error: thread creation failed
        }
        
    }
    return 0; // success
}

// FUNCTION: clean_thread_pool() - cleans up the thread pool and frees resources
void clean_thread_pool(Master_Thread* master) {
    for (int i = 0; i < master->num_workers; i++) {
        pthread_cancel(master->worker[i].thread_id); // cancel the worker thread using pthread_cancel
        pthread_join(master->worker[i].thread_id, NULL); // wait for thread to finish
    }
    free(master->worker); // free allocated memory
}