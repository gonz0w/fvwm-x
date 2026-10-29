/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/query_json.h"
#include "fvwm/execcontext.h"
#include "fvwm/screen.h"
#include "fvwm/menustyle.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static size_t attempts, fail_at, live;

static void *checked_malloc(size_t size)
{
	void *p;

	if (attempts++ == fail_at)
		return NULL;
	p = malloc(size);
	if (p)
		live++;
	return p;
}

static void checked_free(void *p)
{
	if (p)
	{
		assert(live > 0);
		live--;
	}
	free(p);
}

static void test_internal_grammar(void)
{
	struct fvwm_query_spec spec;
	const char *bad[] = {NULL, "",
		"a 0000000000000001 topology 0 8 - extra",
		"a;Exec 0000000000000001 topology 0 8 -",
		"a 0000000000000001 bindings 4294967296 8 -",
		"a 0000000000000001 bindings -1 8 -",
		"a 0000000000000001 bindings 0 0 -",
		"a 0000000000000001 bindings 0 17 -",
		"a 0000000000000001 bindings 0 8 wrong"};

	assert(fvwm_query_parse("a-b 0000000000000001 bindings 4096 16 "
		"ffffffffffffffffffffffffffffffffffffffffffffffff", &spec));
	assert(spec.offset == 4096 && spec.limit == 16);
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		assert(!fvwm_query_parse(bad[i], &spec));
		assert(spec.cookie[0] == '\0');
	}
}

static const char token[] =
	"000000000000000000000000000000000000000000000001";

static void test_appearance(void)
{
	MenuStyle menus[2] = {{.name = "*"}, {.name = "literal $[desk.n]"}};
	FvwmDecor decors[2] = {{.tag = "Default"}, {.tag = "custom"}};
	struct fvwm_query_spec spec = {.limit = 1, .generation = "-"};
	const char *error;
	cJSON *result, *item;
	char first[MODULE_QUERY_GENERATION_LEN + 1], second[sizeof(first)];

	menus[0].next_style = menus + 1;
	menus[1].feel.flags.is_animated = 1;
	menus[1].look.TranslucentPercent = 73;
	menus[1].feel.item_format = "%l literal $[desk.n]";
	decors[0].next = decors + 1;
	decors[1].title_height = 23;
	TB_JUSTIFICATION(decors[1].titlebar) = JUST_LEFT;
	assert(fvwm_query_appearance_token(first));
	assert(fvwm_query_appearance_token(second) && strcmp(first,
		second) == 0);
	fvwm_query_appearance_changed();
	assert(fvwm_query_appearance_token(second) && strcmp(first,
		second) != 0);
	result = fvwm_query_appearance(token, &spec, &error);
	assert(result && !error);
	assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(result,
		"conditional_application")));
	assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(result,
		"preview_supported")));
	cJSON_Delete(result);
	for (unsigned int kind = 0; kind < 2; kind++)
	{
		const char *property = kind ? "decorations" : "menu_styles";

		spec.offset = 0;
		strcpy(spec.generation, "-");
		result = kind ? fvwm_query_decorations(decors, token, &spec,
			&error) : fvwm_query_menu_styles(menus, token, &spec,
				&error);
		assert(result && !error);
		assert(cJSON_GetObjectItemCaseSensitive(result,
			"next_offset")->valueint == 1);
		item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(
			result,
			property), 0);
		assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item,
			"default")));
		cJSON_Delete(result);
		spec.offset = 1;
		assert(!(kind ? fvwm_query_decorations(decors, token, &spec,
			&error) : fvwm_query_menu_styles(menus, token, &spec,
				&error)));
		assert(strcmp(error, "stale_generation") == 0);
		strcpy(spec.generation, token);
		result = kind ? fvwm_query_decorations(decors, token, &spec,
			&error) : fvwm_query_menu_styles(menus, token, &spec,
				&error);
		assert(result && !error);
		assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(result,
			"next_offset")));
		item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(
			result,
			property), 0);
		assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(item,
			"default")));
		if (kind)
			assert(cJSON_GetObjectItemCaseSensitive(item,
				"title_height")->valueint == 23);
		else
			assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(
				item,
				"animation")));
		cJSON_Delete(result);
		spec.offset = 3;
		assert(!(kind ? fvwm_query_decorations(decors, token, &spec,
			&error) : fvwm_query_menu_styles(menus, token, &spec,
				&error)));
		assert(strcmp(error, "invalid_offset") == 0);
	}
	menus[0].next_style = menus;
	decors[0].next = decors;
	spec.offset = MODULE_QUERY_OFFSET_MAX;
	assert(!fvwm_query_menu_styles(menus, token, &spec, &error));
	assert(strcmp(error, "query_too_large") == 0);
	assert(!fvwm_query_decorations(decors, token, &spec, &error));
	assert(strcmp(error, "query_too_large") == 0);
	menus[0].next_style = NULL;
	decors[0].next = NULL;
	spec.offset = 0;
	strcpy(spec.generation, "-");
	char oversized[MODULE_QUERY_STRING_MAX + 2];

	memset(oversized, 'a', sizeof(oversized) - 1);
	oversized[sizeof(oversized) - 1] = '\0';
	menus[0].feel.item_format = oversized;
	assert(!fvwm_query_menu_styles(menus, token, &spec, &error));
	assert(strcmp(error, "query_too_large") == 0);
	menus[0].feel.item_format = NULL;
	decors[0].tag = oversized;
	assert(!fvwm_query_decorations(decors, token, &spec, &error));
	assert(strcmp(error, "query_too_large") == 0);
	decors[0].tag = NULL;
	cJSON_Hooks hooks = {.malloc_fn = checked_malloc,
		.free_fn = checked_free};

	for (unsigned int kind = 0; kind < 3; kind++)
	{
		bool completed = false;

		for (fail_at = 0; fail_at < 1024; fail_at++)
		{
			attempts = live = 0;
			cJSON_InitHooks(&hooks);
			result = kind == 0 ? fvwm_query_appearance(token, &spec,
				&error) : kind == 1 ? fvwm_query_menu_styles(
					menus,
				token, &spec, &error) : fvwm_query_decorations(
					decors,
				token, &spec, &error);
			completed = result != NULL;
			assert(completed ? error == NULL : error != NULL);
			cJSON_Delete(result);
			assert(live == 0);
			cJSON_InitHooks(NULL);
			if (completed)
				break;
		}
		assert(completed);
	}
}

