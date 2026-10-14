/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include "stack_sort.h"

void fvwm_stack_sort_ring(FvwmWindow *ring)
{
	FvwmWindow *head;
	size_t run_size = 1;

	if (ring->stack_next->stack_next == ring)
	{
		return;
	}
	/* Keep the common already-sorted case linear without relinking. */
	for (head = ring->stack_next; head->stack_next != ring;
	     head = head->stack_next)
	{
		if (head->scratch.i < head->stack_next->scratch.i)
		{
			break;
		}
	}
	if (head->stack_next == ring)
	{
		return;
	}
	/* No callbacks or X work occur while this temporary ring is linear. */
	head = ring->stack_next;
	ring->stack_prev->stack_next = NULL;
	for (;;)
	{
		FvwmWindow *left = head;
		FvwmWindow *tail = NULL;
		size_t merges = 0;

		head = NULL;
		while (left != NULL)
		{
			FvwmWindow *right = left;
			size_t left_count = 0;
			size_t right_count = run_size;

			merges++;
			for (; left_count < run_size && right != NULL;
			     left_count++)
			{
				right = right->stack_next;
			}
			while (left_count || (right_count && right != NULL))
			{
				FvwmWindow *next;

				if (!left_count)
				{
					next = right;
					right = right->stack_next;
					right_count--;
				}
				else if (!right_count || right == NULL ||
					 left->scratch.i >= right->scratch.i)
				{
					/* Taking the left node on ties preserves order. */
					next = left;
					left = left->stack_next;
					left_count--;
				}
				else
				{
					next = right;
					right = right->stack_next;
					right_count--;
				}
				if (tail != NULL)
				{
					tail->stack_next = next;
				}
				else
				{
					head = next;
				}
				next->stack_prev = tail;
				tail = next;
			}
			left = right;
		}
		tail->stack_next = NULL;
		if (merges == 1)
		{
			ring->stack_next = head;
			head->stack_prev = ring;
			ring->stack_prev = tail;
			tail->stack_next = ring;
			return;
		}
		run_size = run_size > SIZE_MAX / 2 ? SIZE_MAX : run_size * 2;
	}
}
