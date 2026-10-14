#ifndef FVWM_STACK_WINDOW_BUFFER_H
#define FVWM_STACK_WINDOW_BUFFER_H

#include "stack_window_list.h"

typedef struct
{
	Window *windows;
	size_t capacity;
} fvwm_stack_window_buffer;

/* Prepare before planning/link/layer mutation, with no X work/callbacks. The
 * current stack ring bounds the selected subtree's packed window slots. A new
 * or finished owner is required; refusal leaves an empty initialized owner. */
Bool fvwm_stack_window_buffer_prepare(
	fvwm_stack_window_buffer *buffer, const FvwmWindow *ring, size_t padding);

/* Allocate an already-counted populated prefix with optional spare slots.
 * Count remains representable by Xlib; padding and bytes are checked before
 * allocation. The same new/finished-owner contract applies. */
Bool fvwm_stack_window_buffer_allocate(
	fvwm_stack_window_buffer *buffer, size_t count, size_t padding);

/* The caller owns storage through synchronous borrowed use. Idempotent finish
 * clears all fields; no managed-record references or global state are kept. */
void fvwm_stack_window_buffer_finish(fvwm_stack_window_buffer *buffer);

#endif
