#ifndef FVWM_STACK_WINDOW_LIST_H
#define FVWM_STACK_WINDOW_LIST_H

#include <limits.h>
#include <stddef.h>
#include "fvwm.h"

/* Frames are always included; icon title/pixmap XIDs follow only for a visible
 * iconified record. Keep counting and packing tied to the same selection. */
static inline size_t fvwm_stack_window_count(const FvwmWindow *window)
{
	size_t count = 1;

	if (IS_ICONIFIED(window) && !IS_ICON_SUPPRESSED(window))
	{
		count += FW_W_ICON_TITLE(window) != None;
		count += FW_W_ICON_PIXMAP(window) != None;
	}
	return count;
}

/* The lower sibling must be the last XID selected for this same record.
 * Suppressed icon windows are outside that selection and cannot anchor it. */
static inline Window fvwm_stack_window_bottom(const FvwmWindow *window)
{
	if (IS_ICONIFIED(window) && !IS_ICON_SUPPRESSED(window))
	{
		if (FW_W_ICON_PIXMAP(window) != None)
		{
			return FW_W_ICON_PIXMAP(window);
		}
		if (FW_W_ICON_TITLE(window) != None)
		{
			return FW_W_ICON_TITLE(window);
		}
	}
	return FW_W_FRAME(window);
}

/* XRestackWindows takes int. Refuse unrepresentable totals before addition;
 * refusal leaves the caller's retained count unchanged. */
static inline Bool fvwm_stack_window_count_add(
	size_t *count, const FvwmWindow *window)
{
	size_t added = fvwm_stack_window_count(window);

	if (*count > INT_MAX || added > (size_t)INT_MAX - *count)
	{
		return False;
	}
	*count += added;
	return True;
}

/* No write on refusal. The caller owns storage and submits only [0, used). */
static inline Bool fvwm_stack_window_append(
	Window *list, size_t capacity, size_t *used, const FvwmWindow *window)
{
	size_t added = fvwm_stack_window_count(window);

	if (*used > capacity || added > capacity - *used ||
	    *used > INT_MAX || added > (size_t)INT_MAX - *used)
	{
		return False;
	}
	list[(*used)++] = FW_W_FRAME(window);
	if (IS_ICONIFIED(window) && !IS_ICON_SUPPRESSED(window))
	{
		if (FW_W_ICON_TITLE(window) != None)
		{
			list[(*used)++] = FW_W_ICON_TITLE(window);
		}
		if (FW_W_ICON_PIXMAP(window) != None)
		{
			list[(*used)++] = FW_W_ICON_PIXMAP(window);
		}
	}
	return True;
}

/* Each follow-on batch repeats the previous bottom XID as its first, unchanged
 * anchor. This preserves one top-to-bottom restack across bounded Xlib calls.
 * The cursor borrows the stable ring only for this synchronous operation. */
#define FVWM_STACK_WINDOW_BATCH_CAPACITY 64

typedef struct
{
	Window windows[FVWM_STACK_WINDOW_BATCH_CAPACITY];
	size_t used;
	const FvwmWindow *next;
	const FvwmWindow *end;
} fvwm_stack_window_batch;

static inline void fvwm_stack_window_batch_init(
	fvwm_stack_window_batch *batch, const FvwmWindow *first,
	const FvwmWindow *end)
{
	batch->used = 0;
	batch->next = first;
	batch->end = end;
}

static inline Bool fvwm_stack_window_batch_next(fvwm_stack_window_batch *batch)
{
	if (batch->next == batch->end)
	{
		return False;
	}
	if (batch->used != 0)
	{
		batch->windows[0] = batch->windows[batch->used - 1];
		batch->used = 1;
	}
	while (batch->next != batch->end &&
	       fvwm_stack_window_append(batch->windows,
		       FVWM_STACK_WINDOW_BATCH_CAPACITY, &batch->used,
		       batch->next))
	{
		batch->next = batch->next->stack_next;
	}
	return True;
}

#endif
