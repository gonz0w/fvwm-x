/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "fvwm/fvwm.h"


#include "fvwm/stack_sort.h"

static int compare_expected(const void *left, const void *right)
{
	const FvwmWindow *a = *(FvwmWindow * const *)left;
	const FvwmWindow *b = *(FvwmWindow * const *)right;

	if (a->scratch.i != b->scratch.i)
	{
		return a->scratch.i > b->scratch.i ? -1 : 1;
	}
	return (a->layer > b->layer) - (a->layer < b->layer);
}

static void measure(unsigned count, unsigned shape)
{
	FvwmWindow ring = { 0 };
	FvwmWindow *windows = calloc(count ? count : 1, sizeof(*windows));
	FvwmWindow **expected = calloc(count ? count : 1, sizeof(*expected));

	assert(windows && expected);
	ring.stack_next = count ? windows : &ring;
	ring.stack_prev = count ? windows + count - 1 : &ring;
	for (unsigned i = 0; i < count; i++)
	{
		FvwmWindow *w = windows + i;
		w->stack_prev = i ? w - 1 : &ring;
		w->stack_next = i + 1 == count ? &ring : w + 1;
		w->layer = i;
		w->scratch.i = shape == 0 ? i : shape == 1 ? count - i :
			shape == 2 ? 0 : shape == 3 ? (i * 37U) % 11 :
			(i % 2 ? INT64_MIN : INT64_MAX);
		expected[i] = w;
	}
	qsort(expected, count, sizeof(*expected), compare_expected);
	fvwm_stack_sort_ring(&ring);
	FvwmWindow *current = ring.stack_next;
	for (unsigned i = 0; i < count; i++)
	{
		assert(current == expected[i]);
		assert(current->stack_prev->stack_next == current);
		assert(current->stack_next->stack_prev == current);
		current = current->stack_next;
	}
	assert(current == &ring);
	assert(ring.stack_prev->stack_next == &ring);
	assert(ring.stack_next->stack_prev == &ring);

	free(expected);
	free(windows);
}

int main(void)
{
	for (unsigned shape = 0; shape < 5; shape++)
	{
		measure(0, shape);
		measure(1, shape);
		const unsigned partial_runs[] = { 2, 3, 7, 127, 129, 513, 2049 };
		for (unsigned i = 0; i < sizeof(partial_runs) /
		     sizeof(partial_runs[0]); i++)
		{
			measure(partial_runs[i], shape);
		}
		for (unsigned count = 128; count <= 2048; count *= 2)
		{
			measure(count, shape);
		}
	}
	return 0;
}
