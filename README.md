# Multithreaded Web Server (COMP3659 Assignment 2)

This is a multithreaded web server implemented in C using POSIX threads and sockets. It serves static files from a document root directory.

## Project Structure

- **myserver.c**: Main entry point. Initializes the server, thread pool, and handles the main accept loop.
- **globals.h/c**: Defines global data structures (`Web_Server`, `Client_Request`, etc.) and initializes global state.
- **worker.h/c**: Implements the thread pool and worker thread logic (request processing).
- **queue.h/c**: Implements a thread-safe producer-consumer queue for client requests.
- **files.h/c**: Handles file I/O, MIME type detection, and HTTP response generation.
- **makefile**: Build script.

## Compilation

To compile the server, run:

```bash
make
```

This will produce an executable named `myserver`.

## Usage

Run the server with an optional port number (default is 8080):

```bash
./myserver [port]
```

Example:
```bash
./myserver 9000
```

## Features

- **Thread Pool**: A fixed number of worker threads process requests concurrently.
- **Synchronization**: Uses mutexes and condition variables to manage the request queue safely.
- **Static File Serving**: Serves HTML, CSS, JS, images, etc.
- **Error Handling**: Returns 404 for missing files and 500 for internal errors.
- **Graceful Shutdown**: Handles SIGINT (Ctrl+C) to clean up resources and exit.

## Configuration

Default configuration values (port, thread pool size, etc.) can be modified in `globals.h`.
