/**
 * @file myserver.c
 * @brief Main entry point for multithreaded web server
 *
 * This is the main server program that initializes all components and runs
 * the accept loop. The server uses a thread pool to handle HTTP requests.
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

/**
 * @brief Main entry point - sets up socket, thread pool, and runs accept loop
 * @param argc Argument count
 * @param argv Argument vector (optional port number)
 * @return 0 on successful shutdown
 * @details
 *   - Calls initialize_server_globals() to set defaults
 *   - Parses optional port argument from argv[1]
 *   - Installs signal handlers (SIGINT, SIGTERM, ignores SIGPIPE)
 *   - Creates TCP socket, sets SO_REUSEADDR, binds to port
 *   - Calls listen() with backlog of 128
 *   - Calls init_thread_pool() to create 10 worker threads
 *   - Runs accept loop: accept() -> enqueue_request()
 *   - On shutdown: clean_thread_pool(), cleanup_server_globals()
 */
int main(int argc, char *argv[]) {
    // initialize globals with defaults
    initialize_server_globals();
    
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
        write(STDOUT_FILENO, "Error: Failed to create socket\n", 32);
        exit(EXIT_FAILURE);
    }

    // set socket options to reuse address
    int opt = 1;
    if (setsockopt(global_server.config.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        write(STDOUT_FILENO, "Error: setsockopt failed\n", 25);
        exit(EXIT_FAILURE);
    }

    // bind socket to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(global_server.config.server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        write(STDOUT_FILENO, "Error: Bind failed\n", 19);
        exit(EXIT_FAILURE);
    }

    // listen on socket (increased backlog for concurrent connections)
    if (listen(global_server.config.server_socket, LISTEN_BACKLOG) < 0) {
        write(STDOUT_FILENO, "Error: Listen failed\n", 21);
        exit(EXIT_FAILURE);
    }

    char msg[64];
    int msg_len = snprintf(msg, sizeof(msg), "Server listening on port %d\n", port);
    write(STDOUT_FILENO, msg, msg_len);
    write(STDOUT_FILENO, "Press Ctrl+C to stop.\n", 22);

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
            write(STDOUT_FILENO, "Error: Accept failed\n", 21);
            continue;
        }

        // enqueue request
        enqueue_request(&global_server.request_queue, client_socket);
    }

    // cleanup
    write(STDOUT_FILENO, "Cleaning up resources...\n", 25);
    clean_thread_pool(&global_server.thread_pool);
    cleanup_server_globals();
    
    write(STDOUT_FILENO, "Server shutdown complete.\n", 26);
    return 0;
}
