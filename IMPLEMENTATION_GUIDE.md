# Multithreaded Web Server Implementation Guide

## Overview
This document details the implementation of the COMP3659 multi-threaded web server. The project uses a **Thread Pool** architecture with a **Producer-Consumer Queue** to handle concurrent HTTP requests efficiently.

## Architecture Components

### 1. **myserver.c** (Main Entry Point)
- **Responsibility**: Server initialization, socket setup, and the main connection acceptance loop.
- **Key Operations**:
    - Initializes global data structures (`initialize_server_globals`).
    - Sets up signal handling for graceful shutdown (`SIGINT`).
    - Creates, binds, and listens on the server socket.
    - Initializes the thread pool (`init_thread_pool`).
    - **Main Loop**: Accepts new client connections and enqueues them (`enqueue_request`) for worker threads to process.
    - **Cleanup**: Frees resources and joins threads upon shutdown.

### 2. **globals.h** / **globals.c** (Global State)
- **Responsibility**: Centralized definition of data structures and global variables to prevent circular dependencies.
- **Key Structures**:
    - `Web_Server`: Holds config, stats, queue, and thread pool.
    - `Client_Request`: Linked list node representing a client connection.
    - `Shared_Queue`: Thread-safe queue structure with mutex and condition variables.
    - `Thread_Pool`: Manages the array of worker threads.
    - `Worker_Thread`: Individual worker state.
- **Key Functions**:
    - `initialize_server_globals()`: Sets default values.
    - `cleanup_server_globals()`: Frees global resources.
    - `get_mime_type()`: Returns MIME type string based on file extension.

### 3. **worker.h** / **worker.c** (Thread Pool & Logic)
- **Responsibility**: Manages the lifecycle of worker threads and the request processing logic.
- **Key Functions**:
    - `init_thread_pool()`: Spawns `N` worker threads.
    - `worker_function()`: The main loop for each thread.
        1. Dequeues a request (blocks if empty).
        2. Sets socket timeouts to prevent hanging.
        3. Reads the HTTP request from the socket.
        4. Parses the request line to extract the filename (`parse_http_request`).
        5. Calls `serve_file` to handle the response.
        6. Closes the socket and updates stats.
    - `clean_thread_pool()`: Signals threads to exit and joins them.

### 4. **queue.h** / **queue.c** (Task Queue)
- **Responsibility**: Implements a thread-safe FIFO queue for client sockets.
- **Synchronization**:
    - Uses `pthread_mutex_t` to protect queue operations.
    - Uses `pthread_cond_t` (`not_empty`) to wake up sleeping workers when a new request arrives.
    - Uses `pthread_cond_t` (`not_full`) to block the main thread if the queue is full (backpressure).
- **Key Functions**:
    - `queue_init()`: Initializes mutexes and condition variables.
    - `enqueue_request()`: Adds a request to the tail.
    - `dequeue_request()`: Removes a request from the head (blocking).

### 5. **files.h** / **files.c** (File I/O & HTTP)
- **Responsibility**: Handles file system operations and constructs HTTP responses.
- **Key Functions**:
    - `read_file()`: Reads a file into a dynamically allocated buffer.
    - `serve_file()`: Orchestrates the response:
        1. Checks if file exists.
        2. Gets MIME type.
        3. Sends HTTP 200 OK headers.
        4. Sends file content.
    - `send_http_error()`: Sends HTTP error responses (e.g., 404 Not Found, 500 Internal Server Error) with a simple HTML body.

## Request Processing Flow

1.  **Connection**: Client connects to server port.
2.  **Accept**: Main thread accepts connection, getting a `client_socket`.
3.  **Enqueue**: Main thread calls `enqueue_request(client_socket)`.
4.  **Wake Up**: A sleeping worker thread is signaled via `pthread_cond_signal`.
5.  **Dequeue**: Worker thread wakes up, locks mutex, and removes the request from the queue.
6.  **Process**:
    *   Worker reads data from `client_socket`.
    *   Parses "GET /filename HTTP/1.1".
    *   Resolves path (e.g., `./www/filename`).
7.  **Serve**:
    *   If file exists: Sends headers + content.
    *   If missing: Sends 404 Error.
8.  **Finish**: Worker closes socket and returns to wait for next request.

## Build & Run

**Compile:**
```bash
make
```

**Run:**
```bash
./myserver [port]
# Example: ./myserver 8080
```

**Stop:**
Press `Ctrl+C` to trigger graceful shutdown.
