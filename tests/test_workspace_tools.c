/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "fvwm/workspace_tools.h"
#include "fvwm/execcontext.h"
#include "fvwm/screen.h"
#include "fvwm/module_records.h"
#include "libs/monitor_types.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

ScreenInfo Scr;
static struct screen_info screen_info = {
	.x = -1920, .y = 0, .w = 1920, .h = 1080};
static struct monitor monitor = {.si = &screen_info, .number = 0};
static FvwmWindow *focused;

struct monitor *monitor_get_current(void) { return &monitor; }
struct monitor *monitor_resolve_name(const char *name)
{
	return strcmp(name, "0") == 0 ? &monitor : NULL;
}
FvwmWindow *get_focus_window(void) { return focused; }
void module_list_itr_init(fmodule_list_itr *iterator)
{
	memset(iterator, 0, sizeof(*iterator));
}
void module_list_itr_finish(fmodule_list_itr *iterator) { (void)iterator; }
fmodule *module_list_itr_next(fmodule_list_itr *iterator)
{
	(void)iterator;
	return NULL;
}
void EWMH_GetWorkAreaIntersection(struct monitor *m,
	int *x, int *y, int *w, int *h, int type)
{
	(void)m; (void)type;
	*x = -1920; *y = 20; *w = 1920; *h = 1040;
}
bool fvwm_query_token(uint64_t revision, char output[49])
{
	snprintf(output, 49, "0123456789abcdef0123456789abcdef%016" PRIx64,
		revision);
	return revision != 0;
}

static struct scratchpad_request request(enum scratchpad_verb verb,
	unsigned int serial)
{
	struct scratchpad_request request = {.cookie = "abc",
		.name = "terminal", .class_name = "Terminal", .verb = verb};

	memset(request.fingerprint, 'a', 64);
	snprintf(request.serial, sizeof(request.serial), "%016x", serial);
	return request;
}

int main(void)
{
	struct fmodule owner = {0};
	FvwmWindow prior = {.palette_identity = 1, .m = &monitor};
	FvwmWindow fw = {.palette_identity = 2, .m = &monitor};
	FvwmWindow other = {.palette_identity = 3, .m = &monitor};
	struct scratchpad_request r = request(SCRATCHPAD_TOGGLE, 1);
	struct scratchpad_plan plan, old_plan;
	const char *error;
	cJSON *result;
	char command[160];

	FW_W(&prior) = 0x41;
	FW_W(&fw) = 0x42;
	fw.class.res_class = "Terminal";
	fw.g.frame.width = 800;
	fw.g.frame.height = 600;
	prior.next = &other;
	Scr.FvwmRoot.next = &prior;
	focused = &prior;
	scratchpad_initialize(false);
	result = scratchpad_prepare(&owner, &r, &plan, &error);
	assert(result && !error && plan.action == SCRATCHPAD_LAUNCH);
	cJSON_Delete(result);
	/* Mapping must not replace the stable pre-launch focus. */
	other.next = &fw;
	focused = &fw;
	r = request(SCRATCHPAD_STATUS, 2);
	result = scratchpad_prepare(&owner, &r, &plan, &error);
	assert(result && plan.action == SCRATCHPAD_SUMMON &&
		plan.prior_focus == 1);
	cJSON_Delete(result);
	assert(scratchpad_command(&plan, 2, command, sizeof(command), &error));
	assert(strcmp(command,
		"WindowId 0x42 Move +-1360p +240p ewmhiwa") == 0);
	/* Oversize frames stay the same size with their origin reachable. */
	fw.g.frame.width = 2400;
	assert(scratchpad_command(&plan, 2, command, sizeof(command), &error));
	assert(strcmp(command,
		"WindowId 0x42 Move +-1920p +240p ewmhiwa") == 0);
	/* An old action cannot cross destruction and XID reuse. */
	fw.palette_identity = 4;
	assert(!scratchpad_command(&plan, 3, command, sizeof(command), &error));
	fw.palette_identity = 2;
	SET_TRANSIENT(&fw, 1);
	assert(!scratchpad_command(&plan, 3, command, sizeof(command), &error));
	SET_TRANSIENT(&fw, 0);
	fw.g.frame.x = -1800;
	r = request(SCRATCHPAD_STATUS, 3);
	result = scratchpad_prepare(&owner, &r, &plan, &error);
	assert(result && plan.action == SCRATCHPAD_NONE);
	cJSON_Delete(result);
	r = request(SCRATCHPAD_TOGGLE, 4);
	result = scratchpad_prepare(&owner, &r, &plan, &error);
	assert(result && plan.action == SCRATCHPAD_DISMISS && plan.owns_focus);
	cJSON_Delete(result);
	assert(scratchpad_command(&plan, 0, command, sizeof(command), &error));
	focused = NULL;
	assert(scratchpad_command(&plan, 1, command, sizeof(command), &error));
	assert(strcmp(command, "WindowId 0x41 Focus") == 0);
	focused = &other;
	assert(scratchpad_command(&plan, 1, command, sizeof(command), &error));
	assert(!command[0]);
	/* A new service intent cancels the old plan between engine calls. */
	old_plan = plan;
	r = request(SCRATCHPAD_STATUS, 5);
	result = scratchpad_prepare(&owner, &r, &plan, &error);
	assert(result);
	cJSON_Delete(result);
	assert(!scratchpad_command(&old_plan, 0, command,
		sizeof(command), &error));
	scratchpad_owner_gone(&owner);
	assert(!scratchpad_command(&plan, 0, command, sizeof(command), &error));
	return 0;
}
