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

// Global flag for shutdown
volatile sig_atomic_t server_running = 1;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nShutting down server...\n");
        server_running = 0;
        global_server.config.shutdown_requested = 1; // Set global shutdown flag
        
        // Close the server socket to break the accept loop if possible, 
        // or rely on the loop condition check.
        // Note: accept() is blocking, so we might need to close the socket from another thread 
        // or use select/poll with timeout. For simplicity, we'll just set the flag.
        if (global_server.config.server_socket > 0) {
            close(global_server.config.server_socket);
            global_server.config.server_socket = -1;
        }
    }
}

int main(int argc, char *argv[]) {
    // 1. Initialize globals with defaults
    initialize_server_globals();
    
    // Initialize the queue mutexes and cond vars
    queue_init(&global_server.request_queue, DEFAULT_QUEUE_SIZE);
    
    // Optional: Allow port to be passed as argument
    if (argc > 1) {
        global_server.config.port = atoi(argv[1]);
    }

    int port = global_server.config.port;
    
    // 2. Setup signal handling
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // 3. Create server socket
    global_server.config.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (global_server.config.server_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(global_server.config.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // 4. Bind
    if (bind(global_server.config.server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 5. Listen
    if (listen(global_server.config.server_socket, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);
    printf("Press Ctrl+C to stop.\n");

    // 6. Initialize thread pool
    init_thread_pool(&global_server.thread_pool, global_server.config.thread_pool_size);

    // 7. Main loop
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

        // Log connection (optional)
        // printf("New connection from %s\n", inet_ntoa(client_addr.sin_addr));

        // 8. Enqueue request
        enqueue_request(&global_server.request_queue, client_socket);
    }

    // 9. Cleanup
    printf("Cleaning up resources...\n");
    clean_thread_pool(&global_server.thread_pool);
    cleanup_server_globals();
    
    printf("Server shutdown complete.\n");
    return 0;
}
