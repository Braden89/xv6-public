#include "spinlock.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "types.h"
#include "buddy.h"


// Global pointer to the start of the free list
static struct block *free_list = 0;

// Spinlock for thread-safe operations
struct spinlock buddy_lock;

// Initialize the free list with a memory pool
void init_free_list(void *base, unsigned int total_size) {
    free_list = (struct block *)base;  // Cast base address to a block pointer
    free_list->next = 0;               // Initially, no next block
    free_list->size = total_size;      // Set the size of the memory pool
}

// Add a memory block to the free list
void add_to_free_list(struct block *new_block) {
    struct block *current = free_list;
    struct block *prev = 0;

    // Find the correct position to insert the block (sorted by address)
    while (current && current < new_block) {
        prev = current;
        current = current->next;
    }

    // Link the new block into the list
    new_block->next = current;
    if (prev) {
        prev->next = new_block;
    } else {
        free_list = new_block;
    }

    // Coalesce with the next block if they are adjacent
    if (new_block->next &&
        (char *)new_block + new_block->size == (char *)new_block->next) {
        new_block->size += new_block->next->size;
        new_block->next = new_block->next->next;
    }

    // Coalesce with the previous block if they are adjacent
    if (prev &&
        (char *)prev + prev->size == (char *)new_block) {
        prev->size += new_block->size;
        prev->next = new_block->next;
    }
}

// Allocate a memory block from the free list
void *allocate_from_free_list(unsigned int size) {
    struct block *current = free_list;
    struct block *prev = 0;

    // Align size to the nearest multiple of struct block size
    size = (size + sizeof(struct block) - 1) & ~(sizeof(struct block) - 1);

    while (current) {
        if (current->size >= size) {
            // If the block is larger, split it
            if (current->size > size + sizeof(struct block)) {
                struct block *split_block = 
                    (struct block *)((char *)current + size);
                split_block->size = current->size - size;
                split_block->next = current->next;

                current->size = size;

                if (prev) {
                    prev->next = split_block;
                } else {
                    free_list = split_block;
                }
            } else {
                // Use the whole block
                if (prev) {
                    prev->next = current->next;
                } else {
                    free_list = current->next;
                }
            }
            return (void *)((char *)current + sizeof(struct block));
        }
        prev = current;
        current = current->next;
    }

    // No suitable block found
    return 0;
}

// Free a memory block and return it to the free list
void free_block(void *ptr, unsigned int size) {
    struct block *block_to_free = (struct block *)((char *)ptr - sizeof(struct block));
    block_to_free->size = size;

    add_to_free_list(block_to_free);
}


// Helper function to calculate the index in the free list array
static int size_to_index(uint size) {
    int index = 0;
    while ((1UL << (index + 5)) < size) {
        index++;
    }
    return index;
}

// Free lists for each size class (one for each power of two from 32 to 4096 bytes)
static struct free_list free_lists[NUM_FREE_LISTS];

// Spinlock for synchronization
struct spinlock buddy_lock;

// Helper function: Map size to free list index
static int size_to_index(uint size) {
    int index = 0;
    while ((1UL << (index + 5)) < size) {
        index++;
    }
    return index;
}

// Helper function: Align size to nearest power of 2
static uint align_size(uint size) {
    uint aligned = MIN_BLOCK_SIZE;
    while (aligned < size + sizeof(struct block_header)) {
        aligned <<= 1;
    }
    return aligned;
}

// Initialize the buddy allocator
void buddyinit(void) {
    initlock(&buddy_lock, "buddy_lock");

    // Initialize all free lists to empty
    for (int i = 0; i < NUM_FREE_LISTS; i++) {
        free_lists[i].head = 0;
    }
}

