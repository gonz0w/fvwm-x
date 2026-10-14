/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include "stack_subtree.h"

typedef struct
{
	FvwmWindow *parent;
	FvwmWindow *child;
	size_t pending;
	Bool queued;
} stack_transient_edge;

static int compare_parent(const void *left, const void *right)
{
	const stack_transient_edge *a = left;
	const stack_transient_edge *b = right;
	uintptr_t a_key = (uintptr_t)a->parent;
	uintptr_t b_key = (uintptr_t)b->parent;

	return (a_key > b_key) - (a_key < b_key);
}

static Bool is_candidate(FvwmWindow *child, Bool ignore_icons)
{
	return !IS_IN_TRANSIENT_SUBTREE(child) && IS_TRANSIENT(child) &&
		!(ignore_icons && IS_ICONIFIED(child)) && child->scratch.p != NULL;
}

static void queue_parent(
	stack_transient_edge *edges, size_t count, FvwmWindow *parent,
	size_t *tail)
{
	size_t low = 0;
	size_t high = count;
	uintptr_t key = (uintptr_t)parent;

	while (low < high)
	{
		size_t mid = low + (high - low) / 2;

		if ((uintptr_t)edges[mid].parent < key)
		{
			low = mid + 1;
		}
		else
		{
			high = mid;
		}
	}
	if (low < count && edges[low].parent == parent && !edges[low].queued)
	{
		/* One queue entry per parent bucket fits in the edge allocation. */
		edges[low].queued = True;
		edges[(*tail)++].pending = low;
	}
}

Bool fvwm_stack_mark_closure(
	FvwmWindow *start, FvwmWindow *end, int mode, Bool ignore_icons)
{
	stack_transient_edge *edges;
	FvwmWindow *child;
	size_t count = 0;
	size_t head = 0;
	size_t tail = 0;

	for (child = start; child != end; child = child->stack_next)
	{
		if (is_candidate(child, ignore_icons))
		{
			if (count == SIZE_MAX / sizeof(*edges))
			{
				return False;
			}
			count++;
		}
	}
	if (count == 0)
	{
		return True;
	}
	edges = calloc(count, sizeof(*edges));
	if (edges == NULL)
	{
		return False;
	}
	for (child = start; child != end; child = child->stack_next)
	{
		if (is_candidate(child, ignore_icons))
		{
			edges[tail].parent = child->scratch.p;
			edges[tail++].child = child;
		}
	}
	qsort(edges, count, sizeof(*edges), compare_parent);
	tail = 0;
	for (size_t i = 0; i < count;)
	{
		FvwmWindow *parent = edges[i].parent;

		if (IS_IN_TRANSIENT_SUBTREE(parent))
		{
			edges[i].queued = True;
			edges[tail++].pending = i;
		}
		do
		{
			i++;
		} while (i < count && edges[i].parent == parent);
	}
	while (head < tail)
	{
		size_t i = edges[head++].pending;
		FvwmWindow *parent = edges[i].parent;

		do
		{
			child = edges[i].child;
			if (fvwm_stack_mark_parent(child, mode, ignore_icons))
			{
				queue_parent(edges, count, child, &tail);
			}
			i++;
		} while (i < count && edges[i].parent == parent);
	}
	free(edges);
	return True;
}
