#ifndef LIST_H
#define LIST_H

struct list_head {
    struct list_head *next, *prev;
};

// Macro to initialize a list head
#define INIT_LIST_HEAD(ptr)       \
    do {                          \
        (ptr)->next = (ptr);      \
        (ptr)->prev = (ptr);      \
    } while (0)

#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Function prototypes
void list_add(struct list_head *new_node, struct list_head *head);
void list_del(struct list_head *entry);
int list_empty(const struct list_head *head);





#endif