static void test_colorsets(void)
{
	colorset_t cs = {.fg = (Pixel)~0UL, .bg = 3,
		.fg_alpha_percent = 80, .image_alpha_percent = 70,
		.icon_alpha_percent = 60, .pixmap = ParentRelative};
	struct fvwm_query_colorset entry = {2147483647, &cs, "pending"};
	struct fvwm_query_spec spec = {.limit = 1, .generation = "-",
		.resource = "colorsets"};
	const char *error;
	cJSON *result, *item, *background, *pixels;
	char first[MODULE_QUERY_GENERATION_LEN + 1], second[sizeof(first)];

	assert(!fvwm_query_token(0, first) && !first[0]);
	assert(fvwm_query_token(1, first));
	assert(fvwm_query_token(1, second) && strcmp(first, second) == 0);
	assert(fvwm_query_token(2, second) && strcmp(first, second) != 0);
	result = fvwm_query_colorsets(&entry, 1, 2, token, &spec, &error);
	assert(result && !error);
	assert(cJSON_GetObjectItemCaseSensitive(result,
		"next_offset")->valueint == 1);
	item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(result,
		"colorsets"), 0);
	assert(cJSON_GetObjectItemCaseSensitive(item,
		"id")->valueint == 2147483647);
	background = cJSON_GetObjectItemCaseSensitive(item, "background");
	pixels = cJSON_GetObjectItemCaseSensitive(item, "pixels");
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(background,
		"transparency")->valuestring, "parent") == 0);
	assert(strlen(cJSON_GetObjectItemCaseSensitive(pixels,
		"foreground")->valuestring) == 16);
	cJSON_Delete(result);
	spec.offset = 1;
	assert(!fvwm_query_colorsets(&entry, 1, 2, token, &spec, &error));
	assert(strcmp(error, "stale_generation") == 0);
	strcpy(spec.generation, token);
	result = fvwm_query_colorsets(&entry, 1, 2, token, &spec, &error);
	assert(result && !error);
	assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(result,
		"next_offset")));
	cJSON_Delete(result);
	strcpy(spec.resource, "colorset");
	spec.offset = 2147483647;
	strcpy(spec.generation, "-");
	result = fvwm_query_colorsets(&entry, 1, 2, token, &spec, &error);
	assert(result && !error);
	assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(result,
		"targeted")));
	cJSON_Delete(result);
	assert(fvwm_query_parse("a 0000000000000001 colorset 2147483647 1 -",
		&spec));
	assert(!fvwm_query_parse("a 0000000000000001 colorset 2147483648 1 -",
		&spec));
	assert(!fvwm_query_parse("a 0000000000000001 colorsets 2147483647 1 -",
		&spec));
	spec = (struct fvwm_query_spec){.limit = 1, .generation = "-",
		.resource = "color-summary"};
	result = fvwm_query_colorsets(NULL, 0, 2, token, &spec, &error);
	assert(result && !error);
	assert(!cJSON_GetObjectItemCaseSensitive(result, "colorsets"));
	cJSON_Delete(result);
	strcpy(spec.resource, "colorsets");
	assert(!fvwm_query_colorsets(NULL, 1, 2, token, &spec, &error));
	entry.update_state = "invalid";
	assert(!fvwm_query_colorsets(&entry, 1, 2, token, &spec, &error));
	entry.update_state = "refused";
	cJSON_Hooks hooks = {.malloc_fn = checked_malloc,
		.free_fn = checked_free};
	bool completed = false;

	for (fail_at = 0; fail_at < 1024; fail_at++)
	{
		attempts = live = 0;
		cJSON_InitHooks(&hooks);
		result = fvwm_query_colorsets(&entry, 1, 1, token, &spec,
			&error);
		completed = result != NULL;
		assert(completed ? error == NULL : error != NULL);
		cJSON_Delete(result);
		assert(live == 0);
		cJSON_InitHooks(NULL);
		if (completed)
			break;
	}
	assert(completed);
}

