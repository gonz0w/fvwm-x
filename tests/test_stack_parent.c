/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "fvwm/stack_parent.h"

static Bool refuse_allocation;
static unsigned allocations;
static void *owned;
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);

void *__wrap_calloc(size_t count, size_t size)
{
	allocations++;
	assert(owned == NULL);
	owned = refuse_allocation ? NULL : __real_calloc(count, size);
	return owned;
}

void __wrap_free(void *pointer)
{
	if (pointer != NULL)
	{
		assert(pointer == owned);
		owned = NULL;
	}
	__real_free(pointer);
}

static void make_chain(FvwmWindow *ring, FvwmWindow *windows, size_t count)
{
	memset(ring, 0, sizeof(*ring));
	memset(windows, 0, count * sizeof(*windows));
	ring->stack_next = count ? windows : ring;
	ring->stack_prev = count ? windows + count - 1 : ring;
	for (size_t i = 0; i < count; i++)
	{
		FvwmWindow *w = windows + i;

		FW_W(w) = 1000 + count - i;
		w->layer = 4;
		w->stack_next = i + 1 == count ? ring : w + 1;
		w->stack_prev = i == 0 ? ring : w - 1;
		SET_TRANSIENT(w, i + 1 < count);
		SET_SCHEDULED_FOR_RAISE(w, 1);
		FW_COMMON_STATIC_FLAGS(w).do_stack_transient_parent = 1;
		/* The next record's ID is not initialized yet. */
		FW_W_TRANSIENTFOR(w) = i + 1 < count ? 1000 + count - i - 1 : None;
	}
	allocations = 0;
	assert(owned == NULL);
}

static void assert_finished(fvwm_stack_parent_path *path)
{
	assert(path->leaf == NULL && path->leaf_client == None &&
		path->index == NULL);
	assert(path->count == 0 && path->depth == 0 && owned == NULL);
	for (unsigned i = 0; i < FVWM_STACK_PARENT_INLINE; i++)
	{
		assert(path->inline_frames[i] == NULL &&
			path->inline_clients[i] == None);
	}
	assert(fvwm_stack_parent_pop(path, NULL) == NULL);
	fvwm_stack_parent_finish(path);
}

static void test_depth_and_modes(void)
{
	const size_t sizes[] = { 1, 2, 31, 32, 33, 34, 65, 4096 };
	FvwmWindow ring;
	FvwmWindow *windows = malloc(4096 * sizeof(*windows));

	assert(windows != NULL);
	for (unsigned si = 0; si < sizeof(sizes) / sizeof(*sizes); si++)
	{
		size_t count = sizes[si];

		for (int mode = MARK_RAISE; mode <= MARK_RESTACK; mode++)
		{
			fvwm_stack_parent_path path;

			make_chain(&ring, windows, count);
			/* Caller-supplied weights prove policy retains no magic limits. */
			assert(fvwm_stack_parent_prepare(&path, &ring, windows,
				mode, True, 71, -901) == FVWM_STACK_PARENT_READY);
			assert(path.leaf == windows + count - 1);
			assert(path.depth == count - 1);
			assert(allocations == (count > 33 ? 1 : 0));
			for (size_t i = 0; i < count; i++)
			{
				assert(!windows[i].flags.is_scheduled_for_raise);
				assert(windows[i].stack_next ==
					(i + 1 == count ? &ring : windows + i + 1));
				assert(windows[i].stack_prev ==
					(i == 0 ? &ring : windows + i - 1));
				assert(windows[i].scratch.i == (i + 1 == count ?
					0 : mode == MARK_LOWER && i + 2 == count ? -901 : 71));
			}
			for (size_t i = count - 1; i != 0; i--)
			{
				assert(fvwm_stack_parent_pop(&path, NULL) == windows + i - 1);
			}
			assert(fvwm_stack_parent_pop(&path, NULL) == NULL);
			fvwm_stack_parent_finish(&path);
			assert_finished(&path);
		}
	}
	__real_free(windows);
}