void *buddy_alloc(uint length) {
    if (length == 0 || length > MAX_BLOCK_SIZE - sizeof(struct block_header)) {
        return 0; // Invalid size
    }

    acquire(&buddy_lock);

    // Align requested size to the nearest power of 2, including header size
    uint size = align_size(length + sizeof(struct block_header)); // Include header size
    int index = size_to_index(size);

    // Find a suitable block in the free lists
    for (int i = index; i < NUM_FREE_LISTS; i++) {
        if (free_lists[i].head) {
            // Remove the first block from the free list
            struct block_header *block = free_lists[i].head;
            free_lists[i].head = block->next;

            // Split the block into smaller blocks if necessary
            while (i > index) {
                i--;
                uint split_size = block->size >> 1; // Split block into halves
                struct block_header *split_block = (struct block_header *)((char *)block + split_size);

                // Initialize the split block
                split_block->magic = FREE_MAGIC;
                split_block->size = split_size;
                split_block->next = free_lists[i].head;

                // Add the split block to the appropriate free list
                free_lists[i].head = split_block;

                // Update the size of the block to be returned
                block->size = split_size;
            }

            // Mark the block as used
            block->magic = USED_MAGIC;

            void *allocated_memory = (void *)((char *)block + sizeof(struct block_header));
            assert(((uint)allocated_memory % block->size) == 0); // Check alignment
            cprintf("Allocated memory at %p, size: %u\n", allocated_memory, block->size);

            release(&buddy_lock);

            release(&buddy_lock);

            // Return a pointer to the usable memory (after the header)
            return (void *)((char *)block + sizeof(struct block_header));
        }
    }

    // No suitable block found, allocate a new 4096-byte page using kalloc
    char *new_block = kalloc();
    if (!new_block) {
        release(&buddy_lock);
        return 0; // Out of memory
    }

    // Initialize the new block as a 4096-byte block
    struct block_header *header = (struct block_header *)new_block;
    header->magic = FREE_MAGIC;
    header->size = MAX_BLOCK_SIZE;
    header->next = 0;

    // Add the new block to the largest free list
    free_lists[NUM_FREE_LISTS - 1].head = header;

    release(&buddy_lock);

    // Retry allocation with the new block added to the free lists
    return buddy_alloc(length);
}

// Free a block
void buddy_free(void *ptr) {
    if (!ptr) {
        return;
    }

    struct block_header *block = (struct block_header *)((char *)ptr - sizeof(struct block_header));
    if (block->magic != USED_MAGIC || block->size < MIN_BLOCK_SIZE || block->size > MAX_BLOCK_SIZE) {
        panic("Invalid block to free");
    }

    acquire(&buddy_lock);

    block->magic = FREE_MAGIC;
    int index = size_to_index(block->size);

    // Coalesce blocks
    while (block->size < MAX_BLOCK_SIZE) {
        uint buddy_address = ((uint)block) ^ block->size;
        struct block_header **current = &free_lists[index].head;
        struct block_header *prev = 0;
        struct block_header *buddy = 0;

        while (*current) {
            if ((uint)*current == buddy_address) {
                buddy = *current;
                if (prev) {
                    prev->next = buddy->next;
                } else {
                    free_lists[index].head = buddy->next;
                }
                break;
            }
            prev = *current;
            current = &(*current)->next;
        }

        if (!buddy) {
            break;
        }

        if ((char *)buddy < (char *)block) {
            block = buddy;
        }

        block->size <<= 1;
        index++;
    }

    block->next = free_lists[index].head;
    free_lists[index].head = block;

    release(&buddy_lock);
}
/*
// Print the buddy allocator structure
void buddy_print(void *ptr) {
    struct block_header *block = (struct block_header *)((char *)ptr - sizeof(struct block_header));
    cprintf("Buddy block at %p, size: %lu, magic: %lx\n", block, block->size, block->magic);

    // Add a recursive function here to traverse and print the tree structure
    // TODO: Implement detailed recursive tree printing
}
*/
// Test the buddy allocator
void buddy_test(void) {
    cprintf("Starting buddy allocator test...\n");

    void *block1 = buddy_alloc(64);
    cprintf("Allocated block1 at %p\n", block1);

    void *block2 = buddy_alloc(128);
    cprintf("Allocated block2 at %p\n", block2);

    buddy_free(block1);
    cprintf("Freed block1\n");

    buddy_free(block2);
    cprintf("Freed block2\n");

    //buddy_print(block2);

    cprintf("Buddy allocator test completed.\n");
}
