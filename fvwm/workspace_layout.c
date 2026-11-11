/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "workspace_placement.h"
#include "execcontext.h"
#include "commands.h"
#include "screen.h"
#include "libs/Parse.h"
#include "libs/log.h"
#include <stdlib.h>
#include <string.h>

#define LAYOUT_SLOTS 8

struct layout_entry {
	struct workspace_placement_target applied;
	rectangle original;
};

struct layout_slot {
	struct workspace_placement_destination destination;
	size_t count;
	struct layout_entry entries[WORKSPACE_LAYOUT_MAX];
};

static struct layout_slot layouts[LAYOUT_SLOTS];

static struct layout_slot *layout_slot(
	const struct workspace_placement_destination *destination)
{
	struct layout_slot *empty = NULL;

	for (size_t i = 0; i < LAYOUT_SLOTS; i++)
	{
		struct layout_slot *slot = &layouts[i];
		const struct workspace_placement_destination *prior =
			&slot->destination;

		if (!slot->count || prior->epoch != destination->epoch)
		{
			slot->count = 0;
			if (!empty)
				empty = slot;
		}
		else if (prior->role == destination->role &&
		    prior->desk == destination->desk &&
		    prior->viewport_x == destination->viewport_x &&
		    prior->viewport_y == destination->viewport_y)
			return slot;
	}
	return empty;
}

static const struct layout_entry *owned_entry(const struct layout_slot *slot,
	const struct workspace_placement_target *current)
{
	for (size_t i = 0; i < slot->count; i++)
	{
		const struct layout_entry *entry = &slot->entries[i];

		if (entry->applied.identity == current->identity &&
		    entry->applied.revision == current->revision)
			return entry;
	}
	return NULL;
}

void CMD_WorkspaceLayout(F_CMD_ARGS)
{
	struct workspace_placement_destination destination;
	struct workspace_placement_target targets[WORKSPACE_LAYOUT_MAX];
	struct workspace_layout_plan plan;
	struct layout_slot *slot;
	struct layout_slot next = {0};
	struct monitor *monitor = monitor_get_current();
	enum workspace_layout_kind kind;
	char *mode = NULL;
	char *extra = NULL;
	char *rest;
	size_t count;
	bool floating;

	(void)exc;
	(void)pc;
	if (cond_rc)
		cond_rc->rc = COND_RC_ERROR;
	if (!action || strnlen(action, MAX_TOKEN_LENGTH + 1) > MAX_TOKEN_LENGTH)
		goto invalid;
	rest = GetNextToken(action, &mode);
	GetNextToken(rest, &extra);
	if (!mode || extra)
		goto invalid;
	floating = strcmp(mode, "floating") == 0;
	if (floating || strcmp(mode, "columns") == 0)
		kind = WORKSPACE_LAYOUT_COLUMNS;
	else if (strcmp(mode, "master-stack") == 0)
		kind = WORKSPACE_LAYOUT_MASTER_STACK;
	else
		goto invalid;
	free(mode);
	if (!workspace_placement_destination(monitor, &destination) ||
	    !(slot = layout_slot(&destination)))
	{
		fvwm_debug("WorkspaceLayout", "no available workspace layout slot");
		return;
	}
	if (floating)
	{
		for (size_t i = 0; i < slot->count; i++)
		{
			struct workspace_placement_target observed;

			workspace_placement_apply(&slot->entries[i].applied,
				&destination, slot->entries[i].original,
				WORKSPACE_PLACEMENT_FLOAT, &observed);
		}
		slot->count = 0;
		if (cond_rc)
			cond_rc->rc = COND_RC_OK;
		return;
	}
	if (!workspace_placement_collect(monitor, targets, &count) ||
	    !workspace_layout_plan(kind, destination.area, count, &plan))
	{
		fvwm_debug("WorkspaceLayout", "no bounded layout for this work area");
		return;
	}
	next.destination = destination;
	for (size_t i = 0; i < count; i++)
	{
		const struct layout_entry *prior = owned_entry(slot, &targets[i]);
		struct layout_entry entry = {
			.original = prior ? prior->original : targets[i].frame};

		if (workspace_placement_apply(&targets[i], &destination,
			plan.cells[i], WORKSPACE_PLACEMENT_TILE, &entry.applied))
			next.entries[next.count++] = entry;
		else if (prior)
			next.entries[next.count++] = *prior;
	}
	*slot = next;
	if (cond_rc && next.count)
		cond_rc->rc = COND_RC_OK;
	return;

invalid:
	free(extra);
	free(mode);
	fvwm_debug("WorkspaceLayout", "expected columns, master-stack or floating");
}
