#pragma once

#include <stddef.h>

typedef struct list_handle
{
    struct list_handle *next;
} list_handle_t;

#define null_list_handle \
    {                    \
        NULL             \
    }

#define list_insert_after(base, insertee) \
    (insertee)->next = (base)->next;      \
    (base)->next = (insertee)

#define list_entry(ptr, type, field) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->field)))

#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != NULL; (pos) = (pos)->next)