/* Independent lookup/visited-vector oracle, with no production cycle/index API. */
static fvwm_stack_parent_result reference(
	FvwmWindow *ring, FvwmWindow *target, FvwmWindow **frames, size_t *depth,
	FvwmWindow **leaf)
{
	FvwmWindow *seen[256];
	size_t visits = 0;
	FvwmWindow *current = target;

	*depth = 0;
	for (;;)
	{
		FvwmWindow *parent;

		seen[visits++] = current;
		assert(visits <= 256);
		if (IS_ICONIFIED(current) || !IS_TRANSIENT(current) ||
		    !DO_STACK_TRANSIENT_PARENT(current))
		{
			break;
		}
		for (parent = ring->stack_next; parent != ring;
		     parent = parent->stack_next)
		{
			if (FW_W(parent) == FW_W_TRANSIENTFOR(current))
			{
				break;
			}
		}
		if (parent == ring || parent == current || IS_ICONIFIED(parent) ||
		    parent->layer != current->layer)
		{
			break;
		}
		for (size_t i = 0; i < visits; i++)
		{
			if (parent == seen[i])
			{
				return FVWM_STACK_PARENT_CYCLE;
			}
		}
		frames[(*depth)++] = current;
		current = parent;
	}
	*leaf = current;
	return FVWM_STACK_PARENT_READY;
}

static void compare_reference(FvwmWindow *ring, FvwmWindow *target)
{
	fvwm_stack_parent_path path;
	FvwmWindow *frames[256];
	FvwmWindow *leaf = NULL;
	size_t depth;
	fvwm_stack_parent_result expected = reference(
		ring, target, frames, &depth, &leaf);

	assert(fvwm_stack_parent_prepare(&path, ring, target, MARK_RAISE,
		True, 71, -901) == expected);
	if (expected == FVWM_STACK_PARENT_READY)
	{
		assert(path.leaf == leaf && path.depth == depth);
		while (depth != 0)
		{
			assert(fvwm_stack_parent_pop(&path, NULL) == frames[--depth]);
		}
		assert(fvwm_stack_parent_pop(&path, NULL) == NULL);
	}
	fvwm_stack_parent_finish(&path);
	assert_finished(&path);
}

static void test_cycles_and_gates(void)
{
	FvwmWindow ring, windows[128];
	const unsigned sizes[] = { 2, 3, 31, 32, 33, 34, 65, 100 };

	for (unsigned si = 0; si < sizeof(sizes) / sizeof(*sizes); si++)
	{
		unsigned count = sizes[si];

		for (unsigned prefix = 0; prefix + 1 < count; prefix++)
		{
			make_chain(&ring, windows, count);
			SET_TRANSIENT(windows + count - 1, 1);
			FW_W_TRANSIENTFOR(windows + count - 1) = FW_W(windows + prefix);
			compare_reference(&ring, windows);
		}
		for (unsigned stop = 0; stop < count; stop++)
		{
			for (unsigned gate = 0; gate < 5; gate++)
			{
				FvwmWindow *w = windows + stop;

				make_chain(&ring, windows, count);
				switch (gate)
				{
				case 0: SET_TRANSIENT(w, 0); break;
				case 1: SET_ICONIFIED(w, 1); break;
				case 2: w->layer = 9; break;
				case 3:
					FW_COMMON_STATIC_FLAGS(w).do_stack_transient_parent = 0;
					break;
				case 4: FW_W_TRANSIENTFOR(w) = FW_W(w); break;
				}
				compare_reference(&ring, windows);
			}
		}
	}
}