static void test_binding_pages(void)
{
	Binding bindings[3] = {{.type = BIND_KEYPRESS, .key_name = "F1",
		.Action = "Echo literal \"$[desk.n]\"", .Context = 1},
		{.type = BIND_BUTTONRELEASE, .Button_Key = 3,
		.Action = "Nop", .Modifier = 4, .windowName = "editor*"},
		{.type = BIND_PKEYPRESS, .key_name = "F2", .Action = "Nop"}};
	struct fvwm_query_spec spec = {.limit = 2, .generation = "-"};
	const char *error;
	cJSON *result, *list;

	bindings[0].NextBinding = bindings + 1;
	bindings[1].NextBinding = bindings + 2;
	result = fvwm_query_bindings(bindings, token, &spec, &error);
	assert(result && error == NULL);
	list = cJSON_GetObjectItemCaseSensitive(result, "bindings");
	assert(cJSON_GetArraySize(list) == 2);
	assert(cJSON_GetObjectItemCaseSensitive(result,
		"next_offset")->valueint == 2);
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(
		cJSON_GetArrayItem(list, 0),
		"action")->valuestring, bindings[0].Action) == 0);
	cJSON_Delete(result);
	spec.offset = 2;
	strcpy(spec.generation, token);
	result = fvwm_query_bindings(bindings, token, &spec, &error);
	assert(result && !error);
	assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(result,
		"next_offset")));
	cJSON_Delete(result);
	assert(!fvwm_query_bindings(bindings,
		"000000000000000000000000000000000000000000000002",
		&spec, &error));
	assert(strcmp(error, "stale_generation") == 0);
	assert(!fvwm_query_bindings(bindings, NULL, &spec, &error));
	assert(strcmp(error, "generation_unavailable") == 0);
	spec.offset = 4;
	assert(!fvwm_query_bindings(bindings, token, &spec, &error));
	assert(strcmp(error, "invalid_offset") == 0);
	spec.offset = 0;
	char oversized[MODULE_QUERY_STRING_MAX + 2];

	memset(oversized, 'a', sizeof(oversized) - 1);
	oversized[sizeof(oversized) - 1] = '\0';
	bindings[0].Action = oversized;
	assert(!fvwm_query_bindings(bindings, token, &spec, &error));
	assert(strcmp(error, "query_too_large") == 0);
}

static void test_binding_domains(void)
{
	struct fvwm_query_spec spec = {.limit = 8, .generation = "-",
		.resource = "menu-bindings"};
	Binding binding = {.type = BIND_KEYPRESS, .Action = "SelectItem"};
	const char *error;
	cJSON *result = fvwm_query_bindings(&binding, token, &spec, &error);

	assert(result && !error);
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(result,
		"scope")->valuestring, "menu-bindings") == 0);
	assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(
		cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(result,
			"bindings"), 0), "key")));
	cJSON_Delete(result);
	strcpy(spec.resource, "menu-defaults");
	result = fvwm_query_bindings(NULL, token, &spec, &error);
	assert(result && !error);
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(result,
		"scope")->valuestring, "menu-fallback-bindings") == 0);
	assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(result,
		"bindings")) == 0);
	cJSON_Delete(result);
	result = fvwm_query_placement(token, &spec, 4, 5, &error);
	assert(result && !error);
	assert(cJSON_GetObjectItemCaseSensitive(result,
		"drag_finish_mask")->valueint == 4);
	assert(cJSON_GetObjectItemCaseSensitive(result,
		"interactive_finish_mask")->valueint == 5);
	assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(result,
		"key_bindings")));
	cJSON_Delete(result);
	result = fvwm_query_binding_policy(token, &spec, 2, &error);
	assert(result && !error);
	assert(cJSON_GetObjectItemCaseSensitive(result,
		"ignored_modifier_mask")->valueint == 2);
	assert(cJSON_GetObjectItemCaseSensitive(
		cJSON_GetObjectItemCaseSensitive(result, "contexts"),
		"root")->valueint == 8);
	assert(cJSON_GetObjectItemCaseSensitive(
		cJSON_GetObjectItemCaseSensitive(result, "modifiers"),
		"control")->valueint == 4);
	cJSON_Delete(result);
	spec.offset = 1;
	assert(!fvwm_query_placement(token, &spec, 0, 0, &error));
	assert(strcmp(error, "invalid_request") == 0);
	assert(!fvwm_query_binding_policy(token, &spec, 0, &error));
	assert(strcmp(error, "invalid_request") == 0);
	spec.offset = 0;
	strcpy(spec.generation,
		"000000000000000000000000000000000000000000000002");
	assert(!fvwm_query_placement(token, &spec, 0, 0, &error));
	assert(strcmp(error, "stale_generation") == 0);
	assert(!fvwm_query_binding_policy(token, &spec, 0, &error));
	assert(strcmp(error, "stale_generation") == 0);
}

