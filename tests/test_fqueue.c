/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "libs/fqueue.h"

static int refuse_allocation;
void *__real_calloc(size_t count, size_t size);

void *__wrap_calloc(size_t count, size_t size)
{
	return refuse_allocation ? NULL : __real_calloc(count, size);
}

static void test_fallible_append_preserves_ownership(void)
{
	fqueue queue;
	int a = 1, b = 2;
	void *object;

	fqueue_init(&queue);
	refuse_allocation = 1;
	assert(!fqueue_try_add_at_end(&queue, &a));
	assert(FQUEUE_IS_EMPTY(&queue));
	refuse_allocation = 0;
	assert(fqueue_try_add_at_end(&queue, &a));
	refuse_allocation = 1;
	assert(!fqueue_try_add_at_end(&queue, &b));
	assert(fqueue_get_length(&queue) == 1);
	refuse_allocation = 0;
	assert(fqueue_try_add_at_end(&queue, &b));
	assert(fqueue_take_first(&queue, &object) && object == &a);
	assert(fqueue_take_first(&queue, &object) && object == &b);
	assert(FQUEUE_IS_EMPTY(&queue));
}

static void test_fifo_transfer_and_tail_reuse(void)
{
	fqueue queue;
	int a = 1, b = 2, c = 3;
	void *object = &a;

	fqueue_init(&queue);
	assert(!fqueue_take_first(&queue, &object) && object == NULL);
	fqueue_add_at_front(&queue, &b);
	fqueue_add_at_front(&queue, &a);
	fqueue_add_at_end(&queue, &c);
	assert(fqueue_get_length(&queue) == 3);
	assert(fqueue_take_first(&queue, &object) && object == &a);
	assert(fqueue_take_first(&queue, &object) && object == &b);
	assert(fqueue_take_first(&queue, &object) && object == &c);
	assert(FQUEUE_IS_EMPTY(&queue));
	fqueue_add_at_end(&queue, NULL);
	assert(fqueue_take_first(&queue, &object) && object == NULL);
	assert(!fqueue_take_first(&queue, &object) && object == NULL);
	fqueue_add_at_front(&queue, &a);
	assert(fqueue_take_first(&queue, &object) && object == &a);
}

typedef struct
{
	fqueue *queue;
	int *added;
	unsigned operated;
} callback_state;

static fqueue *destroy_queue;
static unsigned destroyed;

static void check_destroy_locked(void *object)
{
	void *taken = object;

	assert(!fqueue_take_first(destroy_queue, &taken));
	assert(taken == NULL);
	destroyed++;
}

static void nested_operation(void *object, void *context)
{
	callback_state *state = context;
	void *taken = object;

	assert(!fqueue_take_first(state->queue, &taken));
	assert(taken == NULL);
	state->operated++;
	if (state->operated == 1)
	{
		/* Nested removal defers deletion and additions stay out of the
		 * current traversal. The new FIFO API must refuse that lock. */
		fqueue_remove_or_operate_from_front(
			state->queue, NULL, NULL, check_destroy_locked, NULL);
		fqueue_add_at_end(state->queue, state->added);
	}
}

static void try_append_locked(void *object, void *context)
{
	callback_state *state = context;

	if (state->operated++ == 0)
	{
		unsigned before = fqueue_get_length(state->queue);

		refuse_allocation = 1;
		assert(!fqueue_try_add_at_end(state->queue, state->added));
		assert(fqueue_get_length(state->queue) == before);
		refuse_allocation = 0;
		assert(fqueue_try_add_at_end(state->queue, state->added));
	}
}

static void test_fallible_locked_visibility(void)
{
	fqueue queue;
	int a = 1, b = 2, added = 3;
	callback_state state = { &queue, &added, 0 };
	void *object;

	fqueue_init(&queue);
	fqueue_add_at_end(&queue, &a);
	fqueue_add_at_end(&queue, &b);
	fqueue_remove_or_operate_all(
		&queue, NULL, try_append_locked, NULL, &state);
	assert(state.operated == 2 && fqueue_get_length(&queue) == 1);
	assert(fqueue_take_first(&queue, &object) && object == &added);
	assert(FQUEUE_IS_EMPTY(&queue));
}

static void test_nested_callbacks_preserve_cleanup(void)
{
	fqueue queue;
	int a = 1, b = 2, added = 3;
	callback_state state = { &queue, &added, 0 };
	void *object;

	fqueue_init(&queue);
	fqueue_add_at_end(&queue, &a);
	fqueue_add_at_end(&queue, &b);
	destroy_queue = &queue;
	destroyed = 0;
	fqueue_remove_or_operate_all(
		&queue, NULL, nested_operation, check_destroy_locked, &state);
	assert(state.operated == 2 && destroyed == 2);
	assert(fqueue_get_length(&queue) == 1);
	assert(fqueue_take_first(&queue, &object) && object == &added);
	assert(FQUEUE_IS_EMPTY(&queue));
	destroy_queue = NULL;
}

