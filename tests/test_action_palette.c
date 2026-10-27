/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "fvwm/action_palette.h"
#include "fvwm/query_json.h"
#include "fvwm/execcontext.h"
#include "fvwm/screen.h"
#include "fvwm/module_records.h"
#include "libs/monitor_types.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exercise the production snapshot and guard with owned in-memory core
 * records. No X connection or command execution is needed for these checks. */
ScreenInfo Scr;
struct monitors monitor_q = RB_INITIALIZER(&monitor_q);
static monitor_topology_snapshot topology = {.epoch = 7};
static struct fmodule owners[5];
static struct screen_info screen_info = {
	.name = "literal $[desk.n]; Exec bad", .rr_output = 8, .name_atom = 12};
static struct monitor monitor = {
	.si = &screen_info, .number = 0,
	.ewmhc.CurrentNumberOfDesktops = 3};

int monitor_compare(struct monitor *left, struct monitor *right)
{
	return (left->number > right->number) - (left->number < right->number);
}

RB_GENERATE(monitors, monitor, entry, monitor_compare);

const monitor_topology_snapshot *FScreenGetTopologySnapshot(void)
{
	return &topology;
}

struct monitor *monitor_resolve_name(const char *name)
{
	return name && strcmp(name, "0") == 0 ? &monitor : NULL;
}

bool fvwm_query_token(uint64_t revision, char output[49])
{
	snprintf(output, 49, "0123456789abcdef0123456789abcdef%016" PRIx64,
		revision);
	return revision != 0;
}

static cJSON *field(cJSON *object, const char *name)
{
	cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);

	assert(value);
	return value;
}

static cJSON *query(size_t owner, const char *cookie)
{
	struct fvwm_query_spec spec = {.limit = 16, .generation = "-"};
	const char *error;
	cJSON *result;

	strcpy(spec.cookie, cookie);
	result = action_palette_query(&owners[owner], &spec, &error);
	assert(result && !error);
	assert(strcmp(field(result, "state")->valuestring, "observed") == 0);
	assert(strcmp(field(result, "scope")->valuestring,
		"core-palette") == 0);
	return result;
}

static struct palette_request request(cJSON *result, unsigned int index)
{
	struct palette_request request = {.cookie = "a", .index = index};

	strcpy(request.generation, field(result, "generation")->valuestring);
	return request;
}

static void test_snapshot_and_guard(void)
{
	FvwmWindow fw = {.palette_identity = 1, .m = &monitor,
		.visible_name = "literal $[desk.n]; Exec bad"};
	struct palette_request action;
	struct palette_row row;
	char id[129], original[129], command[128];
	const char *error;
	cJSON *result, *rows, *item;
	struct fvwm_query_spec spec = {.cookie = "a", .offset = 1, .limit = 1};
	DesktopsInfo desktop_name = {.desk = 1, .name = "named desktop"};
	DesktopsInfo desktop_head = {.next = &desktop_name};

	monitor.Desktops = &desktop_head;
	FW_W(&fw) = 0x42;
	Scr.FvwmRoot.next = &fw;
	result = query(0, "a");
	assert(field(result, "total")->valueint == 4);
	assert(cJSON_IsFalse(field(result, "truncated")));
	rows = field(result, "rows");
	item = cJSON_GetArrayItem(rows, 0);
	assert(strcmp(field(item, "label")->valuestring, fw.visible_name) == 0);
	assert(strcmp(field(item, "kind")->valuestring, "window") == 0);
	strcpy(original, field(item, "id")->valuestring);
	item = cJSON_GetArrayItem(rows, 2);
	assert(strcmp(field(item, "label")->valuestring, "named desktop") == 0);
	assert(strcmp(field(item, "kind")->valuestring, "desktop") == 0);
	action = request(result, 0);
	assert(action_palette_target(&owners[0], &action, &row, id, &error));
	assert(!error && strcmp(id, original) == 0);
	assert(action_palette_command(&row, 0, command,
		sizeof(command), &error));
	assert(strcmp(command, "WindowId 0x42 Iconify off") == 0);
	assert(action_palette_command(&row, 1, command,
		sizeof(command), &error));
	assert(strcmp(command, "WindowId 0x42 Raise") == 0);
	assert(action_palette_command(&row, 2, command,
		sizeof(command), &error));
	assert(strcmp(command, "WindowId 0x42 Focus") == 0);
	/* Reuse the exact record and XID, but give its new management lifetime
	 * a fresh identity. Old snapshot must refuse it. */
	fw.palette_identity = 2;
	assert(!action_palette_target(&owners[0], &action, &row, id, &error));
	assert(strcmp(error, "stale_target") == 0);
	fw.palette_identity = 1;
	SET_DO_SKIP_WINDOW_LIST(&fw, 1);
	assert(!action_palette_target(&owners[0], &action, &row, id, &error));
	SET_DO_SKIP_WINDOW_LIST(&fw, 0);
	SET_SCHEDULED_FOR_DESTROY(&fw, 1);
	assert(!action_palette_target(&owners[0], &action, &row, id, &error));
	SET_SCHEDULED_FOR_DESTROY(&fw, 0);
	strcpy(spec.generation, action.generation);
	cJSON_Delete(result);
	result = action_palette_query(&owners[0], &spec, &error);
	assert(result && !error && field(result, "offset")->valueint == 1);
	assert(field(result, "next_offset")->valueint == 2);
	cJSON_Delete(result);
	result = query(0, "a");
	assert(!action_palette_target(&owners[0], &action, &row, id, &error));
	assert(strcmp(error, "stale_generation") == 0);
	item = cJSON_GetArrayItem(field(result, "rows"), 0);
	assert(strcmp(field(item, "id")->valuestring, original) == 0);
	action = request(result, 2);
	assert(action_palette_target(&owners[0], &action, &row, id, &error));
	assert(action_palette_command(&row, 0, command,
		sizeof(command), &error));
	assert(strcmp(command, "GotoDesk screen 0 0 1") == 0);
	topology.epoch++;
	assert(!action_palette_target(&owners[0], &action, &row, id, &error));
	assert(strcmp(error, "stale_target") == 0);
	topology.epoch--;
	monitor.ewmhc.CurrentNumberOfDesktops = 1;
	assert(!action_palette_target(&owners[0], &action, &row, id, &error));
	monitor.ewmhc.CurrentNumberOfDesktops = 3;
	Scr.FvwmRoot.next = NULL;
	cJSON_Delete(result);
	action_palette_owner_gone(&owners[0]);
	monitor.Desktops = NULL;
}

