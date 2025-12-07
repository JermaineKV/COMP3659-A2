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
 * ============================================================================
 * MODULE: Web Server
 * ============================================================================
 * 
 * This is the main entry point for the multithreaded HTTP web server. 
 * The server:
 *   1. Initializes global state and synchronization primitives
 *   2. Sets up signal handling for graceful shutdown (SIGINT, SIGTERM)
 *   3. Creates a listening socket on a specified port
 *   4. Initializes a thread pool of worker threads
 *   5. Accepts incoming client connections in the main loop
 *   6. Enqueues each connection to worker threads for processing
 *   7. Performs graceful shutdown when signaled
 * 
 * Key Components:
 *   - handle_signal(): Signal handler for SIGINT/SIGTERM
 *   - main(): Server initialization, main accept loop, and cleanup
 * 
 * ============================================================================
 */

// Global flag for server running state
volatile sig_atomic_t server_running = 1;

/**
 * ============================================================================
 * FUNCTION: handle_signal
 * ============================================================================
 * 
 * Signal handler for SIGINT (Ctrl+C) and SIGTERM. Sets global shutdown flags
 * and closes the server socket to initiate graceful shutdown of the server.
 * 
 * PARAMETERS:
 *   - sig:     Signal number received (SIGINT=2 or SIGTERM=15
 *              Handler only processes SIGINT and SIGTERM, ignores others
 * 
 * RETURN VALUE:
 *   - void:    Function is called by the kernel when signal is received.
 * 
 * NOTES:
 *   - Called asynchronously by kernel
 *   - Coordinates shutdown with main thread via flags
 * 
 * ============================================================================
 */
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

/**
 * ============================================================================
 * FUNCTION: main()
 * ============================================================================
 * 
 * Main entry point for the HTTP web server. Initializes all server components,
 * sets up networking, creates worker threads, and runs the main accept loop
 * to accept client connections and distribute them to workers.
 * 
 * PARAMETERS:
 *   - argc:    Argument count (number of command-line arguments)
 *   - argv:    Argument vector. argv[0] is program name
 *              Optional argv[1]: port number (default 8080)
 * 
 * RETURN VALUE:
 *   - 0: Success.      Server initialized, ran, and shut down cleanly.
 *   - EXIT_FAILURE:    Failure during initialization. Possible reasons:
 *      > Socket creation failed
 *      > setsockopt failed (SO_REUSEADDR)
 *      > Bind failed (port already in use)
 *      > Listen failed
 * 
 * COMMAND-LINE USAGE:
 *   ./myserver              - Run on default port 8080
 *   ./myserver 9000         - Run on port 9000
 *   ./myserver 8888         - Run on port 8888
 * 
 * BEHAVIOR (9 main steps):
 *   1. Initialize globals:
 *      - Calls initialize_server_globals() to set default config
 *      - Calls queue_init() to create request queue mutexes/conditions
 *      - Parses optional port argument (argv[1]) if provided
 *   2. Setup signal handling:
 *      - Creates sigaction structure
 *      - Sets sa_handler to handle_signal()
 *      - Registers handler for SIGINT (Ctrl+C) and SIGTERM
 *   3. Create server socket:
 *      - Calls socket(AF_INET, SOCK_STREAM, 0) for TCP socket
 *      - On failure: prints error, exits with EXIT_FAILURE
 *   4. Set socket options:
 *      - Sets SO_REUSEADDR to allow socket reuse after restart
 *      - Prevents "Address already in use" errors on restart
 *   5. Bind socket:
 *      - Creates sockaddr_in structure with port and INADDR_ANY
 *      - Binds socket to all interfaces (0.0.0.0) on configured port
 *   6. Listen on socket:
 *      - Sets socket to listening mode (passive socket)
 *      - Backlog of 10 (max pending connections in accept queue)
 *   7. Initialize thread pool:
 *      - Calls init_thread_pool() to create worker threads
 *      - Creates thread pool with DEFAULT_THREAD_POOL_SIZE workers
 *   8. Main accept loop:
 *      - While server_running (set by signal handler):
 *         a. Calls accept() to wait for incoming connection
 *         b. On accept failure: logs error, continues loop
 *         c. On success: enqueues socket to request_queue
 *      - Continues until signal received (server_running becomes 0)
 *   9. Cleanup and shutdown:
 *      - Calls clean_thread_pool() to gracefully exit all workers
 *      - Calls cleanup_server_globals() to free resources
 *      - Prints shutdown complete message
 *      - Returns 0
 * 
 * NOTES:
 *   - Default port is 8080, configurable via command line
 *   - Document root defaults to "./www" (defined in globals.h)
 *   - Thread pool size defaults to 10 (defined in globals.h)
 * 
 * ============================================================================
 */
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

    // listen on socket
    if (listen(global_server.config.server_socket, 10) < 0) {
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
