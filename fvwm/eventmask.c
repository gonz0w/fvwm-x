/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "eventmask.h"
#include "fvwm.h"
#include "externs.h"
#include "libs/FEvent.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static struct startup_group_state startup_groups;
static unsigned int eventmask_scope_depth;

_Static_assert(sizeof(startup_groups) + sizeof(eventmask_scope_depth) +
	FEV_DESTROY_BATCH_MAX * sizeof(XEvent) <= STARTUP_GROUP_STATE_MAX,
	"startup group storage and queue scratch exceed their fixed budget");

void eventmask_initialize(void)
{
	startup_group_init(&startup_groups, PropertyChangeMask, StructureNotifyMask);
	eventmask_scope_depth = 0;
}

struct startup_group_state *eventmask_startup_groups(void)
{
	return &startup_groups;
}

long eventmask_base(Window window, long server_mask)
{
	const struct startup_group_leader *leader =
		startup_group_find_leader(&startup_groups, window);

	return leader != NULL && leader->base_known ? leader->base_mask : server_mask;
}

static long compose_base(Window window, long base_mask)
{
	struct startup_group_leader *leader =
		startup_group_find_leader(&startup_groups, window);

	if (leader == NULL)
		return base_mask;
	startup_group_base(leader, (uint32_t)base_mask);
	return startup_group_effective(&startup_groups, leader);
}

void eventmask_select(Window window, long base_mask)
{
	XSelectInput(dpy, window, compose_base(window, base_mask));
}

void eventmask_change_attributes(Window window, unsigned long value_mask,
	const XSetWindowAttributes *attributes)
{
	XSetWindowAttributes copy = *attributes;

	if ((value_mask & CWEventMask) != 0)
		copy.event_mask = compose_base(window, copy.event_mask);
	XChangeWindowAttributes(dpy, window, value_mask, &copy);
}

void eventmask_suppress_begin(Window window, long server_mask, long suppressed,
	struct eventmask_scope *scope)
{
	struct startup_group_leader *leader =
		startup_group_find_leader(&startup_groups, window);

	memset(scope, 0, sizeof(*scope));
	scope->window = window;
	scope->base_mask = eventmask_base(window, server_mask);
	scope->suppressed = (uint32_t)suppressed;
	assert(eventmask_scope_depth != UINT_MAX);
	eventmask_scope_depth++;
	if (leader != NULL)
	{
		if (!leader->base_known)
			startup_group_base(leader, (uint32_t)scope->base_mask);
		scope->serial = leader->serial;
		scope->tracked = True;
		scope->counted = startup_group_suppress(leader, scope->suppressed);
		/* These call sites suppress one bit. Exhausted nesting means that
		 * bit is already suppressed by all retained scopes. */
		XSelectInput(dpy, window,
			startup_group_effective(&startup_groups, leader));
	}
	else
		XSelectInput(dpy, window, scope->base_mask & ~suppressed);
}

void eventmask_suppress_end(struct eventmask_scope *scope)
{
	struct startup_group_leader *leader =
		startup_group_find_leader(&startup_groups, scope->window);

	assert(eventmask_scope_depth != 0);
	if (scope->tracked)
	{
		/* Actual destruction drops the record without restoration. A
		 * later acquisition of a reused XID owns another serial. */
		if (leader != NULL && leader->serial == scope->serial)
		{
			if (scope->counted)
				startup_group_unsuppress(leader, scope->suppressed);
			XSelectInput(dpy, scope->window,
				startup_group_effective(&startup_groups, leader));
		}
	}
	else
	{
		/* These short regions dispatch no events or commands. Acquisition
		 * is forbidden until the outer turn. */
		XSelectInput(dpy, scope->window,
			leader != NULL && leader->base_known ?
			startup_group_effective(&startup_groups, leader) :
			scope->base_mask);
	}
	eventmask_scope_depth--;
	scope->window = None;
}

Bool eventmask_outer_ready(void)
{
	return eventmask_scope_depth == 0;
}
