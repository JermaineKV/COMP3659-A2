#ifndef WORKER_H
#define WORKER_H

#include <unistd.h>
#include <time.h>
#include <pthread.h>
/* 
* Module that contains thread management functions for worker threads.
*/

#define MAX_WORKERS 10

typedef struct Master_Thread {
    pthread_t thread_id;            // master thread identifier
    int num_workers;                // number of worker threads it's managing

    Worker_Thread* worker;         // array of worker threads
} Master_Thread;

typedef struct Worker_Thread {
    pthread_t thread_id;            // thread identifier
    int worker_id;                  // worker identifier
    int is_active;                  // status flag if thread is active 0 = inactive or 1 = active
    int num_requests;               // number of requests handled
    time_t last_active;             // timestamp of last thread activity
    // add pointer to current client it's serving
} Worker_Thread;


// Worker thread function that each thread will execute
void* worker_function(void* arg) {
    Worker_Thread* worker = (Worker_Thread*)arg;

    while (1) {
        // Wait for work assignments
        // Process requests
        // Update worker status
    }
    return NULL;
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

#endif