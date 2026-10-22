/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "query_json.h"
#include "libs/defaults.h"
#include "libs/modifiers.h"
#include "libs/wcontext.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool bounded_text(const char *text, size_t limit)
{
	return text && strnlen(text, limit + 1) <= limit;
}

static bool hex_token(const char *text, size_t length)
{
	return strlen(text) == length &&
		strspn(text, "0123456789abcdef") == length;
}

static bool decimal(const char *text, unsigned int maximum,
	unsigned int *value)
{
	unsigned int n = 0;

	if (!*text)
		return false;
	for (; *text; text++)
	{
		if (*text < '0' || *text > '9' ||
		    n > maximum / 10 ||
		    (n == maximum / 10 && (unsigned int)(*text - '0') >
			maximum % 10))
			return false;
		n = n * 10 + (unsigned int)(*text - '0');
	}
	*value = n;
	return true;
}

bool fvwm_query_parse(const char *text, struct fvwm_query_spec *spec)
{
	struct fvwm_query_spec parsed = {0};
	char offset[11], limit[11], extra;

	memset(spec, 0, sizeof(*spec));
	if (!bounded_text(text, 1000) || sscanf(text,
	    "%64s %16s %15s %10s %10s %48s %c", parsed.cookie,
	    parsed.serial, parsed.resource, offset, limit,
	    parsed.generation, &extra) != 6 ||
	    strspn(parsed.cookie, "0123456789abcdef-") !=
		strlen(parsed.cookie) || !hex_token(parsed.serial, 16) ||
	    !decimal(offset, MODULE_QUERY_OFFSET_MAX, &parsed.offset) ||
	    !decimal(limit, MODULE_QUERY_PAGE_MAX, &parsed.limit) ||
	    parsed.limit == 0 ||
	    (strcmp(parsed.generation, "-") != 0 &&
		!hex_token(parsed.generation, MODULE_QUERY_GENERATION_LEN)))
		return false;
	*spec = parsed;
	return true;
}

static cJSON *observed_result(const char *scope, const char **error)
{
	cJSON *result = cJSON_CreateObject();

	*error = "allocation_failed";
	if (result && cJSON_AddStringToObject(result, "state", "observed") &&
	    cJSON_AddStringToObject(result, "scope", scope))
		return result;
	cJSON_Delete(result);
	return NULL;
}

