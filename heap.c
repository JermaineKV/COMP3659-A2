#include "heap.h"

#define HEAP_SIZE 10000  // Total heap size: 10KB for storage

static char heap[HEAP_SIZE];    // Fixed-size memory pool for allocations
static char *freep = heap;      // Pointer to next available memory location

/*
 * ========================================================================
 * FUNCTION: alloc
 * ========================================================================
 * 
 * PURPOSE:
 * - Allocates a block of memory from the custom heap
 * - Provides linear allocation without fragmentation
 * - Performs bounds checking to prevent buffer overflow
 * 
 * PARAMETERS:
 * - size: Number of bytes to allocate (unsigned int)
 * 
 * RETURN VALUES:
 * - Pointer to allocated memory block (success)
 * - NULL (0): Insufficient heap space remaining (failure)
 * 
 * ========================================================================
 */
char *alloc(unsigned int size)
{
    // check if requested size fits in remaining heap space
    if (heap + HEAP_SIZE - freep >= size) {
        freep += size;              // advance free pointer by allocation size
        return freep - size;        // return pointer to start of allocated block
    }
    return 0;                       // insufficient space - return NULL
}

/*
 * ========================================================================
 * FUNCTION: free_all
 * ========================================================================
 * 
 * PURPOSE:
 * - Deallocates all previously allocated memory in one operation
 * - Resets heap to initial state for next command processing
 * - Prevents memory leaks between shell commands
 * 
 * PARAMETERS:
 * - None
 * 
 * RETURN VALUES:
 * - None (void function)
 * 
 * ========================================================================
 */
void free_all()
{
    freep = heap;                   // reset free pointer to heap beginning
    // Note: No need to clear memory contents - will be overwritten
}
