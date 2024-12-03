#include "types.h" // Include types for uint64

// Structure defining a memory block in the free list
struct block {
    struct block *next;   // Pointer to the next block in the free list
    unsigned int size;    // Size of the memory block in bytes
};

// Initialize the free list with a memory pool
void init_free_list(void *base, unsigned int total_size);

// Allocate a memory block of the requested size
void *allocate_from_free_list(unsigned int size);

// Free a memory block and return it to the free list
void free_block(void *ptr, unsigned int size);

// Add a memory block to the free list (used internally but exposed if needed)
void add_to_free_list(struct block *new_block);


#define MIN_BLOCK_SIZE 32
#define MAX_BLOCK_SIZE 4096
#define NUM_FREE_LISTS 7
#define USED_MAGIC 0xABCDEF1234567890
#define FREE_MAGIC 0xDEADBEEFCAFEBABE

struct block_header {
    uint magic;       // Magic number (used or free)
    uint size;        // Size of the block
    struct block_header *next; // Pointer to next block in free list
};

struct free_list {
    struct block_header *head;
};

extern struct spinlock buddy_lock;