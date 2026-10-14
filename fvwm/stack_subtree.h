#ifndef FVWM_STACK_SUBTREE_H
#define FVWM_STACK_SUBTREE_H

#include "stack.h"
#include "stack_rank_policy.h"

/* A usable group hint requires the style opt-in and a distinct group XID. */
static inline Bool fvwm_stack_group_hint_is_active(
	const FvwmWindow *window, Window root)
{
	return DO_ICONIFY_WINDOW_GROUPS(window) && window->wmhints != NULL &&
		(window->wmhints->flags & WindowGroupHint) &&
		window->wmhints->window_group != None &&
		window->wmhints->window_group != FW_W(window) &&
		window->wmhints->window_group != root;
}

/* The cached parent and retained ranks belong to the current stack operation. */
static inline Bool fvwm_stack_mark_parent(
	FvwmWindow *child, int mode, Bool ignore_icons)
{
	FvwmWindow *parent = child->scratch.p;

	if (IS_IN_TRANSIENT_SUBTREE(child) || !IS_TRANSIENT(child) ||
	    (ignore_icons && IS_ICONIFIED(child)) || parent == NULL ||
	    !IS_IN_TRANSIENT_SUBTREE(parent))
	{
		return False;
	}
	if (mode != MARK_ALL &&
	    !((mode == MARK_RAISE || mode == MARK_RESTACK) &&
	      DO_RAISE_TRANSIENT(parent)) &&
	    !((mode == MARK_LOWER || mode == MARK_RESTACK) &&
	      DO_LOWER_TRANSIENT(parent)))
	{
		return False;
	}
	SET_IN_TRANSIENT_SUBTREE(child, 1);
	child->scratch.i = fvwm_stack_rank_add_parent(
		child->scratch.i, parent->scratch.i);
	return True;
}

/* Complete parent-only closure of [start, end). No X work, callbacks or retained
 * references. Existing marks/ranks seed the operation. False means allocation
 * failed before any state changed; the caller can continue its original scan.
 * Window-group closure retains its separate ordering-sensitive scan. */
Bool fvwm_stack_mark_closure(
	FvwmWindow *start, FvwmWindow *end, int mode, Bool ignore_icons);

#endif
