// global data we may want to highlight
#ifndef GLOBALS_H
#define GLOBALS_H

#include <semaphore.h>

struct WebServer {
    // server settings
    int port;

    // threads
    sem_t sem;
};

#endif