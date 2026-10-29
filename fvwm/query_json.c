/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "query_json.h"
#include "execcontext.h"
#include "screen.h"
#include "menustyle.h"
#include "libs/defaults.h"
#include "libs/modifiers.h"
#include "libs/wcontext.h"
#include <inttypes.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* Conservative, process-local activity revision. All callers run on the core
 * thread. A wrap permanently disables observations, rather than reusing IDs. */
static uint64_t appearance_generation = 1;

void fvwm_query_appearance_changed(void)
{
	if (appearance_generation)
		appearance_generation++;
}

bool fvwm_query_appearance_token(
	char output[MODULE_QUERY_GENERATION_LEN + 1])
{
	return fvwm_query_token(appearance_generation, output);
}

bool fvwm_query_token(uint64_t revision,
	char output[MODULE_QUERY_GENERATION_LEN + 1])
{
	static struct timespec incarnation;
	static bool initialized;

	output[0] = '\0';
	if (!revision)
		return false;
	if (!initialized)
	{
		if (clock_gettime(CLOCK_MONOTONIC, &incarnation) != 0)
			return false;
		initialized = true;
	}
	/* Bind revisions to this core incarnation, including after reconnect.
	 * This identifies an observation; it is not an authentication token. */
	snprintf(output, MODULE_QUERY_GENERATION_LEN + 1,
		"%016" PRIx64 "%08" PRIx32 "%08" PRIx32 "%016" PRIx64,
		(uint64_t)incarnation.tv_sec, (uint32_t)incarnation.tv_nsec,
		(uint32_t)getpid(), revision);
	return true;
}

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
	    !decimal(offset, strcmp(parsed.resource, "colorset") == 0 ?
		INT_MAX : MODULE_QUERY_OFFSET_MAX, &parsed.offset) ||
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

