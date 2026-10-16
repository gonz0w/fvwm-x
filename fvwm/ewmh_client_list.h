#ifndef FVWM_EWMH_CLIENT_LIST_H
#define FVWM_EWMH_CLIENT_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <X11/Xlib.h>

enum { EWMH_CLIENT_LIST_INLINE = 64 };

/* Operation-owned X11 window IDs; never managed window records. */
typedef struct
{
	Window local[EWMH_CLIENT_LIST_INLINE];
	Window *windows;
	size_t count;
	size_t capacity;
} ewmh_client_list;

bool ewmh_client_list_grow_capacity(size_t capacity, size_t *grown);
void ewmh_client_list_init(ewmh_client_list *list);
/* Refusal preserves the existing populated prefix and ownership. */
bool ewmh_client_list_append(ewmh_client_list *list, Window window);
void ewmh_client_list_finish(ewmh_client_list *list);

#endif
