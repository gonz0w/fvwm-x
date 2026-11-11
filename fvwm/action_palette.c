/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "action_palette.h"
#include "fvwm.h"
#include "execcontext.h"
#include "screen.h"
#include "query_json.h"
#include "focus.h"
#include "libs/monitor_types.h"
#include "libs/safemalloc.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Metadata is optional and allocated lazily. The four fixed slot headers plus
 * all four full row arrays fit PALETTE_BUDGET_MAX (policy static assertion).
 * No window, monitor or source string pointer survives snapshot preparation. */
static struct palette_slot palette_slots[PALETTE_OWNER_MAX];
static uint64_t palette_next_snapshot = 1;

static void palette_slot_clear(struct palette_slot *slot)
{
	free(slot->rows);
	memset(slot, 0, sizeof(*slot));
}

void action_palette_owner_gone(struct fmodule *owner)
{
	for (int i = 0; i < PALETTE_OWNER_MAX; i++)
	{
		if (palette_slots[i].owner == (uintptr_t)owner)
			palette_slot_clear(&palette_slots[i]);
	}
}

static struct palette_slot *palette_find(struct fmodule *owner,
	const char *cookie, const char *generation)
{
	for (int i = 0; i < PALETTE_OWNER_MAX; i++)
	{
		if (palette_slot_matches(&palette_slots[i], (uintptr_t)owner,
			cookie, generation))
			return &palette_slots[i];
	}
	return NULL;
}

static bool palette_window_eligible(const FvwmWindow *fw)
{
	return fw->palette_identity && fw->m &&
		!IS_SCHEDULED_FOR_DESTROY(fw) && !DO_SKIP_WINDOW_LIST(fw);
}

static FvwmWindow *palette_window_find(uint64_t identity)
{
	FvwmWindow *fw;
	size_t scanned = 0;

	if (!identity)
		return NULL;
	for (fw = Scr.FvwmRoot.next; fw && scanned < PALETTE_SCAN_MAX;
	     fw = fw->next, scanned++)
	{
		if (fw->palette_identity == identity &&
		    palette_window_eligible(fw))
			return fw;
	}
	return NULL;
}

static void palette_copy_text(struct palette_slot *slot,
	char output[PALETTE_TEXT_MAX + 1], const char *input,
	const char *fallback)
{
	if (!palette_text(output, input, fallback))
		slot->truncated = true;
}

/* Frame positions are viewport-relative; page facts include virtual offset. */
static int palette_page(int coordinate, int offset, int extent)
{
	int64_t value = (int64_t)coordinate + offset;

	if (extent <= 0)
		return 0;
	value = value >= 0 ? value / extent :
		-((-value + extent - 1) / extent);
	return value > INT_MAX ? INT_MAX : value < INT_MIN ? INT_MIN : (int)value;
}

static void palette_prepare_windows(struct palette_slot *slot)
{
	FvwmWindow *fw;
	size_t scanned = 0;

	for (fw = Scr.FvwmRoot.next; fw && scanned < PALETTE_SCAN_MAX;
	     fw = fw->next, scanned++)
	{
		struct palette_row *row;
		char detail[256];

		if (!palette_window_eligible(fw))
			continue;
		if (slot->count == PALETTE_WINDOW_MAX)
		{
			slot->truncated = true;
			continue;
		}
		row = &slot->rows[slot->count++];
		memset(row, 0, sizeof(*row));
		row->kind = PALETTE_WINDOW;
		row->identity = fw->palette_identity;
		row->placement_revision = fw->placement_revision;
		row->desk = fw->Desk;
		row->output = fw->m->si ? fw->m->si->rr_output : 0;
		row->role = fw->m->si ? fw->m->si->name_atom : 0;
		row->x = fw->g.frame.x;
		row->y = fw->g.frame.y;
		row->width = fw->g.frame.width;
		row->height = fw->g.frame.height;
		row->page_x = palette_page(fw->g.frame.x,
			fw->m->virtual_scr.Vx, monitor_get_all_widths());
		row->page_y = palette_page(fw->g.frame.y,
			fw->m->virtual_scr.Vy, monitor_get_all_heights());
		row->flags = (IS_STICKY_ACROSS_DESKS(fw) ?
			PALETTE_STICKY_DESKS : 0) |
			(IS_STICKY_ACROSS_PAGES(fw) ? PALETTE_STICKY_PAGES : 0) |
			(IS_ICONIFIED(fw) ? PALETTE_ICONIFIED : 0) |
			(IS_EWMH_FULLSCREEN(fw) ? PALETTE_FULLSCREEN : 0) |
			(focus_is_focused(fw) ? PALETTE_FOCUSED : 0);
		palette_copy_text(slot, row->label,
			fw->visible_name ? fw->visible_name : fw->name.name,
			"Unnamed window");
		snprintf(detail, sizeof(detail), "Window; desktop %d; %.160s",
			fw->Desk, fw->m->si && fw->m->si->name ?
			fw->m->si->name : "unknown monitor");
		palette_copy_text(slot, row->detail, detail, "Window");
	}
	if (fw)
		slot->truncated = true;
}

