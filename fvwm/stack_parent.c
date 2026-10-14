/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "parent_chain_policy.h"
#include "stack_parent.h"

struct fvwm_stack_parent_entry
{
	FvwmWindow *window;
	FvwmWindow *frame;
	Window frame_client;
	size_t order;
};

static int compare_parent_window(const void *left, const void *right)
{
	const struct fvwm_stack_parent_entry *a = left;
	const struct fvwm_stack_parent_entry *b = right;
	Window a_key = FW_W(a->window);
	Window b_key = FW_W(b->window);

	if (a_key != b_key)
	{
		return (a_key > b_key) - (a_key < b_key);
	}
	return (a->order > b->order) - (a->order < b->order);
}

static FvwmWindow *find_parent_window(
	struct fvwm_stack_parent_entry *index, size_t count, Window key)
{
	size_t low = 0;
	size_t high = count;

	while (low < high)
	{
		size_t mid = low + (high - low) / 2;

		if (FW_W(index[mid].window) < key)
		{
			low = mid + 1;
		}
		else
		{
			high = mid;
		}
	}
	return low < count && FW_W(index[low].window) == key ?
		index[low].window : NULL;
}

fvwm_stack_parent_result fvwm_stack_parent_prepare(
	fvwm_stack_parent_path *path, FvwmWindow *ring, FvwmWindow *target,
	int mode, Bool allow_parent, int64_t branch_bonus, int64_t lower_penalty)
{
	fvwm_parent_chain_state parents;
	struct fvwm_stack_parent_entry *index = NULL;
	FvwmWindow *current = target;
	size_t count = 0;
	size_t depth = 0;

	memset(path, 0, sizeof(*path));
	fvwm_parent_chain_init(&parents, target);
	for (;;)
	{
		FvwmWindow *parent = NULL;

		SET_SCHEDULED_FOR_RAISE(current, 0);
		if (!allow_parent || IS_ICONIFIED(current) ||
		    !IS_TRANSIENT(current) || !DO_STACK_TRANSIENT_PARENT(current))
		{
			break;
		}
		if (index != NULL)
		{
			parent = find_parent_window(index, count,
				FW_W_TRANSIENTFOR(current));
		}
		else
		{
			for (parent = ring->stack_next;
			     parent != ring; parent = parent->stack_next)
			{
				if (FW_W(parent) == FW_W_TRANSIENTFOR(current))
				{
					break;
				}
			}
			if (parent == ring)
			{
				parent = NULL;
			}
		}
		if (parent == NULL || parent == current || IS_ICONIFIED(parent) ||
		    parent->layer != current->layer)
		{
			break;
		}
		if (fvwm_parent_chain_repeated(&parents, parent))
		{
			fvwm_stack_parent_finish(path);
			return FVWM_STACK_PARENT_CYCLE;
		}
		if (index == NULL && depth == FVWM_STACK_PARENT_INLINE)
		{
			FvwmWindow *window;

			for (window = ring->stack_next;
			     window != ring; window = window->stack_next)
			{
				if (count == SIZE_MAX / sizeof(*index))
				{
					fvwm_stack_parent_finish(path);
					return FVWM_STACK_PARENT_UNAVAILABLE;
				}
				count++;
			}
			if (count <= depth)
			{
				/* A further parent cannot be distinct in this ring. */
				fvwm_stack_parent_finish(path);
				return FVWM_STACK_PARENT_CYCLE;
			}
			index = calloc(count, sizeof(*index));
			path->index = index;
			if (index == NULL)
			{
				fvwm_stack_parent_finish(path);
				return FVWM_STACK_PARENT_UNAVAILABLE;
			}
			count = 0;
			for (window = ring->stack_next;
			     window != ring; window = window->stack_next)
			{
				index[count].window = window;
				index[count].order = count;
				count++;
			}
			qsort(index, count, sizeof(*index), compare_parent_window);
			for (size_t i = 0; i < depth; i++)
			{
				index[i].frame = path->inline_frames[i];
				index[i].frame_client = path->inline_clients[i];
			}
		}
		if (index != NULL && depth == count)
		{
			fvwm_stack_parent_finish(path);
			return FVWM_STACK_PARENT_CYCLE;
		}
		current->scratch.i = mode == MARK_LOWER &&
			(!IS_TRANSIENT(parent) || !DO_STACK_TRANSIENT_PARENT(parent)) ?
			lower_penalty : branch_bonus;
		if (index != NULL)
		{
			index[depth].frame = current;
			index[depth].frame_client = FW_W(current);
		}
		else
		{
			path->inline_frames[depth] = current;
			path->inline_clients[depth] = FW_W(current);
		}
		depth++;
		current = parent;
	}
	path->leaf = current;
	path->leaf_client = FW_W(current);
	path->count = count;
	path->depth = depth;
	return FVWM_STACK_PARENT_READY;
}

FvwmWindow *fvwm_stack_parent_pop(fvwm_stack_parent_path *path, Window *client)
{
	FvwmWindow *child;

	if (client != NULL)
	{
		*client = None;
	}
	if (path->depth == 0)
	{
		return NULL;
	}
	path->depth--;
	if (path->index != NULL)
	{
		child = path->index[path->depth].frame;
		if (client != NULL)
		{
			*client = path->index[path->depth].frame_client;
		}
		path->index[path->depth].frame_client = None;
		path->index[path->depth].frame = NULL;
	}
	else
	{
		child = path->inline_frames[path->depth];
		if (client != NULL)
		{
			*client = path->inline_clients[path->depth];
		}
	}
	if (path->depth < FVWM_STACK_PARENT_INLINE)
	{
		path->inline_frames[path->depth] = NULL;
		path->inline_clients[path->depth] = None;
	}
	return child;
}

void fvwm_stack_parent_finish(fvwm_stack_parent_path *path)
{
	free(path->index);
	memset(path, 0, sizeof(*path));
}
