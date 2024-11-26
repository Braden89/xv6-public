#include "defs.h"
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "buddy.h"

struct free_area free_areas[MAX_ORDER];  // Define here
struct spinlock buddy_lock;              // Define here

void buddyinit(void) {
    initlock(&buddy_lock, "buddy_lock");
    for (int i = 0; i < MAX_ORDER; i++) {
        INIT_LIST_HEAD(&free_areas[i].free_list);
    }
}

void add_free_block(struct free_block *block, int order) {
    struct list_head *head = &free_areas[order].free_list;
    list_add(&block->list, head);
}

struct free_block *remove_free_block(int order) {
    struct list_head *head = &free_areas[order].free_list;

    if (list_empty(head)) {
        return NULL;  // No free block available
    }

    struct list_head *entry = head->next;
    list_del(entry);

    // Cast back to the free_block structure
    return (struct free_block *)((char *)entry - offsetof(struct free_block, list));
}

void *buddy_alloc(uint64_t size) {
     if (size > (1 << (MAX_ORDER - 1))) {  // If size exceeds maximum buddy block size
        return kalloc();                 // Fall back to kalloc
     }
    int order = get_order(size);
    if (order >= MAX_ORDER)
        return 0;

    spin_lock(&buddy_lock);

    // Find the smallest free block that can accommodate the size
    for (int i = order; i < MAX_ORDER; i++) {
        if (!list_empty(&free_areas[i].free_list)) {
            struct free_block *block = list_first_entry(&free_areas[i].free_list, struct free_block, list);
            list_del(&block->list);

            // Split blocks if needed
            while (i > order) {
                i--;
                split_block(block, i);
            }

            spin_unlock(&buddy_lock);
            return (void *)block;
        }
    }

    spin_unlock(&buddy_lock);
    return NULL;  // No suitable block found
}

void buddy_free(void *addr, uint64_t size) {
    int order = get_order(size);
    struct free_block *block = (struct free_block *)addr;

    spin_lock(&buddy_lock);

    while (order < MAX_ORDER - 1) {
        struct free_block *buddy = find_buddy(block, order);

        if (!is_buddy_free(buddy)) {
            break;
        }

        // Remove buddy from the free list and merge
        list_del(&buddy->list);
        block = merge_blocks(block, buddy);
        order++;
    }

    list_add(&block->list, &free_areas[order].free_list);

    spin_unlock(&buddy_lock);
}

int is_block_free(void *addr, int size) {
    int order = get_order(size);
    struct list_head *pos;
    list_for_each(pos, &free_areas[order].free_list) {
        struct free_block *block = list_entry(pos, struct free_block, list);
        if (block == addr) {
            return 1;  // Block is free
        }
    }
    return 0;  // Block is used
}

void buddy_print_block(void *addr, int size, int depth) {
    // Print indentation for the current depth
    for (int i = 0; i < depth; i++) {
        cprintf("   ");
    }

    // Determine if the block is free or used
    if (is_block_free(addr, size)) {
        cprintf("┌──── free (%d)\n", size);
    } else {
        cprintf("┌──── used (%d)\n", size);
    }

    // If the block is split, recurse into its children
    if (size > MIN_BLOCK_ORDER) {
        int half_size = size / 2;
        void *left_child = addr;
        void *right_child = addr + half_size;

        // Print the left child
        for (int i = 0; i < depth; i++) {
            cprintf("   ");
        }
        cprintf("───┤\n");
        buddy_print_block(left_child, half_size, depth + 1);

        // Print the right child
        for (int i = 0; i < depth; i++) {
            cprintf("   ");
        }
        cprintf("───┤\n");
        buddy_print_block(right_child, half_size, depth + 1);
    }
}


void buddy_print(void *addr) {
    // Validate input
    if (addr == NULL) {
        cprintf("Invalid address.\n");
        return;
    }

    // Determine the size of the block (4096 bytes)
    int block_size = 4096;

    // Start recursive printing
    buddy_print_block(addr, block_size, 0);
}



void
buddy_test(void)
{
    printf("Starting buddy test\n");

    printf("\nallocating 1024-byte block\n");
    void *e = buddy_alloc(1000);
    buddy_print(e);

    printf("\nallocating 128-byte block\n");
    void *c = buddy_alloc(112);
    buddy_print(c);

    printf("\nallocating 32-byte block\n");
    void *a = buddy_alloc(16);
    buddy_print(a);

    printf("\nfreeing 1024-byte block\n");
    buddy_free(e, 1000);
    buddy_print(a);

    printf("\nallocating 128-byte block\n");
    void *b = buddy_alloc(112);
    buddy_print(b);

    printf("\nfreeing 32-byte block\n");
    buddy_free(a, 16);
    buddy_print(b);

    printf("\nfreeing first 128-byte block\n");
    buddy_free(c, 112);
    buddy_print(b);

    printf("\nallocating 2048-byte block\n");
    void *d = buddy_alloc(2000);
    buddy_print(d);

    printf("\nfreeing other 128-byte block\n");
    buddy_free(b, 112);
    buddy_print(d);

    printf("\nfreeing 2048-byte block\n");
    buddy_free(d, 112);
}