static void test_large_drain_and_legacy_front_removal(void)
{
	fqueue queue;
	unsigned items[10000];
	void *object;

	fqueue_init(&queue);
	for (unsigned i = 0; i < 10000; i++)
	{
		items[i] = i;
		fqueue_add_at_end(&queue, &items[i]);
	}
	/* Old removal can leave a dirty flag after cleanup. An unlocked
	 * transfer still sees the normalized records, without rescanning. */
	fqueue_remove_or_operate_from_front(&queue, NULL, NULL, NULL, NULL);
	for (unsigned i = 1; i < 10000; i++)
	{
		assert(fqueue_take_first(&queue, &object));
		assert(object == &items[i]);
	}
	assert(FQUEUE_IS_EMPTY(&queue) && fqueue_get_length(&queue) == 0);
}

typedef struct
{
	uint64_t deadline;
	unsigned sequence;
} sorted_item;

static int compare_sorted_items(void *left, void *right, void *context)
{
	const sorted_item *a = left;
	const sorted_item *b = right;
	size_t *comparisons = context;

	(*comparisons)++;
	return (a->deadline > b->deadline) - (a->deadline < b->deadline);
}

static void test_sorted_monotonic_insertion_work(void)
{
	sorted_item items[512];
	const unsigned count = sizeof(items) / sizeof(items[0]);

	for (unsigned mode = 0; mode < 3; mode++)
	{
		fqueue queue;
		size_t comparisons = 0;

		fqueue_init(&queue);
		for (unsigned i = 0; i < count; i++)
		{
			items[i].deadline = mode == 0 ? i :
				mode == 1 ? count - i : 7;
			items[i].sequence = i;
			fqueue_add_inside(&queue, &items[i], compare_sorted_items,
				&comparisons);
		}
		assert(comparisons <= 4 * count);
		for (unsigned i = 0; i < count; i++)
		{
			void *object;
			unsigned expected = mode == 1 ? count - i - 1 : i;

			assert(fqueue_take_first(&queue, &object));
			assert(object == &items[expected]);
		}
		assert(FQUEUE_IS_EMPTY(&queue));
	}
}

static void test_sorted_mixed_keys_keep_equal_order(void)
{
	sorted_item items[] = {{3, 0}, {1, 1}, {3, 2},
		{1, 3}, {2, 4}, {3, 5}};
	const unsigned order[] = {1, 3, 4, 0, 2, 5};
	fqueue queue;
	size_t comparisons = 0;

	fqueue_init(&queue);
	for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++)
	{
		fqueue_add_inside(&queue, &items[i], compare_sorted_items,
			&comparisons);
	}
	for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++)
	{
		void *object;

		assert(fqueue_take_first(&queue, &object));
		assert(object == &items[order[i]]);
	}
	assert(FQUEUE_IS_EMPTY(&queue));
}

typedef struct
{
	fqueue *queue;
	sorted_item *added;
	size_t comparisons;
	unsigned operated;
} sorted_callback_state;

static void sorted_insert_locked(void *object, void *context)
{
	sorted_callback_state *state = context;

	(void)object;
	if (state->operated++ == 0)
	{
		fqueue_add_inside(state->queue, state->added,
			compare_sorted_items, &state->comparisons);
	}
}

static void test_sorted_locked_insert_keeps_traversal(void)
{
	fqueue queue;
	sorted_item a = {10, 0}, b = {20, 1}, added = {30, 2};
	sorted_callback_state state = {&queue, &added, 0, 0};
	void *object;

	fqueue_init(&queue);
	fqueue_add_at_end(&queue, &a);
	fqueue_add_at_end(&queue, &b);
	fqueue_remove_or_operate_all(
		&queue, NULL, sorted_insert_locked, NULL, &state);
	assert(state.operated == 2 && state.comparisons == 2);
	assert(fqueue_take_first(&queue, &object) && object == &added);
	assert(FQUEUE_IS_EMPTY(&queue));
}

int main(void)
{
	test_fallible_append_preserves_ownership();
	test_fallible_locked_visibility();
	test_fifo_transfer_and_tail_reuse();
	test_nested_callbacks_preserve_cleanup();
	test_large_drain_and_legacy_front_removal();
	test_sorted_monotonic_insertion_work();
	test_sorted_mixed_keys_keep_equal_order();
	test_sorted_locked_insert_keeps_traversal();
	return 0;
}
