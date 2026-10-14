/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fvwm/stack_window_list.h"

static void test_selection_and_capacity(void)
{
	for (unsigned flags = 0; flags < 16; flags++)
	{
		FvwmWindow window = { 0 };
		Window expected[3];
		size_t count = 0;

		FW_W_FRAME(&window) = 31;
		FW_W_ICON_TITLE(&window) = flags & 4 ? 41 : None;
		FW_W_ICON_PIXMAP(&window) = flags & 8 ? 51 : None;
		SET_ICONIFIED(&window, flags & 1);
		FW_COMMON_STATIC_FLAGS(&window).is_icon_suppressed = !!(flags & 2);
		expected[count++] = 31;
		if ((flags & 3) == 1)
		{
			if (flags & 4)
			{
				expected[count++] = 41;
			}
			if (flags & 8)
			{
				expected[count++] = 51;
			}
		}
		assert(fvwm_stack_window_count(&window) == count);
		assert(fvwm_stack_window_bottom(&window) == expected[count - 1]);
		for (size_t start = 0; start < 4; start++)
		{
			for (size_t capacity = 0; capacity < 8; capacity++)
			{
				Window list[8], before[8];
				size_t used = start;
				Bool fits = start <= capacity && count <= capacity - start;

				for (unsigned i = 0; i < 8; i++)
				{
					before[i] = list[i] = 700 + i;
				}
				assert(fvwm_stack_window_append(list, capacity, &used,
					&window) == fits);
				assert(used == start + (fits ? count : 0));
				for (unsigned i = 0; i < 8; i++)
				{
					assert(list[i] == (fits && i >= start && i < used ?
						expected[i - start] : before[i]));
				}
			}
		}
	}
}

static void test_xlib_count_boundary(void)
{
	FvwmWindow window = { 0 };
	Window list[1] = { 79 };

	SET_ICONIFIED(&window, 1);
	for (unsigned icons = 0; icons < 3; icons++)
	{
		FW_W_ICON_TITLE(&window) = icons > 0 ? 41 : None;
		FW_W_ICON_PIXMAP(&window) = icons > 1 ? 51 : None;
		for (unsigned remaining = 0; remaining < 5; remaining++)
		{
			size_t count = (size_t)INT_MAX - remaining;
			size_t original = count;

			assert(fvwm_stack_window_count_add(&count, &window) ==
				(remaining >= 1 + icons));
			assert(count == original + (remaining >= 1 + icons ? 1 + icons : 0));
			/* Capacity is synthetic; integer refusal precedes any access. */
			if (remaining < 1 + icons)
			{
				size_t used = original;

				assert(!fvwm_stack_window_append(list, SIZE_MAX,
					&used, &window));
				assert(used == original && list[0] == 79);
			}
		}
	}
	size_t count = SIZE_MAX;
	assert(!fvwm_stack_window_count_add(&count, &window));
	assert(count == SIZE_MAX);
	assert(!fvwm_stack_window_append(list, 1, &count, &window));
	assert(count == SIZE_MAX && list[0] == 79);
}

static void test_batched_order_and_anchors(void)
{
	const unsigned lengths[] = { 0, 1, 2, 20, 21, 22, 63, 64, 65, 127, 129 };
	FvwmWindow ring = { 0 };
	FvwmWindow *windows = calloc(129, sizeof(*windows));

	assert(windows != NULL);
	for (unsigned flags = 0; flags < 32; flags++)
	{
		for (unsigned length_i = 0; length_i < sizeof(lengths) /
		     sizeof(lengths[0]); length_i++)
		{
			unsigned count = lengths[length_i];
			Window expected[129 * 3], observed[129 * 3];
			size_t total = 0, used = 0;
			unsigned batches = 0;
			Window anchor = None;
			fvwm_stack_window_batch batch;

			ring.stack_next = count ? windows : &ring;
			for (unsigned i = 0; i < count; i++)
			{
				FvwmWindow *window = windows + i;
				unsigned selection = flags < 16 ? flags : (flags + i) % 16;

				window->stack_next = i + 1 == count ? &ring : window + 1;
				FW_W_FRAME(window) = 10 + i * 3;
				FW_W_ICON_TITLE(window) = selection & 4 ? 11 + i * 3 : None;
				FW_W_ICON_PIXMAP(window) = selection & 8 ? 12 + i * 3 : None;
				SET_ICONIFIED(window, selection & 1);
				SET_ICON_SUPPRESSED(window, selection & 2);
				assert(fvwm_stack_window_append(expected, 129 * 3,
					&total, window));
			}
			fvwm_stack_window_batch_init(&batch, ring.stack_next, &ring);
			while (fvwm_stack_window_batch_next(&batch))
			{
				assert(batch.used > (batches ? 1 : 0));
				assert(batch.used <= FVWM_STACK_WINDOW_BATCH_CAPACITY);
				if (batches)
				{
					assert(batch.windows[0] == anchor);
				}
				for (size_t i = batches ? 1 : 0; i < batch.used; i++)
				{
					assert(used < total);
					observed[used++] = batch.windows[i];
				}
				anchor = batch.windows[batch.used - 1];
				batches++;
			}
			assert(batch.next == &ring && !fvwm_stack_window_batch_next(&batch));
			assert(used == total);
			assert(memcmp(expected, observed, total * sizeof(Window)) == 0);
			assert((batches == 0) == (count == 0));
			assert((batches > 1) == (total > FVWM_STACK_WINDOW_BATCH_CAPACITY));
		}
	}
	free(windows);
}

int main(void)
{
	test_selection_and_capacity();
	test_xlib_count_boundary();
	test_batched_order_and_anchors();
	return 0;
}