static void palette_prepare_desktops(struct palette_slot *slot)
{
	struct monitor *m;
	size_t desktops = 0, monitors = 0, names = 0;

	RB_FOREACH(m, monitors, &monitor_q)
	{
		size_t first = slot->count, count;
		DesktopsInfo *desk;

		if (++monitors > MODULE_QUERY_MONITORS_MAX)
		{
			slot->truncated = true;
			break;
		}
		if (!m->si || (m->flags & MONITOR_DISABLED))
			continue;
		count = m->ewmhc.CurrentNumberOfDesktops;
		if (count > PALETTE_DESKTOP_MAX - desktops)
		{
			count = PALETTE_DESKTOP_MAX - desktops;
			slot->truncated = true;
		}
		for (size_t i = 0; i < count; i++)
		{
			struct palette_row *row = &slot->rows[slot->count++];
			char label[64], detail[256];

			memset(row, 0, sizeof(*row));
			row->kind = PALETTE_DESKTOP;
			row->desk = (int)i;
			row->output = m->si->rr_output;
			row->role = m->si->name_atom;
			row->page_max_x = monitor_get_all_widths() > 0 ?
				m->virtual_scr.VxMax / monitor_get_all_widths() : 0;
			row->page_max_y = monitor_get_all_heights() > 0 ?
				m->virtual_scr.VyMax / monitor_get_all_heights() : 0;
			snprintf(label, sizeof(label), "Desktop %zu", i);
			palette_copy_text(slot, row->label, label, "Desktop");
			snprintf(detail, sizeof(detail), "Desktop %zu; %.160s",
				i, m->si->name ?
				m->si->name : "unknown monitor");
			palette_copy_text(slot, row->detail, detail, "Desktop");
		}
		desktops += count;
		/* One bounded walk supplies configured labels, including sparse
		 * DesktopName declarations, without repeated list searches. */
		for (desk = m->Desktops ? m->Desktops->next : NULL;
		     desk && names < PALETTE_SCAN_MAX;
		     desk = desk->next, names++)
		{
			if (desk->desk >= 0 && (size_t)desk->desk < count)
				palette_copy_text(slot,
					slot->rows[first + desk->desk].label,
					desk->name, "Unnamed desktop");
		}
		if (desk)
			slot->truncated = true;
	}
}

static struct palette_slot *palette_prepare(struct fmodule *owner,
	const struct fvwm_query_spec *spec, const char **error)
{
	const monitor_topology_snapshot *topology =
		FScreenGetTopologySnapshot();
	char generation[49];
	uint64_t identity;
	int index = palette_slot_select(palette_slots, (uintptr_t)owner);
	struct palette_slot *slot;

	*error = "palette_busy";
	if (index < 0)
		return NULL;
	*error = "generation_unavailable";
	if (!topology ||
	    !palette_next_identity(&palette_next_snapshot, &identity) ||
	    !fvwm_query_token(identity, generation))
		return NULL;
	slot = &palette_slots[index];
	/* New cookie replaces this module's preceding view; old close cannot
	 * release it because releases match both module and cookie. */
	if (!slot->rows)
		slot->rows = fxmalloc(PALETTE_ROW_MAX * sizeof(*slot->rows));
	slot->owner = (uintptr_t)owner;
	strcpy(slot->cookie, spec->cookie);
	strcpy(slot->generation, generation);
	slot->topology = topology->epoch;
	slot->count = 0;
	slot->truncated = false;
	palette_prepare_windows(slot);
	palette_prepare_desktops(slot);
	return slot;
}