static void test_membership_and_first_match(void)
{
	FvwmWindow ring, windows[70], outside = { 0 };
	fvwm_stack_parent_path path;

	for (unsigned stop = 1; stop < 66; stop += 64)
	{
		make_chain(&ring, windows, 70);
		FW_W(windows + stop + 1) = FW_W(windows + stop);
		/* First ring match wins even when it blocks a later valid duplicate. */
		SET_ICONIFIED(windows + stop, 1);
		compare_reference(&ring, windows);
		make_chain(&ring, windows, 70);
		windows[stop - 1].next = &outside;
		FW_W(&outside) = 999999;
		FW_W_TRANSIENTFOR(windows + stop - 1) = FW_W(&outside);
		compare_reference(&ring, windows);
		/* The invoked target need not itself belong to the stack ring. */
		SET_TRANSIENT(&outside, 1);
		FW_COMMON_STATIC_FLAGS(&outside).do_stack_transient_parent = 1;
		outside.layer = 4;
		FW_W_TRANSIENTFOR(&outside) = FW_W(windows);
		compare_reference(&ring, &outside);
	}
	make_chain(&ring, windows, 0);
	compare_reference(&ring, &outside);
	make_chain(&ring, windows, 70);
	refuse_allocation = True;
	assert(fvwm_stack_parent_prepare(&path, &ring, windows, MARK_LOWER,
		False, 71, -901) == FVWM_STACK_PARENT_READY);
	assert(path.leaf == windows && path.depth == 0 && allocations == 0);
	fvwm_stack_parent_finish(&path);
	assert_finished(&path);
	refuse_allocation = False;
}

static void test_refusal_and_retry(void)
{
	FvwmWindow ring, windows[70];
	fvwm_stack_parent_path path;

	make_chain(&ring, windows, 70);
	refuse_allocation = True;
	assert(fvwm_stack_parent_prepare(&path, &ring, windows, MARK_RAISE,
		True, 71, -901) == FVWM_STACK_PARENT_UNAVAILABLE);
	assert(allocations == 1);
	assert_finished(&path);
	assert(windows[0].scratch.i == 71);
	/* Production discards only temporary ranks before its direct fallback. */
	for (unsigned i = 0; i < 70; i++)
	{
		windows[i].scratch.i = 0;
	}
	assert(fvwm_stack_parent_prepare(&path, &ring, windows, MARK_RAISE,
		False, 71, -901) == FVWM_STACK_PARENT_READY);
	assert(path.leaf == windows && path.depth == 0);
	fvwm_stack_parent_finish(&path);
	refuse_allocation = False;
	compare_reference(&ring, windows);
	/* Early caller cancellation releases the entire continuation owner. */
	assert(fvwm_stack_parent_prepare(&path, &ring, windows, MARK_RAISE,
		True, 71, -901) == FVWM_STACK_PARENT_READY);
	fvwm_stack_parent_finish(&path);
	assert_finished(&path);
}

static void test_continuation_client_identity(void)
{
	const unsigned counts[] = { 4, 70 };

	for (unsigned n = 0; n < sizeof(counts) / sizeof(counts[0]); n++)
	{
		FvwmWindow ring, windows[70];
		fvwm_stack_parent_path path;
		Window clients[70];
		unsigned count = counts[n];

		make_chain(&ring, windows, count);
		for (unsigned i = 0; i < count; i++)
		{
			clients[i] = FW_W(windows + i);
		}
		assert(fvwm_stack_parent_prepare(&path, &ring, windows,
			MARK_RAISE, True, 71, -901) == FVWM_STACK_PARENT_READY);
		assert(path.leaf_client == clients[count - 1]);
		for (unsigned i = 0; i < count; i++)
		{
			FW_W(windows + i) = None;
			windows[i].stack_next = windows[i].stack_prev = NULL;
		}
		for (unsigned i = count - 1; i > 0; i--)
		{
			Window client = None;

			assert(fvwm_stack_parent_pop(&path, &client) == windows + i - 1);
			assert(client == clients[i - 1]);
		}
		Window client = 99;

		assert(fvwm_stack_parent_pop(&path, &client) == NULL);
		assert(client == None);
		fvwm_stack_parent_finish(&path);
		assert_finished(&path);
	}
}

int main(void)
{
	test_continuation_client_identity();
	test_depth_and_modes();
	test_cycles_and_gates();
	test_membership_and_first_match();
	test_refusal_and_retry();
	return 0;
}
