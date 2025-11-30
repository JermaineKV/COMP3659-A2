#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include "server.h"

// global queue instance
job_queue_t request_queue;

int main(){
    int server_fd, client_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    pthread_t thread_pool[THREAD_POOL_SIZE];

    // initialize the queue
    queue_init(&request_queue);

    // network initialization 

    // create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // configure address settings
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on any IP
    address.sin_port = htons(PORT); // Listen on port 8080

    // bind socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // start listening 
    if (listen(server_fd, 10)<0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Create thread pool
    for (int i = 0; i < THREAD_POOL_SIZE; i++){
        if (pthread_create(&thread_pool[i], NULL, worker_routine, NULL) != 0){
            perror("Failed to create thread");
        }
    }

    // The accept loop
    while(1){
        printf("Waiting for connection...\n");

        // accept a new connection (blocks here until a client connects)
        if ((client_socket = accept(server_fd,(struct sockaddr*)&address,&addrlen)) <0){
            perror("Accept failed");
            continue;
        }

        printf("Connection received! Socket ID: %d\n", client_socket);

        // pass the new socket to the queue
        enqueue(&request_queue, client_socket);
    }

    return 0; 
}