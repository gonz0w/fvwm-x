/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "session_clients.h"
#include "fvwm.h"
#include "execcontext.h"
#include "screen.h"
#include "query_json.h"
#include "session.h"
#include "commands.h"
#include "decorations.h"
#include "module_records.h"
#include <stdio.h>
#include <string.h>

#define SESSION_MODULE_CLASSES_MAX 128

static struct session_clients_membership membership = {.revision = 1};

struct session_module_class {
	const char *program;
	const char *instance;
};

struct session_clients_scan {
	FvwmWindow *windows[SESSION_CLIENTS_MAX];
	size_t count;
	char generation[MODULE_QUERY_GENERATION_LEN + 1];
};

static bool module_classes(struct session_module_class *classes, size_t *count)
{
	fmodule_list_itr iterator;
	fmodule *module;

	*count = 0;
	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		const char *name = MOD_NAME(module), *base;

		if (MOD_IS_RETIRED(module))
			continue;
		if (*count == SESSION_MODULE_CLASSES_MAX || !name ||
		    strnlen(name, 4097) > 4096)
		{
			module_list_itr_finish(&iterator);
			return false;
		}
		base = strrchr(name, '/');
		classes[*count].program = base ? base + 1 : name;
		classes[*count].instance = MOD_ALIAS(module) &&
			*MOD_ALIAS(module) ? MOD_ALIAS(module) :
			classes[*count].program;
		(*count)++;
	}
	/* These borrowed class strings live only until this nonrecursive scan
	 * returns, never across a shared command-engine call. */
	return true;
}

static bool eligible(const FvwmWindow *fw,
	const struct session_module_class *classes, size_t count)
{
	if (!fw->palette_identity || !fw->m || IS_SCHEDULED_FOR_DESTROY(fw) ||
	    IS_TEAR_OFF_MENU(fw) ||
	    (fw->ewmh_window_type != EWMH_WINDOW_TYPE_NONE_ID &&
	     fw->ewmh_window_type != EWMH_WINDOW_TYPE_NORMAL_ID &&
	     fw->ewmh_window_type != EWMH_WINDOW_TYPE_DIALOG_ID))
		return false;
	for (size_t i = 0; i < count; i++)
		if (fw->class.res_class && fw->class.res_name &&
		    strcmp(fw->class.res_class, classes[i].program) == 0 &&
		    strcmp(fw->class.res_name, classes[i].instance) == 0)
			return false;
	return true;
}

static bool scan_clients(struct session_clients_scan *scan, const char **error)
{
	struct session_module_class classes[SESSION_MODULE_CLASSES_MAX];
	uint64_t identities[SESSION_CLIENTS_MAX];
	size_t class_count, scanned = 0;
	FvwmWindow *fw;

	memset(scan, 0, sizeof(*scan));
	*error = "session_scope_too_large";
	if (!module_classes(classes, &class_count))
		return false;
	for (fw = Scr.FvwmRoot.next; fw && scanned < SESSION_CLIENTS_SCAN_MAX;
	     fw = fw->next, scanned++)
	{
		if (!eligible(fw, classes, class_count))
			continue;
		if (scan->count == SESSION_CLIENTS_MAX)
			return false;
		scan->windows[scan->count] = fw;
		identities[scan->count++] = fw->palette_identity;
	}
	if (fw)
		return false;
	*error = "generation_unavailable";
	if (!session_clients_membership_update(&membership, identities,
	    scan->count) ||
	    !fvwm_query_token(membership.revision, scan->generation))
		return false;
	*error = NULL;
	return true;
}

static bool can_delete(const FvwmWindow *fw)
{
	return WM_DELETES_WINDOW(fw) && is_function_allowed(
		F_DELETE, NULL, fw, RQORIG_PROGRAM_US, False);
}

