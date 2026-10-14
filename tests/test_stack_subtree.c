/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "fvwm/stack_subtree.h"

static Bool refuse_allocation;
void *__real_calloc(size_t count, size_t size);

void *__wrap_calloc(size_t count, size_t size)
{
	return refuse_allocation ? NULL : __real_calloc(count, size);
}

static void test_allocation_failure(void)
{
	FvwmWindow seed = { 0 };
	FvwmWindow windows[2] = { 0 };

	SET_IN_TRANSIENT_SUBTREE(&seed, 1);
	SET_TRANSIENT(windows, 1);
	windows[0].stack_next = windows + 1;
	windows[0].scratch.p = &seed;
	windows[0].scratch.i = 200000;
	refuse_allocation = True;
	assert(!fvwm_stack_mark_closure(windows, windows + 1, MARK_ALL, False));
	assert(!IS_IN_TRANSIENT_SUBTREE(windows));
	assert(windows[0].scratch.i == 200000);
	assert(windows[0].scratch.p == &seed);
	assert(windows[0].stack_next == windows + 1);
	refuse_allocation = False;
	assert(fvwm_stack_mark_closure(windows, windows + 1, MARK_ALL, False));
	assert(IS_IN_TRANSIENT_SUBTREE(windows));
	assert(windows[0].scratch.i == 200001);
}

/* Independent ordered-scan oracle: one parent and one first-mark update. */
static void reference(FvwmWindow *windows, unsigned count, int mode, Bool icons)
{
	Bool changed;

	do
	{
		changed = False;
		for (unsigned i = 0; i < count; i++)
		{
			FvwmWindow *w = windows + i;
			FvwmWindow *p = w->scratch.p;

			if (IS_IN_TRANSIENT_SUBTREE(w) || !IS_TRANSIENT(w) ||
			    (icons && IS_ICONIFIED(w)) || p == NULL ||
			    !IS_IN_TRANSIENT_SUBTREE(p))
			{
				continue;
			}
			if (mode == MARK_ALL ||
			    ((mode == MARK_RAISE || mode == MARK_RESTACK) &&
			     DO_RAISE_TRANSIENT(p)) ||
			    ((mode == MARK_LOWER || mode == MARK_RESTACK) &&
			     DO_LOWER_TRANSIENT(p)))
			{
				SET_IN_TRANSIENT_SUBTREE(w, 1);
				w->scratch.i = fvwm_stack_rank_add_parent(
					w->scratch.i, p->scratch.i);
				changed = True;
			}
		}
	} while (changed);
}

static void setup(FvwmWindow *w, unsigned count, unsigned shape,
	FvwmWindow *seed)
{
	for (unsigned i = 0; i < count; i++)
	{
		w[i].stack_next = w + i + 1;
		w[i].stack_prev = i ? w + i - 1 : NULL;
		SET_TRANSIENT(w + i, 1);
		FW_COMMON_STATIC_FLAGS(w + i).do_raise_transient = shape < 2 ||
			i % 3 != 0;
		FW_COMMON_STATIC_FLAGS(w + i).do_lower_transient = shape < 2 ||
			i % 4 != 0;
		w[i].scratch.i = shape == 4 ? (i % 2 ? INT64_MIN : INT64_MAX) :
			(int64_t)(i % 7) * 200000;
		w[i].scratch.p = shape == 0 ? (i ? w + i - 1 : seed) :
			shape == 1 ? (i + 1 < count ? w + i + 1 : seed) :
			shape == 2 ? (i ? w + (i - 1) / 2 : seed) :
			shape == 3 ? w + (i + 1) % count :
			(i % 4 ? w + i - 1 : seed);
		if (shape == 2 && i % 13 == 0)
		{
			SET_ICONIFIED(w + i, 1);
		}
		if (shape == 3 && i % 17 == 0)
		{
			SET_IN_TRANSIENT_SUBTREE(w + i, 1);
		}
	}
}

static void test_closure(unsigned count, unsigned shape, int mode, Bool icons)
{
	FvwmWindow a_seed = { 0 }, b_seed = { 0 };
	FvwmWindow *a = calloc(count + 1, sizeof(*a));
	FvwmWindow *b = calloc(count + 1, sizeof(*b));

	assert(a && b);
	SET_IN_TRANSIENT_SUBTREE(&a_seed, 1);
	SET_IN_TRANSIENT_SUBTREE(&b_seed, 1);
	FW_COMMON_STATIC_FLAGS(&a_seed).do_raise_transient = 1;
	FW_COMMON_STATIC_FLAGS(&b_seed).do_raise_transient = 1;
	FW_COMMON_STATIC_FLAGS(&a_seed).do_lower_transient = 1;
	FW_COMMON_STATIC_FLAGS(&b_seed).do_lower_transient = 1;
	a_seed.scratch.i = b_seed.scratch.i = -2000000000;
	setup(a, count, shape, &a_seed);
	setup(b, count, shape, &b_seed);
	reference(a, count, mode, icons);
	assert(fvwm_stack_mark_closure(b, b + count, mode, icons));
	for (unsigned i = 0; i < count; i++)
	{
		assert(IS_IN_TRANSIENT_SUBTREE(a + i) ==
			IS_IN_TRANSIENT_SUBTREE(b + i));
		assert(a[i].scratch.i == b[i].scratch.i);
		assert(b[i].stack_next == b + i + 1);
		assert(b[i].stack_prev == (i ? b + i - 1 : NULL));
		if (shape < 2 && mode == MARK_ALL)
		{
			assert(IS_IN_TRANSIENT_SUBTREE(b + i));
		}
	}
	free(b);
	free(a);
}

static void test_group_hint_eligibility(void)
{
	FvwmWindow window = { 0 };
	XWMHints hints = { 0 };
	const Window root = 41;
	const Window groups[] = { None, 42, root, 43 };

	FW_W(&window) = 42;
	for (unsigned style = 0; style < 2; style++)
	{
		FW_COMMON_STATIC_FLAGS(&window).do_iconify_window_groups = style;
		window.wmhints = NULL;
		assert(!fvwm_stack_group_hint_is_active(&window, root));
		window.wmhints = &hints;
		for (unsigned flagged = 0; flagged < 2; flagged++)
		{
			hints.flags = flagged ? WindowGroupHint : 0;
			for (unsigned i = 0; i < 4; i++)
			{
				hints.window_group = groups[i];
				assert(fvwm_stack_group_hint_is_active(&window, root) ==
					(style && flagged && i == 3));
			}
		}
	}
}

int main(void)
{
	const unsigned counts[] = { 0, 1, 2, 7, 31, 127, 513, 2048 };

	test_allocation_failure();
	test_group_hint_eligibility();
	for (unsigned shape = 0; shape < 5; shape++)
	{
		for (int mode = MARK_RAISE; mode <= MARK_ALL; mode++)
		{
			for (unsigned icons = 0; icons < 2; icons++)
			{
				for (unsigned i = 0; i < sizeof(counts) /
				     sizeof(counts[0]); i++)
				{
					test_closure(counts[i], shape, mode, icons);
				}
			}
		}
	}
	return 0;
}
