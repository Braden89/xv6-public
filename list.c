#include "list.h"

void list_add(struct list_head *new_node, struct list_head *head) {
    new_node->next = head->next;
    new_node->prev = head;
    head->next->prev = new_node;
    head->next = new_node;
}


void list_del(struct list_head *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;

    // Optional: Nullify the removed node's pointers (debugging purposes)
    entry->next = entry->prev = 0;
}


int list_empty(const struct list_head *head) {
    return head->next == head;
}