cJSON *session_clients_query(const struct fvwm_query_spec *spec,
	const char **error)
{
	struct session_clients_scan scan;
	cJSON *result = NULL, *clients;
	size_t end;

	if (!scan_clients(&scan, error))
		return NULL;
	*error = "stale_generation";
	if ((spec->offset && strcmp(spec->generation, "-") == 0) ||
	    (strcmp(spec->generation, "-") != 0 &&
	     strcmp(spec->generation, scan.generation) != 0))
		return NULL;
	*error = "invalid_offset";
	if (!spec->limit || spec->limit > SESSION_CLIENTS_PAGE_MAX ||
	    spec->offset > scan.count)
		return NULL;
	end = spec->offset + spec->limit;
	if (end > scan.count)
		end = scan.count;
	*error = "allocation_failed";
	result = cJSON_CreateObject();
	if (!result)
		return NULL;
	clients = cJSON_AddArrayToObject(result, "clients");
	if (!clients || !cJSON_AddStringToObject(result, "state", "observed") ||
	    !cJSON_AddStringToObject(result, "scope", "session-clients") ||
	    !cJSON_AddStringToObject(result, "generation", scan.generation) ||
	    !cJSON_AddNumberToObject(result, "offset", spec->offset) ||
	    !(end < scan.count ?
		cJSON_AddNumberToObject(result, "next_offset", end) :
		cJSON_AddNullToObject(result, "next_offset")) ||
	    !cJSON_AddNumberToObject(result, "total", scan.count) ||
	    !cJSON_AddBoolToObject(result, "truncated", false) ||
	    !cJSON_AddBoolToObject(result, "xsmp", session_xsmp_connected()) ||
	    !cJSON_AddStringToObject(result, "coverage",
		"managed-applications-excluding-active-module-classes-"
		"and-dock-desktop"))
		goto fail;
	for (size_t i = spec->offset; i < end; i++)
	{
		FvwmWindow *fw = scan.windows[i];
		char target[49], title[SESSION_CLIENTS_TITLE_MAX + 1];
		cJSON *item = cJSON_CreateObject();

		if (!item)
			goto fail;
		if (!cJSON_AddItemToArray(clients, item))
		{
			cJSON_Delete(item);
			goto fail;
		}
		if (!fvwm_query_token(fw->palette_identity, target))
			goto fail;
		session_clients_title(title,
			fw->visible_name ? fw->visible_name : fw->name.name);
		if (!cJSON_AddStringToObject(item, "target", target) ||
		    !cJSON_AddBoolToObject(item, "can_delete",
			can_delete(fw)) ||
		    !cJSON_AddBoolToObject(item, "wm_delete",
			WM_DELETES_WINDOW(fw)) ||
		    !cJSON_AddStringToObject(item, "title", title))
			goto fail;
	}
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

static cJSON *action_result(const struct session_clients_request *request,
	const char *state, const char *reason, const char **error)
{
	cJSON *result = cJSON_CreateObject();
	const char *operation = request->operation == SESSION_CLIENTS_DELETE ?
		"delete" : request->operation == SESSION_CLIENTS_QUIT_IF_EMPTY ?
		"quit-if-empty" : "delegate-xsmp";

	*error = "allocation_failed";
	if (!result || !cJSON_AddStringToObject(result, "state", state) ||
	    !cJSON_AddStringToObject(result, "operation", operation) ||
	    (reason && !cJSON_AddStringToObject(result, "reason", reason)) ||
	    (request->operation == SESSION_CLIENTS_DELEGATE_XSMP && !reason &&
	     (!cJSON_AddBoolToObject(result, "delegated", true) ||
	      !cJSON_AddBoolToObject(result, "cancel_supported", false))))
	{
		cJSON_Delete(result);
		return NULL;
	}
	*error = NULL;
	return result;
}

cJSON *session_clients_action(const struct session_clients_request *request,
	char command[96], const char **error)
{
	struct session_clients_scan scan;
	const char *reason = NULL;
	const char *state = "requested";

	command[0] = '\0';
	if (request->operation == SESSION_CLIENTS_DELEGATE_XSMP)
	{
		if (!session_xsmp_connected())
			state = "refused", reason = "xsmp_unavailable";
		else
			strcpy(command, "QuitSession");
	}
	else if (session_xsmp_connected())
		state = "refused", reason = "xsmp_active";
	else if (!scan_clients(&scan, error))
		state = "refused", reason = "scope_unavailable";
	else if (request->operation == SESSION_CLIENTS_QUIT_IF_EMPTY)
	{
		if (strcmp(request->generation, scan.generation) != 0)
			state = "stale", reason = "stale_generation";
		else if (scan.count)
			state = "refused", reason = "clients_remaining";
		else
			strcpy(command, "Quit");
	}
	else
	{
		state = "stale", reason = "stale_target";
		for (size_t i = 0; i < scan.count; i++)
		{
			FvwmWindow *fw = scan.windows[i];
			char target[49];

			if (!fvwm_query_token(fw->palette_identity, target) ||
			    strcmp(target, request->target) != 0)
				continue;
			if (!can_delete(fw))
				state = "refused", reason = "not_deletable";
			else
			{
				snprintf(command, 96, "WindowId 0x%lx Delete",
					FW_W(fw));
				state = "requested", reason = NULL;
			}
			break;
		}
	}
	return action_result(request, state, reason, error);
}

bool session_clients_quit_ready(const char *generation)
{
	struct session_clients_scan scan;
	const char *error;

	return !session_xsmp_connected() && scan_clients(&scan, &error) &&
		!scan.count && strcmp(generation, scan.generation) == 0;
}
