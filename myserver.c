/*
 * myserver.c - Main entry point for multithreaded web server
 *
 * This is the main server program that initializes all components and runs
 * the accept loop. The server uses a thread pool to handle HTTP requests.
 *
 * Server startup sequence:
 *   1. Initialize global state and mutexes
 *   2. Create listening socket and bind to port
 *   3. Start worker thread pool
 *   4. Accept connections and enqueue to worker threads
 *   5. On SIGINT/SIGTERM: graceful shutdown
 *
 * Usage: ./myserver [port]   (default port: 8080)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>

#include "globals.h"
#include "worker.h"
#include "queue.h"

// Global flag for server running state
volatile sig_atomic_t server_running = 1;

/* Signal handler for graceful shutdown (SIGINT/SIGTERM) */
void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nShutting down server...\n");
        server_running = 0;
        global_server.config.shutdown_requested = 1; // set global shutdown flag
        
        // close the server socket to break the accept loop if possible
        if (global_server.config.server_socket > 0) {
            close(global_server.config.server_socket);
            global_server.config.server_socket = -1;
        }
    }
}

/* Main entry point - sets up socket, thread pool, and runs accept loop */
int main(int argc, char *argv[]) {
    // initialize globals with defaults
    initialize_server_globals();
    
    // initialize the queue mutexes and cond vars
    queue_init(&global_server.request_queue, DEFAULT_QUEUE_SIZE);
    
    // allow port to be passed as argument
    if (argc > 1) {
        global_server.config.port = atoi(argv[1]);
    }

    int port = global_server.config.port;
    
    // Ignore SIGPIPE to prevent crashes when writing to closed sockets
    signal(SIGPIPE, SIG_IGN);
    
    // setup signal handling
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // create server socket
    global_server.config.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (global_server.config.server_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // set socket options to reuse address
    int opt = 1;
    if (setsockopt(global_server.config.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // bind socket to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(global_server.config.server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // listen on socket (increased backlog for concurrent connections)
    if (listen(global_server.config.server_socket, 128) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);
    printf("Press Ctrl+C to stop.\n");

    // initialize thread pool
    init_thread_pool(&global_server.thread_pool, global_server.config.thread_pool_size);

    // main loop
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(global_server.config.server_socket, (struct sockaddr *)&client_addr, &client_len);
        
        if (!server_running) {
            if (client_socket >= 0) close(client_socket);
            break;
        }

        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // enqueue request
        enqueue_request(&global_server.request_queue, client_socket);
    }

    // cleanup
    printf("Cleaning up resources...\n");
    clean_thread_pool(&global_server.thread_pool);
    cleanup_server_globals();
    
    printf("Server shutdown complete.\n");
    return 0;
}
