/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fvwm/stack_window_buffer.h"

static Bool refuse;
static unsigned attempts;
static void *owned;
void *__real_malloc(size_t size);
void __real_free(void *pointer);

void *__wrap_malloc(size_t size)
{
	assert(owned == NULL);
	attempts++;
	owned = refuse ? NULL : __real_malloc(size);
	return owned;
}

void __wrap_free(void *pointer)
{
	if (pointer != NULL)
	{
		assert(owned == pointer);
		owned = NULL;
	}
	__real_free(pointer);
}

static void test_scope_and_refusal(void)
{
	FvwmWindow ring = { 0 }, windows[3] = { 0 }, original[3];
	fvwm_stack_window_buffer buffer;

	ring.stack_next = windows;
	ring.stack_prev = windows + 2;
	for (unsigned i = 0; i < 3; i++)
	{
		FvwmWindow *window = windows + i;

		window->stack_prev = i == 0 ? &ring : window - 1;
		window->stack_next = i == 2 ? &ring : window + 1;
		FW_W_FRAME(window) = 10 + i;
		window->layer = 4;
		window->scratch.i = 2000 + i;
	}
	SET_ICONIFIED(windows, 1);
	FW_W_ICON_TITLE(windows) = 20;
	FW_W_ICON_PIXMAP(windows) = 21;
	memcpy(original, windows, sizeof(windows));
	refuse = True;
	assert(!fvwm_stack_window_buffer_prepare(&buffer, &ring, 3));
	assert(attempts == 1 && buffer.windows == NULL && buffer.capacity == 0);
	assert(memcmp(original, windows, sizeof(windows)) == 0);
	fvwm_stack_window_buffer_finish(&buffer);
	fvwm_stack_window_buffer_finish(&buffer);
	assert(owned == NULL);
	refuse = False;
	assert(fvwm_stack_window_buffer_prepare(&buffer, &ring, 3));
	assert(buffer.windows != NULL && buffer.capacity == 8);
	assert(attempts == 2 && memcmp(original, windows, sizeof(windows)) == 0);
	/* Borrowed use submits the selected subset, never the capacity itself. */
	size_t used = 0;
	assert(fvwm_stack_window_append(buffer.windows, buffer.capacity,
		&used, windows));
	assert(fvwm_stack_window_append(buffer.windows, buffer.capacity,
		&used, windows + 2));
	const Window expected[] = { 10, 20, 21, 12 };
	assert(used == 4 && memcmp(buffer.windows, expected, sizeof(expected)) == 0);
	fvwm_stack_window_buffer_finish(&buffer);
	assert(buffer.windows == NULL && buffer.capacity == 0 && owned == NULL);
	/* Retry after a finished successful owner remains supported. */
	assert(fvwm_stack_window_buffer_prepare(&buffer, &ring, 0));
	assert(buffer.capacity == 5);
	fvwm_stack_window_buffer_finish(&buffer);
}

static void test_empty_and_padding_limits(void)
{
	FvwmWindow ring = { 0 }, window = { 0 };
	fvwm_stack_window_buffer buffer;
	unsigned before = attempts;

	ring.stack_next = ring.stack_prev = &ring;
	assert(!fvwm_stack_window_buffer_prepare(&buffer, &ring, 3));
	assert(buffer.windows == NULL && buffer.capacity == 0 && attempts == before);
	ring.stack_next = ring.stack_prev = &window;
	window.stack_next = window.stack_prev = &ring;
	for (unsigned test = 0; test < 2; test++)
	{
		size_t padding = test == 0 ? SIZE_MAX : SIZE_MAX / sizeof(Window);

		assert(!fvwm_stack_window_buffer_prepare(&buffer, &ring, padding));
		assert(buffer.windows == NULL && buffer.capacity == 0);
		assert(attempts == before);
		fvwm_stack_window_buffer_finish(&buffer);
	}
	assert(fvwm_stack_window_buffer_prepare(&buffer, &ring, 0));
	assert(buffer.capacity == 1 && attempts == before + 1);
	fvwm_stack_window_buffer_finish(&buffer);
}

static void test_counted_prefix_and_refusal(void)
{
	fvwm_stack_window_buffer buffer;
	unsigned before = attempts;
	const size_t invalid[] = { 0, (size_t)INT_MAX + 1, SIZE_MAX };

	for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(!fvwm_stack_window_buffer_allocate(&buffer, invalid[i], 0));
		assert(buffer.windows == NULL && buffer.capacity == 0);
		assert(attempts == before);
		fvwm_stack_window_buffer_finish(&buffer);
	}
	refuse = True;
	assert(!fvwm_stack_window_buffer_allocate(&buffer, 2, 0));
	assert(buffer.windows == NULL && buffer.capacity == 0);
	assert(attempts == before + 1 && owned == NULL);
	refuse = False;
	assert(fvwm_stack_window_buffer_allocate(&buffer, 2, 0));
	assert(buffer.capacity == 2 && attempts == before + 2);
	fvwm_stack_window_buffer_finish(&buffer);
	/* This constructor also supplies Layer's existing spare slots. */
	assert(fvwm_stack_window_buffer_allocate(&buffer, 2, 3));
	assert(buffer.capacity == 5);
	fvwm_stack_window_buffer_finish(&buffer);
	before = attempts;
	assert(!fvwm_stack_window_buffer_allocate(&buffer, 1, SIZE_MAX));
	assert(!fvwm_stack_window_buffer_allocate(
		&buffer, 1, SIZE_MAX / sizeof(Window)));
	assert(attempts == before && buffer.windows == NULL);
}

int main(void)
{
	test_scope_and_refusal();
	test_empty_and_padding_limits();
	test_counted_prefix_and_refusal();
	return 0;
}
