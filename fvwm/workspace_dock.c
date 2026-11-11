/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "execcontext.h"
#include "commands.h"
#include "externs.h"
#include "screen.h"
#include "workspace_docking_policy.h"
#include "workspace_placement.h"
#include "libs/monitor_types.h"
#include "libs/Parse.h"
#include "libs/Strings.h"
#include "libs/log.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define DOCK_PROFILE_MAX 8
#define DOCK_PROFILE_NAME_MAX 32

struct dock_record {
	struct workspace_placement_target guard;
	int64_t offset_x, offset_y;
};

struct dock_profile {
	char name[DOCK_PROFILE_NAME_MAX + 1];
	char connector[WORKSPACE_DOCKING_NAME_MAX + 1];
	size_t count;
	struct dock_record records[WORKSPACE_LAYOUT_MAX];
};

static struct dock_profile profiles[DOCK_PROFILE_MAX];
static bool dock_active;

static struct dock_profile *find_profile(const char *name, bool allow_empty)
{
	struct dock_profile *empty = NULL;

	for (size_t i = 0; i < DOCK_PROFILE_MAX; i++)
	{
		if (!strcmp(profiles[i].name, name))
			return &profiles[i];
		if (!profiles[i].name[0] && !empty)
			empty = &profiles[i];
	}
	return allow_empty ? empty : NULL;
}

/* Resolve names only for outputs present in the committed snapshot. Failed
 * queries leave the connector unknown, so they cannot authorize fallback. */
static bool connector_facts(const monitor_topology_snapshot *snapshot,
	struct workspace_docking_connector *facts, size_t *count)
{
	XRRScreenResources *resources;
	size_t output_count = 0, used = 0;
	bool complete = false;

	*count = 0;
	for (int i = 0; i < snapshot->monitor_count; i++)
	{
		int outputs = snapshot->monitors[i].output_count;

		if (outputs < 0 || (size_t)outputs >
		    WORKSPACE_DOCKING_CONNECTOR_MAX - output_count)
			return false;
		output_count += (size_t)outputs;
	}
	if (!output_count)
		return true;
	resources = XRRGetScreenResourcesCurrent(dpy, Scr.Root);
	if (!resources)
		return false;
	for (int i = 0; i < snapshot->monitor_count; i++)
	{
		const monitor_topology_monitor *monitor =
			&snapshot->monitors[i];

		for (int j = 0; j < monitor->output_count; j++)
		{
			XRROutputInfo *info = XRRGetOutputInfo(dpy, resources,
				monitor->outputs[j]);
			bool valid;

			if (!info)
				goto done;
			valid = info->connection == RR_Connected &&
				info->name && info->nameLen > 0 &&
				info->nameLen <= WORKSPACE_DOCKING_NAME_MAX &&
				!memchr(info->name, '\0',
					(size_t)info->nameLen);
			if (valid)
			{
				memcpy(facts[used].name, info->name,
					(size_t)info->nameLen);
				facts[used].name[info->nameLen] = '\0';
				facts[used].role = monitor->name_atom;
				used++;
			}
			XRRFreeOutputInfo(info);
			if (!valid)
				goto done;
		}
	}
	*count = used;
	complete = true;
done:
	XRRFreeScreenResources(resources);
	return complete;
}

static uint64_t primary_role(const monitor_topology_snapshot *snapshot)
{
	uint64_t primary = 0;

	for (int i = 0; i < snapshot->monitor_count; i++)
	{
		const monitor_topology_monitor *monitor =
			&snapshot->monitors[i];

		if (!monitor->primary)
			continue;
		if (primary)
			return 0;
		primary = monitor->name_atom;
	}
	return primary;
}

static bool resolve_destination(const char *connector,
	struct workspace_placement_destination *destination, bool *fallback)
{
	const monitor_topology_snapshot *snapshot =
		FScreenGetTopologySnapshot();
	struct workspace_docking_connector
		facts[WORKSPACE_DOCKING_CONNECTOR_MAX];
	enum workspace_docking_result result;
	struct monitor *m;
	size_t count, scanned = 0;
	uint64_t epoch, role;

	*fallback = false;
	if (!snapshot || snapshot->monitor_count <= 0 ||
	    snapshot->monitor_count > WORKSPACE_DOCKING_CONNECTOR_MAX ||
	    !connector_facts(snapshot, facts, &count))
	{
		fvwm_debug(__func__,
			"WorkspaceDock: connector facts unavailable");
		return false;
	}
	epoch = snapshot->epoch;
	result = workspace_docking_resolve(connector, facts, count, &role);
	if (result == WORKSPACE_DOCKING_MISSING)
	{
		role = primary_role(snapshot);
		if (!role)
		{
			fvwm_debug(__func__,
				"WorkspaceDock: connector missing and "
				"no unique primary");
			return false;
		}
		*fallback = true;
	}
	else if (result != WORKSPACE_DOCKING_MATCH)
	{
		fvwm_debug(__func__, "WorkspaceDock: connector %s",
			result == WORKSPACE_DOCKING_AMBIGUOUS ?
				"ambiguous" : "invalid");
		return false;
	}
	RB_FOREACH(m, monitors, &monitor_q)
	{
		if (++scanned > WORKSPACE_DOCKING_CONNECTOR_MAX)
			break;
		if (m->si && !(m->flags & MONITOR_DISABLED) &&
		    m->si->name_atom == role &&
		    workspace_placement_destination(m, destination) &&
		    destination->epoch == epoch &&
		    destination->role == role)
			return true;
	}
	fvwm_debug(__func__,
		"WorkspaceDock: destination work area unavailable");
	return false;
}