static bool snapshot_generation(const char *generation,
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
	if (!snapshot_generation(generation, spec, error))
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
	if (!snapshot_generation(generation, spec, error))
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
	if (!snapshot_generation(generation, spec, error))
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

static bool add_pixel(cJSON *object, const char *name, Pixel pixel)
{
	char text[17];

	/* X pixels are colormap-local values, not RGB or resource IDs. */
	snprintf(text, sizeof(text), "%016" PRIx64, (uint64_t)pixel);
	return cJSON_AddStringToObject(object, name, text) != NULL;
}

static bool add_colorset(cJSON *list, const struct fvwm_query_colorset *entry)
{
	const colorset_t *cs = entry->colorset;
	cJSON *item = cJSON_CreateObject(), *colors, *opacity, *background;

	if (!item)
		return false;
	if (!cJSON_AddItemToArray(list, item))
	{
		cJSON_Delete(item);
		return false;
	}
	colors = cJSON_AddObjectToObject(item, "pixels");
	opacity = cJSON_AddObjectToObject(item, "opacity_percent");
	background = cJSON_AddObjectToObject(item, "background");
	return colors && opacity && background &&
		cJSON_AddNumberToObject(item, "id", entry->id) &&
		cJSON_AddStringToObject(item, "update_state",
			entry->update_state) &&
		add_pixel(colors, "foreground", cs->fg) &&
		add_pixel(colors, "background", cs->bg) &&
		add_pixel(colors, "hilight", cs->hilite) &&
		add_pixel(colors, "shadow", cs->shadow) &&
		add_pixel(colors, "foreground_shadow", cs->fgsh) &&
		add_pixel(colors, "tint", cs->tint) &&
		add_pixel(colors, "icon_tint", cs->icon_tint) &&
		cJSON_AddNumberToObject(opacity, "foreground",
			cs->fg_alpha_percent) &&
		cJSON_AddNumberToObject(opacity, "image",
			cs->image_alpha_percent) &&
		cJSON_AddNumberToObject(opacity, "icon",
			cs->icon_alpha_percent) &&
		cJSON_AddNumberToObject(item, "tint_percent",
			cs->tint_percent) &&
		cJSON_AddNumberToObject(item, "icon_tint_percent",
			cs->icon_tint_percent) &&
		cJSON_AddBoolToObject(item, "dither", cs->dither) &&
		cJSON_AddBoolToObject(item, "icon_dither",
			cs->do_dither_icon) &&
		cJSON_AddBoolToObject(background, "has_pixmap",
			cs->pixmap != None) &&
		cJSON_AddStringToObject(background, "transparency",
			cs->pixmap == ParentRelative ? "parent" :
			CSETS_IS_TRANSPARENT_ROOT(cs) ? "root" : "none") &&
		cJSON_AddNumberToObject(background, "type", cs->pixmap_type) &&
		cJSON_AddNumberToObject(background, "width", cs->width) &&
		cJSON_AddNumberToObject(background, "height", cs->height) &&
		cJSON_AddBoolToObject(item, "has_shape",
			cs->shape_mask != None);
}

cJSON *fvwm_query_colorsets(const struct fvwm_query_colorset *entries,
	size_t count, size_t total, const char *generation,
	const struct fvwm_query_spec *spec, const char **error)
{
	bool single = strcmp(spec->resource, "colorset") == 0;
	bool summary = strcmp(spec->resource, "color-summary") == 0;
	struct fvwm_query_spec check = *spec;
	cJSON *result, *list;

	*error = "invalid_request";
	if (spec->limit == 0 || spec->limit > MODULE_QUERY_PAGE_MAX ||
	    count > spec->limit || count > total || (count && !entries) ||
	    total > FVWM_COLORSET_LIMIT ||
	    (!single && spec->offset > MODULE_QUERY_OFFSET_MAX) ||
	    (single && (spec->offset > INT_MAX || count != 1)) ||
	    (summary && (spec->offset || count)) ||
	    (!single && !summary &&
		(spec->offset > total || count !=
			(total - spec->offset < spec->limit ?
			 total - spec->offset : spec->limit))))
		return NULL;
	/* Exact ID lookups are independent of an inventory cursor. */
	if (single)
		check.offset = 0;
	if (!snapshot_generation(generation, &check, error))
		return NULL;
	*error = "invalid_core_state";
	for (size_t i = 0; i < count; i++)
		if (entries[i].id < 0 || !entries[i].colorset ||
		    !entries[i].update_state ||
		    (strcmp(entries[i].update_state, "idle") != 0 &&
		     strcmp(entries[i].update_state, "pending") != 0 &&
		     strcmp(entries[i].update_state, "refused") != 0) ||
		    (single && (unsigned)entries[i].id != spec->offset))
			return NULL;
	if (!single && !summary && spec->offset + count < total &&
	    spec->offset + count > MODULE_QUERY_OFFSET_MAX)
	{
		*error = "query_too_large";
		return NULL;
	}
	result = observed_result("core-colorsets", error);
	if (!result)
		return NULL;
	if (!cJSON_AddStringToObject(result, "generation", generation) ||
	    !cJSON_AddStringToObject(result, "values", "published") ||
	    !cJSON_AddStringToObject(result, "units", "actual-pixels") ||
	    !cJSON_AddStringToObject(result, "pixel_encoding",
		"colormap-hex") ||
	    !cJSON_AddNumberToObject(result, "total", total) ||
	    !cJSON_AddNumberToObject(result, "record_limit",
		FVWM_COLORSET_LIMIT) ||
	    !cJSON_AddNumberToObject(result, "page_max",
		MODULE_QUERY_PAGE_MAX) ||
	    !cJSON_AddBoolToObject(result, "preview_supported", false))
		goto fail;
	if (summary)
	{
		*error = NULL;
		return result;
	}
	list = cJSON_AddArrayToObject(result, "colorsets");
	if (!list || !cJSON_AddBoolToObject(result, "targeted", single) ||
	    !(single ? cJSON_AddNullToObject(result, "offset") :
		cJSON_AddNumberToObject(result, "offset", spec->offset)) ||
	    !((!single && spec->offset + count < total) ?
		cJSON_AddNumberToObject(result, "next_offset",
			spec->offset + count) :
		cJSON_AddNullToObject(result, "next_offset")))
		goto fail;
	for (size_t i = 0; i < count; i++)
		if (!add_colorset(list, entries + i))
			goto fail;
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

cJSON *fvwm_query_appearance(const char *generation,
	const struct fvwm_query_spec *spec, const char **error)
{
	static const char *coverage[] = {"window-style-definitions",
		"decoration-definitions", "menu-style-policy",
		"default-font-command", "colorset-publication"};
	cJSON *result, *list;

	*error = "invalid_request";
	if (spec->offset != 0)
		return NULL;
	if (!snapshot_generation(generation, spec, error))
		return NULL;
	result = observed_result("core-appearance-configuration", error);
	if (!result)
		return NULL;
	list = cJSON_AddArrayToObject(result, "coverage");
	if (!list ||
	    !cJSON_AddStringToObject(result, "generation", generation) ||
	    !cJSON_AddBoolToObject(result, "window_effective_state", false) ||
	    !cJSON_AddStringToObject(result, "rendered_completion",
		"unknown") ||
	    !cJSON_AddBoolToObject(result, "conditional_application", false) ||
	    !cJSON_AddBoolToObject(result, "preview_supported", true))
		goto fail;
	for (size_t i = 0; i < sizeof(coverage) / sizeof(coverage[0]); i++)
	{
		cJSON *name = cJSON_CreateString(coverage[i]);

		if (!name)
			goto fail;
		if (!cJSON_AddItemToArray(list, name))
		{
			cJSON_Delete(name);
			goto fail;
		}
	}
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

static bool appearance_page_valid(const char *generation,
	const struct fvwm_query_spec *spec, const char **error)
{
	*error = "invalid_request";
	return spec->offset <= MODULE_QUERY_OFFSET_MAX && spec->limit > 0 &&
		spec->limit <= MODULE_QUERY_PAGE_MAX &&
		snapshot_generation(generation, spec, error);
}

static cJSON *appearance_page(const char *scope, const char *property,
	const char *generation, unsigned int offset, const char **error)
{
	cJSON *result = observed_result(scope, error);

	if (result &&
	    cJSON_AddStringToObject(result, "generation", generation) &&
	    cJSON_AddStringToObject(result, "units", "configuration-pixels") &&
	    cJSON_AddNumberToObject(result, "offset", offset) &&
	    cJSON_AddArrayToObject(result, property))
		return result;
	cJSON_Delete(result);
	return NULL;
}

static bool appearance_page_finish(cJSON *result, bool more,
	unsigned int next, const char **error)
{
	if (more && next > MODULE_QUERY_OFFSET_MAX)
	{
		*error = "query_too_large";
		return false;
	}
	if (!(more ? cJSON_AddNumberToObject(result, "next_offset", next) :
		cJSON_AddNullToObject(result, "next_offset")))
		return false;
	*error = NULL;
	return true;
}

static cJSON *appearance_entry(cJSON *list)
{
	cJSON *item = cJSON_CreateObject();

	if (item && !cJSON_AddItemToArray(list, item))
	{
		cJSON_Delete(item);
		return NULL;
	}
	return item;
}

cJSON *fvwm_query_menu_styles(const MenuStyle *styles,
	const char *generation, const struct fvwm_query_spec *spec,
	const char **error)
{
	const MenuStyle *style = styles;
	unsigned int offset = 0, count = 0;
	cJSON *result, *list;

	if (!appearance_page_valid(generation, spec, error))
		return NULL;
	for (; style && offset < spec->offset; offset++)
		style = ST_NEXT_STYLE(style);
	*error = "invalid_offset";
	if (offset != spec->offset)
		return NULL;
	result = appearance_page("core-menu-style-policy", "menu_styles",
		generation, offset, error);
	if (!result)
		return NULL;
	list = cJSON_GetObjectItemCaseSensitive(result, "menu_styles");
	for (; style && count < spec->limit;
	     style = ST_NEXT_STYLE(style), count++)
	{
		cJSON *item, *colorsets;

		if (!bounded_text(ST_NAME(style), MODULE_QUERY_NAME_MAX) ||
		    (ST_ITEM_FORMAT(style) &&
		     !bounded_text(ST_ITEM_FORMAT(style),
			MODULE_QUERY_STRING_MAX)))
		{
			*error = "query_too_large";
			goto fail;
		}
		item = appearance_entry(list);
		if (!item)
			goto fail;
		colorsets = cJSON_AddObjectToObject(item, "colorsets");
		if (!colorsets ||
		    !cJSON_AddStringToObject(item, "name", ST_NAME(style)) ||
		    !cJSON_AddBoolToObject(item, "default",
			offset + count == 0) ||
		    !cJSON_AddBoolToObject(item, "animation",
			ST_IS_ANIMATED(style)) ||
		    !cJSON_AddNumberToObject(item, "opacity_percent",
			ST_TRANSLUCENT_PERCENT(style)) ||
		    !cJSON_AddNumberToObject(item, "border_width",
			ST_BORDER_WIDTH(style)) ||
		    !cJSON_AddNumberToObject(item, "popup_delay_10ms",
			ST_POPUP_DELAY(style)) ||
		    !cJSON_AddNumberToObject(item, "popdown_delay_10ms",
			ST_POPDOWN_DELAY(style)) ||
		    !cJSON_AddBoolToObject(item, "automatic_hotkeys",
			ST_USE_AUTOMATIC_HOTKEYS(style)) ||
		    !cJSON_AddBoolToObject(item, "submenus_left",
			ST_USE_LEFT_SUBMENUS(style)) ||
		    !(ST_ITEM_FORMAT(style) ? cJSON_AddStringToObject(item,
			"item_format", ST_ITEM_FORMAT(style)) :
			cJSON_AddNullToObject(item, "item_format")) ||
		    !cJSON_AddNumberToObject(colorsets, "menu",
			ST_CSET_MENU(style)) ||
		    !cJSON_AddNumberToObject(colorsets, "active",
			ST_CSET_ACTIVE(style)) ||
		    !cJSON_AddNumberToObject(colorsets, "greyed",
			ST_CSET_GREYED(style)) ||
		    !cJSON_AddNumberToObject(colorsets, "title",
			ST_CSET_TITLE(style)))
			goto fail;
	}
	if (!appearance_page_finish(result, style != NULL,
		offset + count, error))
		goto fail;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

cJSON *fvwm_query_decorations(const FvwmDecor *decorations,
	const char *generation, const struct fvwm_query_spec *spec,
	const char **error)
{
	const FvwmDecor *decor = decorations;
	unsigned int offset = 0, count = 0;
	cJSON *result, *list;

	if (!appearance_page_valid(generation, spec, error))
		return NULL;
	for (; decor && offset < spec->offset; offset++)
		decor = decor->next;
	*error = "invalid_offset";
	if (offset != spec->offset)
		return NULL;
	result = appearance_page("core-decoration-definitions", "decorations",
		generation, offset, error);
	if (!result)
		return NULL;
	list = cJSON_GetObjectItemCaseSensitive(result, "decorations");
	for (; decor && count < spec->limit; decor = decor->next, count++)
	{
		cJSON *item;
		const char *justification =
			TB_JUSTIFICATION(decor->titlebar) == JUST_CENTER ?
			"center" :
			TB_JUSTIFICATION(decor->titlebar) == JUST_LEFT ?
			"left" :
			TB_JUSTIFICATION(decor->titlebar) == JUST_RIGHT ?
			"right" : "unknown";

		if (decor->tag &&
		    !bounded_text(decor->tag, MODULE_QUERY_NAME_MAX))
		{
			*error = "query_too_large";
			goto fail;
		}
		item = appearance_entry(list);
		if (!item ||
		    !(decor->tag ?
			cJSON_AddStringToObject(item, "name", decor->tag) :
			cJSON_AddNullToObject(item, "name")) ||
		    !cJSON_AddBoolToObject(item, "default",
			offset + count == 0) ||
		    !cJSON_AddNumberToObject(item, "title_height",
			decor->title_height) ||
		    !cJSON_AddNumberToObject(item, "minimum_title_height",
			decor->min_title_height) ||
		    !cJSON_AddStringToObject(item, "title_justification",
			justification) ||
		    !cJSON_AddNumberToObject(item, "button_slots",
			NUMBER_OF_TITLE_BUTTONS))
			goto fail;
	}
	if (!appearance_page_finish(result, decor != NULL,
		offset + count, error))
		goto fail;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}
