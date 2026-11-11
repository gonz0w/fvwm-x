/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "workspace_placement.h"
#include "workspace_tools.h"
#include "palette_policy.h"
#include "execcontext.h"
#include "commands.h"
#include "decorations.h"
#include "ewmh.h"
#include "frame.h"
#include "geometry.h"
#include "module_list.h"
#include "move_resize.h"
#include "screen.h"
#include "update.h"
#include "virtual.h"
#include "libs/monitor_types.h"
#include "libs/monitor_ownership.h"
#include <limits.h>

static bool usable_area(rectangle area)
{
	return area.width > 0 && area.height > 0 &&
		area.x >= SHRT_MIN && area.y >= SHRT_MIN &&
		(int64_t)area.x + area.width - 1 <= SHRT_MAX &&
		(int64_t)area.y + area.height - 1 <= SHRT_MAX;
}

bool workspace_placement_destination(const struct monitor *monitor,
	struct workspace_placement_destination *out)
{
	const monitor_topology_snapshot *snapshot = FScreenGetTopologySnapshot();
	rectangle area;

	if (!snapshot || !monitor || !monitor->si ||
	    (monitor->flags & MONITOR_DISABLED) || !monitor->si->name_atom)
		return false;
	area = (rectangle){monitor->si->x, monitor->si->y,
		monitor->si->w, monitor->si->h};
	EWMH_GetWorkAreaIntersection((struct monitor *)monitor, &area.x,
		&area.y, &area.width, &area.height, EWMH_USE_DYNAMIC_WORKING_AREA);
	if (!usable_area(area))
		return false;
	*out = (struct workspace_placement_destination){
		.epoch = snapshot->epoch, .role = monitor->si->name_atom,
		.area = area, .desk = monitor->virtual_scr.CurrentDesk,
		.viewport_x = monitor->virtual_scr.Vx,
		.viewport_y = monitor->virtual_scr.Vy};
	return true;
}

static bool eligible(const FvwmWindow *fw)
{
	return IS_MAPPED(fw) && !IS_ICONIFIED(fw) && !IS_SHADED(fw) &&
		!IS_MAXIMIZED(fw) && !IS_EWMH_FULLSCREEN(fw) &&
		!IS_TRANSIENT(fw) && !IS_STICKY_ACROSS_DESKS(fw) &&
		!IS_STICKY_ACROSS_PAGES(fw) && !DO_SKIP_WINDOW_LIST(fw) &&
		!IS_WINDOW_BEING_MOVED_OPAQUE(fw) && fw->UpdateDesk < 0 &&
		fw->placement_revision != UINT32_MAX &&
		workspace_window_is_application(fw) &&
		is_function_allowed(F_MOVE, NULL, fw, RQORIG_PROGRAM_US, False);
}

static struct workspace_placement_target observe(const FvwmWindow *fw)
{
	return (struct workspace_placement_target){
		.identity = fw->palette_identity, .role = fw->m->si->name_atom,
		.virtual_x = (int64_t)fw->g.frame.x + fw->m->virtual_scr.Vx,
		.virtual_y = (int64_t)fw->g.frame.y + fw->m->virtual_scr.Vy,
		.revision = fw->placement_revision,
		.frame = fw->g.frame, .desk = fw->Desk};
}

bool workspace_placement_collect(const struct monitor *monitor,
	struct workspace_placement_target targets[WORKSPACE_LAYOUT_MAX],
	size_t *count)
{
	struct workspace_placement_destination destination;
	size_t n = 0, scanned = 0;

	*count = 0;
	if (!workspace_placement_destination(monitor, &destination))
		return false;
	for (FvwmWindow *fw = Scr.FvwmRoot.next; fw; fw = fw->next)
	{
		rectangle frame = fw->g.frame;
		rectangle area = destination.area;

		if (++scanned > PALETTE_SCAN_MAX)
			return false;
		if (fw->m != monitor || fw->Desk != destination.desk ||
		    frame.x >= area.x + area.width ||
		    frame.y >= area.y + area.height ||
		    (int64_t)frame.x + frame.width <= area.x ||
		    (int64_t)frame.y + frame.height <= area.y || !eligible(fw))
			continue;
		if (n == WORKSPACE_LAYOUT_MAX)
			return false;
		targets[n++] = observe(fw);
	}
	*count = n;
	return true;
}

static FvwmWindow *find_target(uint64_t identity)
{
	size_t scanned = 0;

	if (!identity)
		return NULL;
	for (FvwmWindow *fw = Scr.FvwmRoot.next;
	     fw && scanned++ < PALETTE_SCAN_MAX; fw = fw->next)
		if (fw->palette_identity == identity && eligible(fw))
			return fw;
	return NULL;
}

