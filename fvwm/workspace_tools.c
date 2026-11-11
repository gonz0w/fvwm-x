/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "workspace_tools.h"
#include "palette_policy.h"
#include "fvwm.h"
#include "execcontext.h"
#include "screen.h"
#include "focus.h"
#include "ewmh.h"
#include "query_json.h"
#include "module_records.h"
#include "libs/monitor_types.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static struct scratchpad_registry registry;
static uint64_t next_lease = 1;

void scratchpad_initialize(bool restarted)
{
	scratchpad_registry_initialize(&registry, restarted);
}

void scratchpad_owner_gone(struct fmodule *owner)
{
	scratchpad_registry_owner_gone(&registry, (uintptr_t)owner);
}

static bool eligible(const FvwmWindow *fw)
{
	fmodule_list_itr iterator;
	fmodule *module;
	size_t scanned = 0;

	if (!fw->palette_identity || !fw->m || !fw->m->si ||
	    IS_SCHEDULED_FOR_DESTROY(fw) || IS_TEAR_OFF_MENU(fw) ||
	    (fw->ewmh_window_type != EWMH_WINDOW_TYPE_NONE_ID &&
	     fw->ewmh_window_type != EWMH_WINDOW_TYPE_NORMAL_ID &&
	     fw->ewmh_window_type != EWMH_WINDOW_TYPE_DIALOG_ID))
		return false;
	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		const char *name = MOD_NAME(module), *base, *instance;

		if (MOD_IS_RETIRED(module))
			continue;
		if (++scanned > 128 || !name || strnlen(name, 4097) > 4096)
		{
			module_list_itr_finish(&iterator);
			return false;
		}
		base = strrchr(name, '/');
		base = base ? base + 1 : name;
		instance = MOD_ALIAS(module) && *MOD_ALIAS(module) ?
			MOD_ALIAS(module) : base;
		if (fw->class.res_class && fw->class.res_name &&
		    strcmp(fw->class.res_class, base) == 0 &&
		    strcmp(fw->class.res_name, instance) == 0)
		{
			module_list_itr_finish(&iterator);
			return false;
		}
	}
	return true;
}

static bool unsupported(const FvwmWindow *fw)
{
	return IS_TRANSIENT(fw) || IS_STICKY_ACROSS_DESKS(fw) ||
		IS_STICKY_ACROSS_PAGES(fw) || IS_EWMH_FULLSCREEN(fw);
}

static FvwmWindow *find_window(uint64_t identity)
{
	FvwmWindow *fw;
	size_t scanned = 0;

	for (fw = Scr.FvwmRoot.next; fw && scanned < PALETTE_SCAN_MAX;
	     fw = fw->next, scanned++)
		if (fw->palette_identity == identity && eligible(fw))
			return fw;
	return NULL;
}

/* An incomplete internal-module scope must refuse, never imply no match. */
static bool module_scope_valid(void)
{
	fmodule_list_itr iterator;
	fmodule *module;
	size_t count = 0;

	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		const char *name = MOD_NAME(module);

		if (MOD_IS_RETIRED(module))
			continue;
		if (++count > 128 || !name || strnlen(name, 4097) > 4096)
		{
			module_list_itr_finish(&iterator);
			return false;
		}
	}
	return true;
}

static bool observe(const struct scratchpad_request *request,
	struct scratchpad_observation *observation)
{
	struct scratchpad_entry *entry =
		scratchpad_registry_find(&registry, request->name);
	FvwmWindow *fw = entry ? find_window(entry->target) : NULL;
	struct monitor *m = monitor_get_current();
	size_t scanned = 0;

	memset(observation, 0, sizeof(*observation));
	if (!module_scope_valid())
		return false;
	if (!fw)
	{
		for (fw = Scr.FvwmRoot.next; fw && scanned < PALETTE_SCAN_MAX;
		     fw = fw->next, scanned++)
		{
			if (!eligible(fw) || !fw->class.res_class ||
			    strcmp(fw->class.res_class, request->class_name) ||
			    (request->instance[0] && (!fw->class.res_name ||
			     strcmp(fw->class.res_name, request->instance))))
				continue;
			observation->matches++;
			observation->target = fw->palette_identity;
		}
		if (fw)
			return false;
		fw = observation->matches == 1 ?
			find_window(observation->target) : NULL;
	}
	else
		observation->matches = 1;
	if (fw)
	{
		observation->target = fw->palette_identity;
		observation->unsupported = unsupported(fw);
		observation->visible = !IS_ICONIFIED(fw) && m && m->si &&
			fw->m == m && fw->Desk == m->virtual_scr.CurrentDesk &&
			(int64_t)fw->g.frame.x + fw->g.frame.width > m->si->x &&
			(int64_t)fw->g.frame.y + fw->g.frame.height >
			m->si->y &&
			fw->g.frame.x < (int64_t)m->si->x + m->si->w &&
			fw->g.frame.y < (int64_t)m->si->y + m->si->h;
	}
	return true;
}

