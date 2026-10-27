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
#include "libs/monitor_types.h"
#include "libs/safemalloc.h"
#include <inttypes.h>
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

static bool palette_window_live(uint64_t identity)
{
	return palette_window_find(identity) != NULL;
}

static void palette_copy_text(struct palette_slot *slot,
	char output[PALETTE_TEXT_MAX + 1], const char *input,
	const char *fallback)
{
	if (!palette_text(output, input, fallback))
		slot->truncated = true;
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
		char id[PALETTE_ID_MAX + 1];
		cJSON *item = cJSON_CreateObject();

		if (!item)
			goto fail;
		if (!cJSON_AddItemToArray(rows, item))
		{
			cJSON_Delete(item);
			goto fail;
		}
		palette_row_id(id, slot->generation, row);
		if (!cJSON_AddStringToObject(item, "id", id) ||
		    !cJSON_AddNumberToObject(item, "index", i) ||
		    !cJSON_AddStringToObject(item, "kind",
			row->kind == PALETTE_WINDOW ? "window" : "desktop") ||
		    !cJSON_AddStringToObject(item, "label", row->label) ||
		    !cJSON_AddStringToObject(item, "detail", row->detail) ||
		    !cJSON_AddStringToObject(item, "icon",
			row->kind == PALETTE_WINDOW ? "file" : "directory") ||
		    !cJSON_AddBoolToObject(item, "enabled", true))
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
		if (!palette_window_live(row->identity))
			return false;
	}
	else
	{
		topology = FScreenGetTopologySnapshot();
		if (!topology || topology->epoch != slot->topology ||
		    !palette_desktop_find(row))
			return false;
	}
	palette_row_id(id, slot->generation, row);
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
		static const char *verbs[] = {"Iconify off", "Raise", "Focus"};
		FvwmWindow *fw = palette_window_find(row->identity);

		if (!fw || step >= sizeof(verbs) / sizeof(verbs[0]))
			return false;
		snprintf(command, size, "WindowId 0x%lx %s",
			FW_W(fw), verbs[step]);
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
