#include <stdio.h>

// main.c program file

/*
Pseudocode:

1. initialize whatever we need (server, queues for requests, etc.)
2. server set up (creating sockets, binding, listening)
3. threads
4. signal handling 

5. main loop/main thread
accept connections
dispatch requests to worker threads

6. cleanup and kill threads
*/