cJSON *scratchpad_prepare(struct fmodule *owner,
	const struct scratchpad_request *request, struct scratchpad_plan *plan,
	const char **error)
{
	static const char *states[] = {"unmatched", "claimed", "pending",
		"uncertain", "ambiguous", "unsupported"};
	static const char *actions[] = {"none", "launch", "summoned",
		"dismissed"};
	struct scratchpad_observation observation;
	struct scratchpad_result result;
	struct scratchpad_entry *entry;
	struct timespec now;
	FvwmWindow *focused = get_focus_window();
	uint64_t prior = focused && eligible(focused) ?
		focused->palette_identity : 0, lease_id;
	char lease[49], target[PALETTE_ID_MAX + 1];
	cJSON *json;

	memset(plan, 0, sizeof(*plan));
	*error = "unavailable";
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
	    (uint64_t)now.tv_sec > (UINT64_MAX - 999) / 1000 ||
	    !observe(request, &observation) ||
	    !palette_next_identity(&next_lease, &lease_id) ||
	    !fvwm_query_token(lease_id, lease))
		return NULL;
	if (!scratchpad_registry_apply(&registry, (uintptr_t)owner, request,
	    &observation, (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000,
	    lease, &result))
	{
		*error = result.reason;
		return NULL;
	}
	entry = scratchpad_registry_find(&registry, request->name);
	if ((request->verb == SCRATCHPAD_TOGGLE ||
	     request->verb == SCRATCHPAD_RETRY) &&
	    (result.action == SCRATCHPAD_LAUNCH ||
	     result.action == SCRATCHPAD_SUMMON) &&
	    prior != entry->target)
		entry->prior_focus = prior;
	plan->owner = (uintptr_t)owner;
	strcpy(plan->name, request->name);
	strcpy(plan->serial, request->serial);
	plan->target = entry->target;
	plan->prior_focus = entry->prior_focus;
	plan->action = result.action;
	plan->owns_focus = focused &&
		focused->palette_identity == entry->target;
	*error = "allocation-failed";
	json = cJSON_CreateObject();
	target[0] = '\0';
	if (result.target)
	{
		struct palette_row row = {.kind = PALETTE_WINDOW,
			.identity = result.target};
		palette_row_id(target, lease, &row);
	}
	if (!json || !cJSON_AddNumberToObject(json, "version", 1) ||
	    !cJSON_AddStringToObject(json, "name", request->name) ||
	    !cJSON_AddStringToObject(json, "state", states[result.state]) ||
	    !cJSON_AddBoolToObject(json, "desired_visible",
		result.desired_visible) ||
	    !cJSON_AddStringToObject(json, "action", actions[result.action]) ||
	    !cJSON_AddStringToObject(json, "reason", result.reason) ||
	    !(result.lease[0] ?
		cJSON_AddStringToObject(json, "lease", result.lease) :
		cJSON_AddNullToObject(json, "lease")) ||
	    !(target[0] ? cJSON_AddStringToObject(json, "target", target) :
		cJSON_AddNullToObject(json, "target")))
	{
		cJSON_Delete(json);
		return NULL;
	}
	*error = NULL;
	return json;
}

bool scratchpad_command(const struct scratchpad_plan *plan, unsigned int step,
	char *command, size_t size, const char **error)
{
	struct scratchpad_entry *entry =
		scratchpad_registry_find(&registry, plan->name);
	FvwmWindow *fw = find_window(plan->target);
	struct monitor *m;
	char number[32];
	int x, y, width, height;

	command[0] = '\0';
	*error = "stale-target";
	if (!entry || entry->owner != plan->owner ||
	    strcmp(entry->serial, plan->serial) ||
	    entry->target != plan->target ||
	    entry->state != SCRATCHPAD_CLAIMED || !fw || unsupported(fw))
		return false;
	if (plan->action == SCRATCHPAD_DISMISS)
	{
		if (!step)
			snprintf(command, size, "WindowId 0x%lx Iconify on",
				FW_W(fw));
		else if (step == 1)
		{
			FvwmWindow *focus = get_focus_window();
			FvwmWindow *prior = find_window(plan->prior_focus);

			/* Do not steal a recursive or user focus transition. */
			if (plan->owns_focus && prior && !IS_ICONIFIED(prior) &&
			    (!focus || focus == fw || focus == prior))
				snprintf(command, size, "WindowId 0x%lx Focus",
					FW_W(prior));
		}
		else
			return false;
	}
	else if (plan->action == SCRATCHPAD_SUMMON)
	{
		m = monitor_get_current();
		if (!m || !m->si || (m->flags & MONITOR_DISABLED) ||
		    m->number < 0)
			return false;
		snprintf(number, sizeof(number), "%d", m->number);
		if (monitor_resolve_name(number) != m)
			return false;
		x = m->si->x;
		y = m->si->y;
		width = m->si->w;
		height = m->si->h;
		EWMH_GetWorkAreaIntersection(m, &x, &y, &width, &height,
			EWMH_USE_DYNAMIC_WORKING_AREA);
		if (!workspace_center(x, y, width, height,
		    fw->g.frame.width, fw->g.frame.height, &x, &y))
			return false;
		switch (step)
		{
		case 0:
			snprintf(command, size,
				"WindowId 0x%lx MoveToDesk 0 %d",
				FW_W(fw), m->virtual_scr.CurrentDesk);
			break;
		case 1:
			snprintf(command, size, "WindowId 0x%lx Iconify off",
				FW_W(fw));
			break;
		case 2:
			/* Leading + makes negative root offsets literal. */
			snprintf(command, size,
				"WindowId 0x%lx Move +%dp +%dp ewmhiwa",
				FW_W(fw), x, y);
			break;
		case 3:
			snprintf(command, size, "WindowId 0x%lx Raise",
				FW_W(fw));
			break;
		case 4:
			snprintf(command, size, "WindowId 0x%lx Focus",
				FW_W(fw));
			break;
		default:
			return false;
		}
	}
	else
		return false;
	*error = NULL;
	return true;
}