cJSON *action_palette_query(struct fmodule *owner,
	const struct fvwm_query_spec *spec, const char **error)
{
	struct palette_slot *slot;
	cJSON *result = NULL, *rows;
	size_t end;

	*error = "stale_generation";
	if (!spec->offset && strcmp(spec->generation, "-") == 0)
		slot = palette_prepare(owner, spec, error);
	else
		slot = palette_find(owner, spec->cookie, spec->generation);
	if (!slot)
		return NULL;
	*error = "invalid_offset";
	if (!palette_page_valid(spec->offset, spec->limit, slot->count))
		return NULL;
	end = spec->offset + spec->limit;
	if (end > slot->count)
		end = slot->count;
	*error = "allocation_failed";
	result = cJSON_CreateObject();
	if (!result)
		return NULL;
	rows = cJSON_AddArrayToObject(result, "rows");
	if (!rows ||
	    !cJSON_AddStringToObject(result, "state", "observed") ||
	    !cJSON_AddStringToObject(result, "scope", "core-palette") ||
	    !cJSON_AddStringToObject(result, "generation", slot->generation) ||
	    !cJSON_AddNumberToObject(result, "offset", spec->offset) ||
	    !(end < slot->count ?
		cJSON_AddNumberToObject(result, "next_offset", end) :
		cJSON_AddNullToObject(result, "next_offset")) ||
	    !cJSON_AddNumberToObject(result, "total", slot->count) ||
	    !cJSON_AddBoolToObject(result, "truncated", slot->truncated) ||
	    !cJSON_AddStringToObject(result, "coverage",
		"managed windows excluding SkipWindowList; "
		"advertised desktops"))
		goto fail;
	for (size_t i = spec->offset; i < end; i++)
	{
		const struct palette_row *row = &slot->rows[i];
		char id[PALETTE_ID_MAX + 1], output[32], role[32];
		cJSON *item = cJSON_CreateObject();

		if (!item)
			goto fail;
		if (!cJSON_AddItemToArray(rows, item))
		{
			cJSON_Delete(item);
			goto fail;
		}
		palette_row_id(id, slot->generation, row);
		snprintf(output, sizeof(output), "%" PRIu64, row->output);
		snprintf(role, sizeof(role), "%" PRIu64, row->role);
		if (!cJSON_AddStringToObject(item, "id", id) ||
		    !cJSON_AddNumberToObject(item, "index", i) ||
		    !cJSON_AddStringToObject(item, "kind",
			row->kind == PALETTE_WINDOW ? "window" : "desktop") ||
		    !cJSON_AddStringToObject(item, "label", row->label) ||
		    !cJSON_AddStringToObject(item, "detail", row->detail) ||
		    !cJSON_AddStringToObject(item, "icon",
			row->kind == PALETTE_WINDOW ? "file" : "directory") ||
		    !cJSON_AddBoolToObject(item, "enabled", true) ||
		    !cJSON_AddNumberToObject(item, "desk", row->desk) ||
		    !cJSON_AddStringToObject(item, "monitor_output", output) ||
		    !cJSON_AddStringToObject(item, "monitor_role", role) ||
		    !cJSON_AddNumberToObject(item, "page_x", row->page_x) ||
		    !cJSON_AddNumberToObject(item, "page_y", row->page_y) ||
		    !cJSON_AddNumberToObject(item, "x", row->x) ||
		    !cJSON_AddNumberToObject(item, "y", row->y) ||
		    !cJSON_AddNumberToObject(item, "width", row->width) ||
		    !cJSON_AddNumberToObject(item, "height", row->height) ||
		    !cJSON_AddNumberToObject(item, "page_max_x", row->page_max_x) ||
		    !cJSON_AddNumberToObject(item, "page_max_y", row->page_max_y) ||
		    !cJSON_AddBoolToObject(item, "sticky_desks",
			(row->flags & PALETTE_STICKY_DESKS) != 0) ||
		    !cJSON_AddBoolToObject(item, "sticky_pages",
			(row->flags & PALETTE_STICKY_PAGES) != 0) ||
		    !cJSON_AddBoolToObject(item, "iconified",
			(row->flags & PALETTE_ICONIFIED) != 0) ||
		    !cJSON_AddBoolToObject(item, "fullscreen",
			(row->flags & PALETTE_FULLSCREEN) != 0) ||
		    !cJSON_AddBoolToObject(item, "focused",
			(row->flags & PALETTE_FOCUSED) != 0))
			goto fail;
	}
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

static struct monitor *palette_desktop_find(const struct palette_row *row)
{
	struct monitor *m;
	size_t scanned = 0;

	RB_FOREACH(m, monitors, &monitor_q)
	{
		if (++scanned > MODULE_QUERY_MONITORS_MAX)
			break;
		if (m->si && !(m->flags & MONITOR_DISABLED) &&
		    m->si->rr_output == row->output &&
		    m->si->name_atom == row->role && row->desk >= 0 &&
		    (unsigned int)row->desk <
			m->ewmhc.CurrentNumberOfDesktops &&
		    m->number >= 0)
			return m;
	}
	return NULL;
}

bool action_palette_target(struct fmodule *owner,
	const struct palette_request *request, struct palette_row *row,
	char id[PALETTE_ID_MAX + 1], const char **error)
{
	struct palette_slot *slot;
	const monitor_topology_snapshot *topology;

	memset(row, 0, sizeof(*row));
	id[0] = '\0';
	*error = "stale_generation";
	slot = palette_find(owner, request->cookie,
		request->release ? NULL : request->generation);
	if (request->release)
	{
		if (slot)
			palette_slot_clear(slot);
		*error = NULL;
		return true;
	}
	if (!slot)
		return false;
	*error = "invalid_target";
	if (request->index >= slot->count)
		return false;
	*row = slot->rows[request->index];
	*error = "stale_target";
	if (row->kind == PALETTE_WINDOW)
	{
		FvwmWindow *fw = palette_window_find(row->identity);

		if (!fw)
			return false;
		if (request->operation != PALETTE_SELECT &&
		    (fw->placement_revision == UINT32_MAX ||
		     fw->placement_revision != row->placement_revision))
		{
			*error = "stale_placement";
			return false;
		}
	}
	else
	{
		topology = FScreenGetTopologySnapshot();
		if (!topology || topology->epoch != slot->topology ||
		    !palette_desktop_find(row))
			return false;
	}
	palette_row_id(id, slot->generation, row);
	row->operation = request->operation;
	row->topology = slot->topology;
	if (request->operation != PALETTE_SELECT)
	{
		*error = "unsupported_target";
		if (row->kind != PALETTE_WINDOW)
			return false;
		topology = FScreenGetTopologySnapshot();
		*error = "stale_topology";
		if (!topology || topology->epoch != slot->topology)
			return false;
		if (request->operation == PALETTE_MOVE)
		{
			const struct palette_row *destination;

			*error = "invalid_destination";
			if (request->destination >= slot->count)
				return false;
			destination = &slot->rows[request->destination];
			if (destination->kind != PALETTE_DESKTOP ||
			    !palette_desktop_find(destination))
				return false;
			row->output = destination->output;
			row->role = destination->role;
			row->desk = destination->desk;
			row->page_x = (int)request->page_x;
			row->page_y = (int)request->page_y;
		}
	}
	*error = NULL;
	return true;
}

bool action_palette_command(const struct palette_row *row, unsigned int step,
	char *command, size_t size, const char **error)
{
	*error = "stale_target";
	command[0] = '\0';
	if (row->kind == PALETTE_WINDOW)
	{
		static const char *verbs[] = {"Iconify off", "Raise", "Focus", "Move"};
		FvwmWindow *fw = palette_window_find(row->identity);

		if (!fw)
			return false;
		if (row->operation != PALETTE_SELECT)
		{
			const monitor_topology_snapshot *topology =
				FScreenGetTopologySnapshot();

			if (step == 0 && (fw->placement_revision == UINT32_MAX ||
			    fw->placement_revision != row->placement_revision))
			{
				*error = "stale_placement";
				return false;
			}
			*error = "unsupported_target";
			if (IS_TRANSIENT(fw) || IS_STICKY_ACROSS_DESKS(fw) ||
			    IS_STICKY_ACROSS_PAGES(fw) || IS_EWMH_FULLSCREEN(fw))
				return false;
			*error = "stale_topology";
			if (!topology || topology->epoch != row->topology)
				return false;
		}
		if (row->operation == PALETTE_MOVE)
		{
			struct monitor *m = palette_desktop_find(row);
			int width = monitor_get_all_widths();
			int height = monitor_get_all_heights();
			char number[32];

			*error = "invalid_destination";
			if (!m || width <= 0 || height <= 0 || step > 1 ||
			    row->page_x < 0 || row->page_y < 0 ||
			    row->page_x > m->virtual_scr.VxMax / width ||
			    row->page_y > m->virtual_scr.VyMax / height)
				return false;
			snprintf(number, sizeof(number), "%d", m->number);
			if (monitor_resolve_name(number) != m)
				return false;
			if (!step)
				snprintf(command, size, "WindowId 0x%lx MoveToDesk 0 %d",
					FW_W(fw), row->desk);
			else
				snprintf(command, size,
					"WindowId 0x%lx MoveToPage screen %d %d %d",
					FW_W(fw), m->number, row->page_x, row->page_y);
		}
		else
		{
			if (step >= (row->operation == PALETTE_DRAG ? 4U : 3U))
				return false;
			snprintf(command, size, "WindowId 0x%lx %s",
				FW_W(fw), verbs[step]);
		}
	}
	else
	{
		struct monitor *m = palette_desktop_find(row);
		char number[32];

		if (!m || step)
			return false;
		snprintf(number, sizeof(number), "%d", m->number);
		if (monitor_resolve_name(number) != m)
			return false;
		snprintf(command, size, "GotoDesk screen %d 0 %d", m->number,
			row->desk);
	}
	*error = NULL;
	return true;
}