cJSON *fvwm_query_topology(const monitor_topology_snapshot *snapshot,
	const char **error)
{
	cJSON *result = NULL, *topology, *epoch;
	char generation[17];

	*error = "unavailable";
	if (!snapshot || !snapshot->monitors || snapshot->epoch == 0 ||
	    snapshot->monitor_count <= 0)
		return NULL;
	*error = "query_too_large";
	if (snapshot->monitor_count > MODULE_QUERY_MONITORS_MAX)
		return NULL;
	for (int i = 0; i < snapshot->monitor_count; i++)
	{
		const monitor_topology_monitor *m = snapshot->monitors + i;

		if (!bounded_text(m->name, MODULE_QUERY_NAME_MAX) ||
		    m->output_count > MODULE_QUERY_OUTPUTS_MAX)
			return NULL;
	}
	result = observed_result("committed-topology", error);
	if (!result)
		return NULL;
	if (!cJSON_AddStringToObject(result, "units", "actual-pixels") ||
	    !cJSON_AddBoolToObject(result, "root_fallback",
		snapshot->root_fallback) ||
	    !monitor_topology_json_add(result, "topology", snapshot))
		goto fail;
	/* Keep the exact epoch usable by JSON consumers with double numbers. */
	snprintf(generation, sizeof(generation), "%016" PRIx64,
		snapshot->epoch);
	topology = cJSON_GetObjectItemCaseSensitive(result, "topology");
	epoch = cJSON_CreateString(generation);
	if (!epoch)
		goto fail;
	if (!cJSON_ReplaceItemInObjectCaseSensitive(topology, "epoch", epoch))
	{
		cJSON_Delete(epoch);
		goto fail;
	}
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

static bool add_rectangle(cJSON *object, const char *name, rectangle area)
{
	/* Before the first geometry update the cache is all zero. It cannot be
	 * distinguished from a fully reserved origin-sized area; report unknown
	 * instead of certifying an empty area or scanning windows. */
	if (area.x == 0 && area.y == 0 && area.width == 0 && area.height == 0)
		return cJSON_AddNullToObject(object, name) != NULL;
	cJSON *r = cJSON_AddObjectToObject(object, name);

	return r && cJSON_AddNumberToObject(r, "x", area.x) &&
		cJSON_AddNumberToObject(r, "y", area.y) &&
		cJSON_AddNumberToObject(r, "width", area.width) &&
		cJSON_AddNumberToObject(r, "height", area.height);
}

cJSON *fvwm_query_workspaces(struct monitors *monitors, int width, int height,
	const char *mode, const char **error)
{
	cJSON *result = observed_result("core-monitors", error), *list;
	struct monitor *m;
	unsigned int count = 0;

	if (!result)
		return NULL;
	list = cJSON_AddArrayToObject(result, "monitors");
	if (!list ||
	    !cJSON_AddStringToObject(result, "units", "actual-pixels") ||
	    !cJSON_AddStringToObject(result, "desktop_mode", mode) ||
	    !cJSON_AddStringToObject(result, "work_area_freshness", "cached"))
		goto fail;
	RB_FOREACH(m, monitors, monitors)
	{
		cJSON *item, *viewport, *grid;

		if (++count > MODULE_QUERY_MONITORS_MAX || !m->si ||
		    !bounded_text(m->si->name, MODULE_QUERY_NAME_MAX))
		{
			*error = "query_too_large";
			goto fail;
		}
		if (!m->Desktops)
		{
			*error = "unavailable";
			goto fail;
		}
		item = cJSON_CreateObject();
		if (!item)
			goto fail;
		if (!cJSON_AddItemToArray(list, item))
		{
			cJSON_Delete(item);
			goto fail;
		}
		viewport = cJSON_AddObjectToObject(item, "viewport");
		grid = cJSON_AddObjectToObject(item, "pages");
		if (!viewport || !grid ||
		    !cJSON_AddStringToObject(item, "name", m->si->name) ||
		    !cJSON_AddNumberToObject(item, "desk",
			m->virtual_scr.CurrentDesk) ||
		    !cJSON_AddNumberToObject(item, "scale_quarters",
			m->effective_scale.factor.quarters) ||
		    !cJSON_AddNumberToObject(viewport, "x",
			m->virtual_scr.Vx) ||
		    !cJSON_AddNumberToObject(viewport, "y",
			m->virtual_scr.Vy) ||
		    !cJSON_AddNumberToObject(viewport, "width", width) ||
		    !cJSON_AddNumberToObject(viewport, "height", height) ||
		    !cJSON_AddNumberToObject(grid, "width", m->dx) ||
		    !cJSON_AddNumberToObject(grid, "height", m->dy) ||
		    !add_rectangle(item, "work_area",
			m->Desktops->ewmh_working_area) ||
		    !add_rectangle(item, "dynamic_work_area",
			m->Desktops->ewmh_dyn_working_area))
			goto fail;
	}
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

static bool binding_generation(const char *generation,
	const struct fvwm_query_spec *spec, const char **error)
{
	*error = "generation_unavailable";
	if (!generation ||
	    !hex_token(generation, MODULE_QUERY_GENERATION_LEN))
		return false;
	*error = "stale_generation";
	if ((spec->offset != 0 && strcmp(spec->generation, "-") == 0) ||
	    (strcmp(spec->generation, "-") != 0 &&
		strcmp(spec->generation, generation) != 0))
		return false;
	return true;
}

cJSON *fvwm_query_bindings(const Binding *bindings, const char *generation,
	const struct fvwm_query_spec *spec, const char **error)
{
	cJSON *result, *list;
	unsigned int offset = 0, count = 0;
	const Binding *b = bindings;

	*error = "invalid_request";
	if (spec->offset > MODULE_QUERY_OFFSET_MAX || spec->limit == 0 ||
	    spec->limit > MODULE_QUERY_PAGE_MAX)
		return NULL;
	if (!binding_generation(generation, spec, error))
		return NULL;
	while (b && offset < spec->offset)
	{
		b = b->NextBinding;
		offset++;
	}
	*error = "invalid_offset";
	if (offset != spec->offset)
		return NULL;
	result = observed_result(
		strcmp(spec->resource, "menu-bindings") == 0 ? "menu-bindings" :
		strcmp(spec->resource, "menu-defaults") == 0 ?
		"menu-fallback-bindings" : "global-bindings", error);
	if (!result)
		return NULL;
	list = cJSON_AddArrayToObject(result, "bindings");
	if (!list ||
	    !cJSON_AddStringToObject(result, "generation", generation) ||
	    !cJSON_AddNumberToObject(result, "offset", offset))
		goto fail;
	for (; b && count < spec->limit; b = b->NextBinding, count++)
	{
		cJSON *item;
		const char *type;

		switch (b->type)
		{
		case BIND_KEYPRESS:
			type = "Key";
			break;
		case BIND_PKEYPRESS:
			type = "PointerKey";
			break;
		case BIND_BUTTONPRESS:
			type = "MousePress";
			break;
		case BIND_BUTTONRELEASE:
			type = "MouseRelease";
			break;
		default:
			*error = "invalid_core_state";
			goto fail;
		}
		if (!bounded_text(b->Action, MODULE_QUERY_STRING_MAX) ||
		    (b->key_name &&
			!bounded_text(b->key_name, MODULE_QUERY_NAME_MAX)) ||
		    (b->windowName &&
			!bounded_text(b->windowName, MODULE_QUERY_STRING_MAX)))
		{
			*error = "query_too_large";
			goto fail;
		}
		item = cJSON_CreateObject();
		if (!item)
			goto fail;
		if (!cJSON_AddItemToArray(list, item))
		{
			cJSON_Delete(item);
			goto fail;
		}
		if (!cJSON_AddStringToObject(item, "type", type) ||
		    !cJSON_AddNumberToObject(item, "code", b->Button_Key) ||
		    !(b->key_name ? cJSON_AddStringToObject(item, "key",
			b->key_name) : cJSON_AddNullToObject(item, "key")) ||
		    !cJSON_AddNumberToObject(item, "context_mask",
			b->Context) ||
		    !cJSON_AddNumberToObject(item, "modifier_mask",
			b->Modifier) ||
		    !cJSON_AddStringToObject(item, "action", b->Action) ||
		    !(b->windowName ? cJSON_AddStringToObject(item, "window",
			b->windowName) : cJSON_AddNullToObject(item, "window")))
			goto fail;
	}
	if (b && offset + count > MODULE_QUERY_OFFSET_MAX)
	{
		*error = "query_too_large";
		goto fail;
	}
	if (!(b ? cJSON_AddNumberToObject(result, "next_offset",
		offset + count) :
		cJSON_AddNullToObject(result, "next_offset")))
		goto fail;
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

cJSON *fvwm_query_placement(const char *generation,
	const struct fvwm_query_spec *spec, unsigned int drag,
	unsigned int interactive, const char **error)
{
	cJSON *result;

	*error = "invalid_request";
	if (spec->offset != 0)
		return NULL;
	if (!binding_generation(generation, spec, error))
		return NULL;
	result = observed_result("placement-bindings", error);
	if (!result)
		return NULL;
	/* Placement stores normalized finish masks, not command strings.
	 * Bit zero describes button 1, including the higher X mouse buttons. */
	if (!cJSON_AddStringToObject(result, "generation", generation) ||
	    !cJSON_AddNumberToObject(result, "button_count",
		NUMBER_OF_EXTENDED_MOUSE_BUTTONS) ||
	    !cJSON_AddNumberToObject(result, "drag_finish_mask", drag) ||
	    !cJSON_AddNumberToObject(result, "interactive_finish_mask",
		interactive) ||
	    !cJSON_AddBoolToObject(result, "key_bindings", false) ||
	    !cJSON_AddBoolToObject(result, "modifier_bindings", false))
	{
		cJSON_Delete(result);
		return NULL;
	}
	*error = NULL;
	return result;
}

cJSON *fvwm_query_binding_policy(const char *generation,
	const struct fvwm_query_spec *spec, unsigned int ignored_modifiers,
	const char **error)
{
	static const struct {
		const char *name;
		unsigned int mask;
	} contexts[] = {
		{"window", C_WINDOW}, {"title", C_TITLE}, {"icon", C_ICON},
		{"root", C_ROOT}, {"menu", C_MENU},
		{"placement", C_PLACEMENT}, {"left_1", C_L1},
		{"right_1", C_R1}, {"left_2", C_L2}, {"right_2", C_R2},
		{"left_3", C_L3}, {"right_3", C_R3}, {"left_4", C_L4},
		{"right_4", C_R4}, {"left_5", C_L5}, {"right_5", C_R5},
		{"unmanaged", C_UNMANAGED}, {"ewmh_desktop", C_EWMH_DESKTOP},
		{"top_left", C_F_TOPLEFT}, {"top_right", C_F_TOPRIGHT},
		{"bottom_left", C_F_BOTTOMLEFT},
		{"bottom_right", C_F_BOTTOMRIGHT},
		{"left_side", C_SB_LEFT}, {"right_side", C_SB_RIGHT},
		{"top_side", C_SB_TOP}, {"bottom_side", C_SB_BOTTOM},
		{"menu_item", C_MENU_ITEM}, {"frame", C_FRAME},
		{"sidebar", C_SIDEBAR}, {"all", C_ALL}
	}, modifiers[] = {
		{"shift", ShiftMask}, {"lock", LockMask},
		{"control", ControlMask}, {"mod1", Mod1Mask},
		{"mod2", Mod2Mask}, {"mod3", Mod3Mask},
		{"mod4", Mod4Mask}, {"mod5", Mod5Mask},
		{"any", AnyModifier}
	};
	cJSON *result, *context_map, *modifier_map;

	*error = "invalid_request";
	if (spec->offset != 0)
		return NULL;
	if (!binding_generation(generation, spec, error))
		return NULL;
	result = observed_result("binding-policy", error);
	if (!result)
		return NULL;
	context_map = cJSON_AddObjectToObject(result, "contexts");
	modifier_map = cJSON_AddObjectToObject(result, "modifiers");
	if (!context_map || !modifier_map ||
	    !cJSON_AddStringToObject(result, "generation", generation) ||
	    !cJSON_AddNumberToObject(result, "ignored_modifier_mask",
		ignored_modifiers) ||
	    !cJSON_AddNumberToObject(result, "xkb_ignored_modifier_mask",
		DEFAULT_XKB_MODS_IGNORED) ||
	    !cJSON_AddNumberToObject(result, "used_modifier_mask",
		(unsigned int)ALL_MODIFIERS & ~ignored_modifiers &
		~DEFAULT_XKB_MODS_IGNORED))
		goto fail;
	for (size_t i = 0; i < sizeof(contexts) / sizeof(contexts[0]); i++)
		if (!cJSON_AddNumberToObject(context_map, contexts[i].name,
			contexts[i].mask))
			goto fail;
	for (size_t i = 0; i < sizeof(modifiers) / sizeof(modifiers[0]); i++)
		if (!cJSON_AddNumberToObject(modifier_map, modifiers[i].name,
			modifiers[i].mask))
			goto fail;
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}
