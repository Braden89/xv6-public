#ifndef BUDDY_H
#define BUDDY_H

#include "types.h"
#include "spinlock.h"
#include "list.h"
#include <stdint.h>

#define MAX_ORDER 11 // Maximum block size = 2^(MAX_ORDER-1)
#define MIN_BLOCK_ORDER 5  // Smallest block size is 32 bytes (2^5 = 32)

struct free_block {
    struct list_head list;  // Linked list node
    uint64_t size;          // Size of the block
    uint64_t magic;         // Magic number for validity check
};

struct free_area {
    struct list_head free_list;  // Free list for this size
};

extern struct free_area free_areas[MAX_ORDER];

extern struct spinlick buddy_lock;

#define list_first_entry(ptr, type, member) \
    ((type *)((char *)(ptr)->next - offsetof(type, member)))

void buddyinit(void);
void *buddy_alloc(uint64_t size);
void buddy_free(void *addr, uint64_t size);
void buddy_print(void);
void buddy_test(void);



#endif