/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include "stack_window_buffer.h"

Bool fvwm_stack_window_buffer_prepare(
	fvwm_stack_window_buffer *buffer, const FvwmWindow *ring, size_t padding)
{
	size_t count = 0;

	buffer->windows = NULL;
	buffer->capacity = 0;
	for (const FvwmWindow *window = ring->stack_next; window != ring;
	     window = window->stack_next)
	{
		if (!fvwm_stack_window_count_add(&count, window))
		{
			return False;
		}
	}
	return fvwm_stack_window_buffer_allocate(buffer, count, padding);
}

Bool fvwm_stack_window_buffer_allocate(
	fvwm_stack_window_buffer *buffer, size_t count, size_t padding)
{
	buffer->windows = NULL;
	buffer->capacity = 0;
	if (count == 0 || count > INT_MAX || padding > SIZE_MAX - count ||
	    count + padding > SIZE_MAX / sizeof(Window))
	{
		return False;
	}
	buffer->windows = malloc((count + padding) * sizeof(Window));
	if (buffer->windows == NULL)
	{
		return False;
	}
	buffer->capacity = count + padding;
	return True;
}

void fvwm_stack_window_buffer_finish(fvwm_stack_window_buffer *buffer)
{
	free(buffer->windows);
	buffer->windows = NULL;
	buffer->capacity = 0;
}
