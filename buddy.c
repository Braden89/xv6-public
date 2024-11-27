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
        return 0;  // No free block available
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
    return 0;  // No suitable block found
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


void buddy_print(void){
    cprintf("Buddy Allocator Structure:\n");

    for (int order = 0; order < MAX_ORDER; order++) {
        cprintf("Order %d (Block size: %d bytes):\n", order, 1 << (order + MIN_BLOCK_ORDER));

        struct list_head *pos;
        list_for_each(pos, &free_areas[order].free_list) {
            struct free_block *block = list_entry(pos, struct free_block, list);
            cprintf("  Free block: Address = %p, Size = %d bytes\n", block, block->size);
        }
    }
}