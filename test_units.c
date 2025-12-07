/* ============================================================================
 * COMP3659 Assignment 2 - Unit Tests for Web Server Components
 * ============================================================================
 * 
 * This file contains unit tests for individual functions in the web server.
 * Tests are run standalone (not as part of running server).
 *
 * Compile: gcc -Wall -Wextra -pthread -g -o test_units test_units.c queue.c globals.c -DTEST_MODE
 * Run:     ./test_units
 *
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include "globals.h"
#include "queue.h"

// ============================================================================
// TEST COUNTERS
// ============================================================================
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("  [PASS] %s\n", message); \
    } else { \
        tests_failed++; \
        printf("  [FAIL] %s\n", message); \
    } \
} while(0)

// ============================================================================
// TEST: parse_http_request function
// ============================================================================
// Forward declaration (from worker.c)
int parse_http_request(const char* request_buffer, char* filename, size_t filename_size);

void test_parse_http_request() {
    printf("\n=== Testing parse_http_request() ===\n");
    
    char filename[256];
    int result;
    
    // Test 1: Standard GET request
    result = parse_http_request("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", 
                                filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse standard GET request returns 0");
    TEST_ASSERT(strcmp(filename, "/index.html") == 0, "Extracted filename is /index.html");
    
    // Test 2: Root path request
    result = parse_http_request("GET / HTTP/1.1\r\n\r\n", filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse root path request returns 0");
    TEST_ASSERT(strcmp(filename, "/index.html") == 0, "Root path maps to /index.html");
    
    // Test 3: CSS file request
    result = parse_http_request("GET /style.css HTTP/1.1\r\n\r\n", filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse CSS file request returns 0");
    TEST_ASSERT(strcmp(filename, "/style.css") == 0, "Extracted filename is /style.css");
    
    // Test 4: Nested path
    result = parse_http_request("GET /images/logo.png HTTP/1.1\r\n\r\n", filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse nested path request returns 0");
    TEST_ASSERT(strcmp(filename, "/images/logo.png") == 0, "Extracted nested path correctly");
    
    // Test 5: POST request (should also parse)
    result = parse_http_request("POST /api/data HTTP/1.1\r\n\r\n", filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse POST request returns 0");
    TEST_ASSERT(strcmp(filename, "/api/data") == 0, "Extracted POST path correctly");
    
    // Test 6: Invalid request (no space)
    result = parse_http_request("INVALIDREQUEST", filename, sizeof(filename));
    TEST_ASSERT(result == -1, "Invalid request (no space) returns -1");
    
    // Test 7: Request with query string
    result = parse_http_request("GET /page.html?id=123 HTTP/1.1\r\n\r\n", filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse request with query string returns 0");
    TEST_ASSERT(strcmp(filename, "/page.html?id=123") == 0, "Query string preserved in path");
    
    // Test 8: Leading whitespace
    result = parse_http_request("  GET /test.html HTTP/1.1\r\n\r\n", filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse request with leading whitespace returns 0");
    TEST_ASSERT(strcmp(filename, "/test.html") == 0, "Leading whitespace handled");
    
    // Test 9: Multiple spaces between method and path
    result = parse_http_request("GET   /spaced.html HTTP/1.1\r\n\r\n", filename, sizeof(filename));
    TEST_ASSERT(result == 0, "Parse request with multiple spaces returns 0");
    TEST_ASSERT(strcmp(filename, "/spaced.html") == 0, "Multiple spaces handled");
}

// ============================================================================
// TEST: Queue Operations
// ============================================================================

void test_queue_init() {
    printf("\n=== Testing queue_init() ===\n");
    
    Shared_Queue queue;
    
    // Test 1: Initialize queue
    queue_init(&queue, 10);
    TEST_ASSERT(queue.head == NULL, "Queue head is NULL after init");
    TEST_ASSERT(queue.tail == NULL, "Queue tail is NULL after init");
    TEST_ASSERT(queue.queue_count == 0, "Queue count is 0 after init");
    TEST_ASSERT(queue.max_size == 10, "Queue max_size is set correctly");
    
    // Cleanup
    pthread_mutex_destroy(&queue.mutex);
    pthread_cond_destroy(&queue.not_empty);
    pthread_cond_destroy(&queue.not_full);
}

void test_enqueue_dequeue() {
    printf("\n=== Testing enqueue/dequeue operations ===\n");
    
    // Initialize globals for testing
    initialize_server_globals();
    queue_init(&global_server.request_queue, 10);
    
    // Test 1: Enqueue single request
    enqueue_request(&global_server.request_queue, 100);
    TEST_ASSERT(global_server.request_queue.queue_count == 1, "Queue count is 1 after enqueue");
    TEST_ASSERT(global_server.request_queue.head != NULL, "Queue head is not NULL after enqueue");
    TEST_ASSERT(global_server.request_queue.tail != NULL, "Queue tail is not NULL after enqueue");
    
    // Test 2: Dequeue the request
    Client_Request* req = dequeue_request(&global_server.request_queue);
    TEST_ASSERT(req != NULL, "Dequeued request is not NULL");
    TEST_ASSERT(req->client_socket == 100, "Dequeued request has correct socket");
    TEST_ASSERT(global_server.request_queue.queue_count == 0, "Queue count is 0 after dequeue");
    free(req);
    
    // Test 3: Enqueue multiple requests
    enqueue_request(&global_server.request_queue, 200);
    enqueue_request(&global_server.request_queue, 201);
    enqueue_request(&global_server.request_queue, 202);
    TEST_ASSERT(global_server.request_queue.queue_count == 3, "Queue count is 3 after 3 enqueues");
    
    // Test 4: FIFO order
    req = dequeue_request(&global_server.request_queue);
    TEST_ASSERT(req->client_socket == 200, "First dequeue returns first enqueued (FIFO)");
    free(req);
    
    req = dequeue_request(&global_server.request_queue);
    TEST_ASSERT(req->client_socket == 201, "Second dequeue returns second enqueued");
    free(req);
    
    req = dequeue_request(&global_server.request_queue);
    TEST_ASSERT(req->client_socket == 202, "Third dequeue returns third enqueued");
    free(req);
    
    TEST_ASSERT(global_server.request_queue.queue_count == 0, "Queue is empty after all dequeues");
    
    // Cleanup
    cleanup_server_globals();
}

// ============================================================================
// TEST: MIME Type Detection
// ============================================================================
// Forward declaration (from globals.c)
const char* get_mime_type(const char* filename);

void test_mime_types() {
    printf("\n=== Testing get_mime_type() ===\n");
    
    const char* mime;
    
    // Test HTML
    mime = get_mime_type("/index.html");
    TEST_ASSERT(strcmp(mime, "text/html") == 0, "HTML file returns text/html");
    
    // Test CSS
    mime = get_mime_type("/style.css");
    TEST_ASSERT(strcmp(mime, "text/css") == 0, "CSS file returns text/css");
    
    // Test JavaScript
    mime = get_mime_type("/app.js");
    TEST_ASSERT(strcmp(mime, "application/javascript") == 0, "JS file returns application/javascript");
    
    // Test PNG
    mime = get_mime_type("/image.png");
    TEST_ASSERT(strcmp(mime, "image/png") == 0, "PNG file returns image/png");
    
    // Test JPEG
    mime = get_mime_type("/photo.jpg");
    TEST_ASSERT(strcmp(mime, "image/jpeg") == 0, "JPG file returns image/jpeg");
    
    mime = get_mime_type("/photo.jpeg");
    TEST_ASSERT(strcmp(mime, "image/jpeg") == 0, "JPEG file returns image/jpeg");
    
    // Test GIF
    mime = get_mime_type("/animation.gif");
    TEST_ASSERT(strcmp(mime, "image/gif") == 0, "GIF file returns image/gif");
    
    // Test JSON
    mime = get_mime_type("/data.json");
    TEST_ASSERT(strcmp(mime, "application/json") == 0, "JSON file returns application/json");
    
    // Test TXT
    mime = get_mime_type("/readme.txt");
    TEST_ASSERT(strcmp(mime, "text/plain") == 0, "TXT file returns text/plain");
    
    // Test unknown extension (should return default)
    mime = get_mime_type("/file.xyz");
    TEST_ASSERT(mime != NULL, "Unknown extension returns non-NULL (default type)");
}

// ============================================================================
// TEST: Thread Safety (Basic)
// ============================================================================

#define NUM_PRODUCER_THREADS 5
#define NUM_REQUESTS_PER_THREAD 20

static int produced_count = 0;
static int consumed_count = 0;
static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;

void* producer_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < NUM_REQUESTS_PER_THREAD; i++) {
        int socket_id = thread_id * 1000 + i;
        enqueue_request(&global_server.request_queue, socket_id);
        
        pthread_mutex_lock(&test_mutex);
        produced_count++;
        pthread_mutex_unlock(&test_mutex);
    }
    
    return NULL;
}

void* consumer_thread(void* arg) {
    (void)arg; // unused
    
    while (1) {
        pthread_mutex_lock(&test_mutex);
        if (consumed_count >= NUM_PRODUCER_THREADS * NUM_REQUESTS_PER_THREAD) {
            pthread_mutex_unlock(&test_mutex);
            break;
        }
        pthread_mutex_unlock(&test_mutex);
        
        Client_Request* req = dequeue_request(&global_server.request_queue);
        if (req != NULL) {
            free(req);
            
            pthread_mutex_lock(&test_mutex);
            consumed_count++;
            pthread_mutex_unlock(&test_mutex);
        }
        
        // Check for shutdown
        if (global_server.config.shutdown_requested) break;
    }
    
    return NULL;
}

void test_thread_safety() {
    printf("\n=== Testing Thread Safety (Producer-Consumer) ===\n");
    
    // Initialize
    initialize_server_globals();
    queue_init(&global_server.request_queue, 50);
    produced_count = 0;
    consumed_count = 0;
    
    pthread_t producers[NUM_PRODUCER_THREADS];
    pthread_t consumer;
    int thread_ids[NUM_PRODUCER_THREADS];
    
    // Start consumer thread
    pthread_create(&consumer, NULL, consumer_thread, NULL);
    
    // Start producer threads
    for (int i = 0; i < NUM_PRODUCER_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&producers[i], NULL, producer_thread, &thread_ids[i]);
    }
    
    // Wait for producers to finish
    for (int i = 0; i < NUM_PRODUCER_THREADS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Wait a bit for consumer to catch up
    sleep(1);
    
    // Signal shutdown and wake up consumer if waiting
    global_server.config.shutdown_requested = 1;
    pthread_cond_broadcast(&global_server.request_queue.not_empty);
    
    pthread_join(consumer, NULL);
    
    int expected = NUM_PRODUCER_THREADS * NUM_REQUESTS_PER_THREAD;
    TEST_ASSERT(produced_count == expected, "All requests were produced");
    TEST_ASSERT(consumed_count == expected, "All requests were consumed");
    TEST_ASSERT(global_server.request_queue.queue_count == 0, "Queue is empty after test");
    
    printf("  Produced: %d, Consumed: %d\n", produced_count, consumed_count);
    
    cleanup_server_globals();
}

// ============================================================================
// TEST: Server Configuration
// ============================================================================

void test_server_config() {
    printf("\n=== Testing Server Configuration ===\n");
    
    initialize_server_globals();
    
    TEST_ASSERT(global_server.config.port == DEFAULT_PORT, "Default port is set correctly");
    TEST_ASSERT(global_server.config.thread_pool_size == DEFAULT_THREAD_POOL_SIZE, 
                "Default thread pool size is set correctly");
    TEST_ASSERT(strlen(global_server.config.document_root) > 0, 
                "Document root is set");
    TEST_ASSERT(strcmp(global_server.config.document_root, DEFAULT_DOCUMENT_ROOT) == 0,
                "Document root is ./www");
    TEST_ASSERT(global_server.config.shutdown_requested == 0, 
                "Shutdown not requested initially");
    
    cleanup_server_globals();
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("============================================================\n");
    printf("COMP3659 Web Server - Unit Test Suite\n");
    printf("============================================================\n");
    
    // Run all tests
    test_parse_http_request();
    test_queue_init();
    test_enqueue_dequeue();
    test_mime_types();
    test_server_config();
    test_thread_safety();
    
    // Print summary
    printf("\n============================================================\n");
    printf("TEST SUMMARY\n");
    printf("============================================================\n");
    printf("Tests Run:    %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("============================================================\n");
    
    if (tests_failed == 0) {
        printf("ALL TESTS PASSED!\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED!\n");
        return 1;
    }
}