static void test_monitors_and_failure_cleanup(void)
{
	monitor_topology_monitor fact = {.name_atom = 1, .name = "test",
		.width = 800, .height = 600, .primary = True};
	monitor_topology_snapshot topology = {.monitors = &fact,
		.monitor_count = 1, .epoch = UINT64_MAX};
	struct screen_info screen = {.name = "test", .w = 800, .h = 600};
	DesktopsInfo desk = {.ewmh_working_area = {0, 30, 800, 570},
		.ewmh_dyn_working_area = {0, 40, 800, 560}};
	struct monitor monitor = {.si = &screen, .Desktops = &desk,
		.dx = 3, .dy = 2,
		.effective_scale = MONITOR_SCALE_RESULT_SAFE_INIT};
	struct monitors monitors;
	Binding binding = {.type = BIND_KEYPRESS, .key_name = "F1",
		.Action = "Nop"};
	struct fvwm_query_spec spec = {.limit = 8, .generation = "-"};
	const char *error;
	cJSON *result;

	RB_INIT(&monitors);
	assert(RB_INSERT(monitors, &monitors, &monitor) == NULL);
	result = fvwm_query_topology(&topology, &error);
	assert(result && !error);
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(
		cJSON_GetObjectItemCaseSensitive(result, "topology"),
		"epoch")->valuestring, "ffffffffffffffff") == 0);
	cJSON_Delete(result);
	result = fvwm_query_workspaces(&monitors, 800, 600, "global", &error);
	assert(result && !error);
	cJSON *item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(
		result,
		"monitors"), 0);
	assert(cJSON_GetObjectItemCaseSensitive(
		cJSON_GetObjectItemCaseSensitive(
		item, "dynamic_work_area"), "y")->valueint == 40);
	cJSON_Delete(result);
	cJSON_Hooks hooks = {.malloc_fn = checked_malloc,
		.free_fn = checked_free};

	for (unsigned int kind = 0; kind < 5; kind++)
	{
		bool completed = false;

		for (fail_at = 0; fail_at < 1024; fail_at++)
		{
			attempts = live = 0;
			cJSON_InitHooks(&hooks);
			switch (kind)
			{
			case 0:
				result = fvwm_query_topology(&topology, &error);
				break;
			case 1:
				result = fvwm_query_workspaces(&monitors,
					800, 600, "global", &error);
				break;
			case 2:
				result = fvwm_query_bindings(&binding, token,
					&spec, &error);
				break;
			case 3:
				result = fvwm_query_placement(token, &spec,
					4, 5, &error);
				break;
			default:
				result = fvwm_query_binding_policy(token, &spec,
					2, &error);
				break;
			}
			completed = result != NULL;
			assert(completed ? error == NULL : error != NULL);
			cJSON_Delete(result);
			assert(live == 0);
			cJSON_InitHooks(NULL);
			if (completed)
				break;
		}
		assert(completed);
	}
	topology.monitor_count = MODULE_QUERY_MONITORS_MAX + 1;
	assert(!fvwm_query_topology(&topology, &error));
	assert(strcmp(error, "query_too_large") == 0);
	monitor.Desktops = NULL;
	assert(!fvwm_query_workspaces(&monitors, 800, 600, "global", &error));
	assert(strcmp(error, "unavailable") == 0);
}

int main(void)
{
	test_internal_grammar();
	test_appearance();
	test_colorsets();
	test_binding_pages();
	test_binding_domains();
	test_monitors_and_failure_cleanup();
	return 0;
}