static void test_quota_release_and_replacement(void)
{
	cJSON *result;
	struct palette_request action = {.cookie = "a", .release = true};
	struct fvwm_query_spec spec = {.cookie = "a", .limit = 16,
		.generation = "-"};
	struct palette_row row;
	char id[129];
	const char *error;

	for (size_t i = 0; i < PALETTE_OWNER_MAX; i++)
		cJSON_Delete(query(i, "a"));
	assert(!action_palette_query(&owners[4], &spec, &error));
	assert(strcmp(error, "palette_busy") == 0);
	result = query(0, "b");
	assert(action_palette_target(&owners[0], &action, &row, id, &error));
	strcpy(action.cookie, "b");
	action.release = false;
	strcpy(action.generation, field(result, "generation")->valuestring);
	assert(action_palette_target(&owners[0], &action, &row, id, &error));
	assert(!action_palette_target(&owners[1], &action, &row, id, &error));
	action.release = true;
	assert(action_palette_target(&owners[0], &action, &row, id, &error));
	assert(!id[0]);
	cJSON_Delete(result);
	cJSON_Delete(query(4, "a"));
	action_palette_owner_gone(&owners[1]);
	cJSON_Delete(query(0, "new-view"));
	for (size_t i = 0; i < 5; i++)
		action_palette_owner_gone(&owners[i]);
}

static void test_limits_and_pagination(void)
{
	FvwmWindow *windows = calloc(PALETTE_SCAN_MAX + 1, sizeof(*windows));
	struct fvwm_query_spec spec = {.cookie = "a", .limit = 16};
	cJSON *result, *rows;
	const char *error;
	size_t observed = 0;

	assert(windows);
	for (size_t i = 0; i <= PALETTE_SCAN_MAX; i++)
	{
		windows[i].palette_identity = i + 1;
		windows[i].m = &monitor;
		windows[i].visible_name = "window";
		windows[i].next = i < PALETTE_SCAN_MAX ? &windows[i + 1] : NULL;
	}
	Scr.FvwmRoot.next = windows;
	monitor.ewmhc.CurrentNumberOfDesktops = PALETTE_DESKTOP_MAX + 1;
	result = query(0, "a");
	assert(cJSON_IsTrue(field(result, "truncated")));
	assert(field(result, "total")->valueint == PALETTE_ROW_MAX);
	strcpy(spec.generation, field(result, "generation")->valuestring);
	for (;;)
	{
		rows = field(result, "rows");
		for (int i = 0; i < cJSON_GetArraySize(rows); i++)
		{
			cJSON *row = cJSON_GetArrayItem(rows, i);

			assert(field(row, "index")->valueint == (int)observed);
			observed++;
		}
		if (cJSON_IsNull(field(result, "next_offset")))
			break;
		spec.offset = field(result, "next_offset")->valueint;
		cJSON_Delete(result);
		result = action_palette_query(&owners[0], &spec, &error);
		assert(result && !error);
	}
	assert(observed == PALETTE_ROW_MAX);
	cJSON_Delete(result);
	/* The scan cap must be reported, even when every inspected record is
	 * skipped and the sole eligible target is just outside the prefix. */
	for (size_t i = 0; i < PALETTE_SCAN_MAX; i++)
		SET_DO_SKIP_WINDOW_LIST(&windows[i], 1);
	monitor.ewmhc.CurrentNumberOfDesktops = 0;
	result = query(0, "a");
	assert(field(result, "total")->valueint == 0);
	assert(cJSON_IsTrue(field(result, "truncated")));
	cJSON_Delete(result);
	action_palette_owner_gone(&owners[0]);
	Scr.FvwmRoot.next = NULL;
	free(windows);
}

int main(void)
{
	RB_INSERT(monitors, &monitor_q, &monitor);
	test_snapshot_and_guard();
	test_quota_release_and_replacement();
	test_limits_and_pagination();
	RB_REMOVE(monitors, &monitor_q, &monitor);
	puts("action palette snapshots and guards: passed");
	return 0;
}