static bool destination_is_current(
	const struct workspace_placement_destination *expected)
{
	const monitor_topology_snapshot *snapshot = FScreenGetTopologySnapshot();
	struct monitor *monitor;

	if (!snapshot || snapshot->epoch != expected->epoch)
		return false;
	RB_FOREACH(monitor, monitors, &monitor_q)
	{
		struct workspace_placement_destination current;

		if (!monitor->si || monitor->si->name_atom != expected->role ||
		    !workspace_placement_destination(monitor, &current))
			continue;
		const rectangle *area = &expected->area;
		return current.desk == expected->desk &&
			current.viewport_x == expected->viewport_x &&
			current.viewport_y == expected->viewport_y &&
			current.area.x == area->x && current.area.y == area->y &&
			current.area.width == area->width &&
			current.area.height == area->height;
	}
	return false;
}

static int fit_axis(int position, int size, int origin, int extent)
{
	int limit = size >= extent ? origin : origin + extent - size;

	if (position < origin)
		return origin;
	return position > limit ? limit : position;
}

static void fit_frame(rectangle *frame, rectangle area)
{
	frame->x = fit_axis(frame->x, frame->width, area.x, area.width);
	frame->y = fit_axis(frame->y, frame->height, area.y, area.height);
}

static void place_frame(FvwmWindow *fw, rectangle frame)
{
	frame_setup_window(fw, frame.x, frame.y, frame.width, frame.height, True);
	update_absolute_geometry(fw);
	maximize_adjust_offset(fw);
}

bool workspace_placement_apply(const struct workspace_placement_target *expected,
	const struct workspace_placement_destination *destination,
	rectangle frame, enum workspace_placement_mode mode, struct workspace_placement_target *observed)
{
	FvwmWindow *fw = find_target(expected->identity);
	rectangle area = destination->area;
	const monitor_topology_snapshot *snapshot;
	monitor_ownership_result owner;
	struct workspace_placement_target current;

	if (!fw || fw->placement_revision != expected->revision ||
	    fw->Desk != expected->desk || !destination_is_current(destination))
		return false;
	current = observe(fw);
	if (current.role != expected->role ||
	    current.virtual_x != expected->virtual_x ||
	    current.virtual_y != expected->virtual_y ||
	    current.frame.width != expected->frame.width ||
	    current.frame.height != expected->frame.height)
		return false;
	if (mode != WORKSPACE_PLACEMENT_DOCK)
	{
		int width = frame.width;
		int height = frame.height;

		if (!is_function_allowed(F_RESIZE, NULL, fw, RQORIG_PROGRAM_US, True) ||
		    width <= 0 || height <= 0 ||
		    (mode == WORKSPACE_PLACEMENT_TILE &&
		     (width > area.width || height > area.height)))
			return false;
		constrain_size(fw, NULL, &frame.width, &frame.height, 0, 0, 0);
		/* Nearest-grid snapping can round a cell dimension up. */
		if (mode == WORKSPACE_PLACEMENT_TILE &&
		    (frame.width > width || frame.height > height))
		{
			frame.width = frame.width > width ?
				width - fw->hints.width_inc / 2 : width;
			frame.height = frame.height > height ?
				height - fw->hints.height_inc / 2 : height;
			if (frame.width <= 0 || frame.height <= 0)
				return false;
			constrain_size(fw, NULL, &frame.width, &frame.height,
				0, 0, 0);
		}
		if (frame.width <= 0 || frame.height <= 0 ||
		    (mode == WORKSPACE_PLACEMENT_TILE &&
		     (frame.width > width || frame.height > height)) ||
		    (mode == WORKSPACE_PLACEMENT_FLOAT &&
		     (frame.width != width || frame.height != height)))
			return false;
	}
	else
	{
		frame.width = current.frame.width;
		frame.height = current.frame.height;
	}
	if (mode != WORKSPACE_PLACEMENT_FLOAT)
		fit_frame(&frame, area);
	snapshot = FScreenGetTopologySnapshot();
	if (monitor_ownership_resolve(&frame, snapshot->monitors,
		snapshot->monitor_count, fw->m->si->name_atom,
		MONITOR_OWNERSHIP_HYSTERESIS_PX, &owner) != MONITOR_OWNERSHIP_OK ||
	    owner.name_atom != destination->role)
		return false;
	/* Keep synchronous configure consumers outside the placement commit. */
	module_config_interaction_begin();
	place_frame(fw, frame);
	if (fw->m->si->name_atom != destination->role)
	{
		fw->UpdateDesk = destination->desk;
		update_fvwm_monitor_preserve_client(fw);
		if (fw->m->si->name_atom != destination->role)
		{
			module_config_interaction_end();
			return false;
		}
		if (fw->Desk != current.desk)
			EWMH_SetWMDesktop(fw);
		frame = fw->g.frame;
		fit_frame(&frame, area);
		place_frame(fw, frame);
	}
	else if (fw->Desk != destination->desk)
		do_move_window_to_desk(fw, destination->desk);
	if (mode != WORKSPACE_PLACEMENT_DOCK)
		record_resized_logical_client(fw);
	*observed = observe(fw);
	module_config_interaction_end();
	return true;
}