static bool capture_profile(const char *name, const char *connector)
{
	struct dock_profile *profile = find_profile(name, true);
	struct workspace_placement_destination source;
	struct workspace_placement_target targets[WORKSPACE_LAYOUT_MAX];
	struct monitor *m = monitor_get_current();
	struct dock_profile captured = {0};

	if (!profile)
	{
		fvwm_debug(__func__, "WorkspaceDock: profile limit reached");
		return false;
	}
	if (!workspace_placement_destination(m, &source) ||
	    !workspace_placement_collect(m, targets, &captured.count) ||
	    !captured.count)
	{
		fvwm_debug(__func__,
			"WorkspaceDock: capture scope unavailable");
		return false;
	}
	strcpy(captured.name, name);
	strcpy(captured.connector, connector);
	for (size_t i = 0; i < captured.count; i++)
	{
		captured.records[i].guard = targets[i];
		captured.records[i].offset_x =
			(int64_t)targets[i].frame.x - source.area.x;
		captured.records[i].offset_y =
			(int64_t)targets[i].frame.y - source.area.y;
	}
	*profile = captured;
	fvwm_debug(__func__, "WorkspaceDock: captured %s (%zu windows)",
		profile->name, profile->count);
	return true;
}

static int translated_origin(int origin, int64_t offset)
{
	int64_t position = (int64_t)origin + offset;

	/* The shared placement path clamps this point to the work area. */
	if (position < INT_MIN)
		return INT_MIN;
	return position > INT_MAX ? INT_MAX : (int)position;
}

static bool restore_profile(const char *name)
{
	struct dock_profile *profile = find_profile(name, false);
	struct workspace_placement_destination destination;
	size_t applied = 0;
	bool fallback;

	if (!profile)
	{
		fvwm_debug(__func__, "WorkspaceDock: profile %s not found",
			name);
		return false;
	}
	if (!resolve_destination(profile->connector, &destination, &fallback))
		return false;
	for (size_t i = 0; i < profile->count; i++)
	{
		struct dock_record *record = &profile->records[i];
		struct workspace_placement_target expected = record->guard;
		struct workspace_placement_target observed;
		rectangle frame = expected.frame;

		frame.x = translated_origin(destination.area.x,
			record->offset_x);
		frame.y = translated_origin(destination.area.y,
			record->offset_y);
		if (workspace_placement_apply(&expected, &destination, frame,
		    WORKSPACE_PLACEMENT_DOCK, &observed))
		{
			record->guard = observed;
			applied++;
		}
	}
	fvwm_debug(__func__, "WorkspaceDock: restored %s (%zu applied, "
		"%zu skipped%s)", name, applied, profile->count - applied,
		fallback ? ", primary fallback" : "");
	return applied != 0;
}

void CMD_WorkspaceDock(F_CMD_ARGS)
{
	char *verb = NULL, *name = NULL, *connector = NULL, *extra = NULL;
	char *rest;
	uint64_t unused;
	bool applied = false;

	(void)exc;
	(void)pc;
	if (cond_rc)
		cond_rc->rc = COND_RC_ERROR;
	if (dock_active)
	{
		fvwm_debug(__func__, "WorkspaceDock: operation already active");
		return;
	}
	if (!action || strnlen(action, MAX_TOKEN_LENGTH + 1) > MAX_TOKEN_LENGTH)
		goto usage;
	rest = GetNextToken(action, &verb);
	rest = GetNextToken(rest, &name);
	rest = GetNextToken(rest, &connector);
	(void)GetNextToken(rest, &extra);
	if (!verb || !name || !name[0] ||
	    strlen(name) > DOCK_PROFILE_NAME_MAX ||
	    strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		"0123456789_-") != strlen(name) || extra)
		goto usage;
	dock_active = true;
	if (StrEquals(verb, "Capture") && connector &&
	    workspace_docking_resolve(connector, NULL, 0, &unused) ==
		    WORKSPACE_DOCKING_MISSING)
		applied = capture_profile(name, connector);
	else if (StrEquals(verb, "Restore") && !connector)
		applied = restore_profile(name);
	else
	{
		dock_active = false;
		goto usage;
	}
	dock_active = false;
	if (cond_rc && applied)
		cond_rc->rc = COND_RC_OK;
	goto done;
usage:
	fvwm_debug(__func__, "WorkspaceDock: expected Capture NAME CONNECTOR "
		"or Restore NAME");
done:
	free(verb);
	free(name);
	free(connector);
	free(extra);
}
