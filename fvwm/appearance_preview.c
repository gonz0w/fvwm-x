/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "appearance_preview.h"
#include "colorset.h"
#include "execcontext.h"
#include "menustyle.h"
#include "query_json.h"
#include <string.h>
#include <time.h>

enum preview_state {
	PREVIEW_INACTIVE, PREVIEW_ACTIVE, PREVIEW_CONFIRMED, PREVIEW_REVERTED,
	PREVIEW_EXPIRED, PREVIEW_CONFLICTED, PREVIEW_ABANDONED
};

/* Exactly one fixed core-owned lease. No borrowed MenuStyle survives a call.
 * owner is cleared before module retirement/free; history is one slot. */
static struct {
	struct fmodule *owner;
	char cookie[65], view[17], id[49], generation[49], target[65];
	unsigned int previous, opacity;
	uint64_t deadline;
	enum preview_state state;
	const char *recovery;
} preview;
static uint64_t next_preview = 1;

enum resource_preview_state {
	RESOURCE_INACTIVE, RESOURCE_PREPARING, RESOURCE_PREPARED,
	RESOURCE_APPLYING, RESOURCE_ACTIVE, RESOURCE_RECOVERY_PENDING,
	RESOURCE_TERMINAL
};

/* No borrowed native record or module survives retirement. Only retained
 * operation resources live across command/interaction stacks; their cleanup,
 * record moves and broadcasts are confined to the outer event boundary. */
static struct {
	struct fmodule *owner;
	char cookie[65], view[17], id[49], generation[49];
	uint32_t colors[14][4];
	struct colorset_preview *bundle;
	struct menustyle_preview_pair menus;
	uint64_t deadline;
	unsigned int timeout_ms;
	enum resource_preview_state state;
	bool applied;
	const char *reason, *recovery, *preparation_error;
} resource_preview;

static bool resource_live(void)
{
	return resource_preview.state != RESOURCE_INACTIVE &&
		resource_preview.state != RESOURCE_TERMINAL;
}

static bool resource_actor(struct fmodule *owner,
	const struct preview_request *request)
{
	return resource_preview.state != RESOURCE_INACTIVE &&
		resource_preview.owner == owner &&
		strcmp(resource_preview.cookie, request->cookie) == 0 &&
		strcmp(resource_preview.view, request->view) == 0;
}

static uint64_t preview_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return UINT64_MAX;
	return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

static void resource_pending(const char *reason)
{
	resource_preview.state = RESOURCE_RECOVERY_PENDING;
	resource_preview.reason = reason;
}

static bool resource_generation_current(void)
{
	char current[49];

	return fvwm_query_appearance_token(current) &&
		strcmp(current, resource_preview.generation) == 0;
}

static void resource_service(void)
{
	enum preview_resource_guard guard;

	if (!resource_live())
		return;
	guard = preview_resource_guard_check(preview_now(),
		resource_preview.deadline, resource_generation_current(),
		resource_preview.owner != NULL);
	if (guard == PREVIEW_RESOURCE_CONFLICT)
		resource_pending("conflicted");
	else if (guard == PREVIEW_RESOURCE_OWNER_GONE)
		resource_pending("abandoned");
	else if (guard == PREVIEW_RESOURCE_EXPIRED &&
	    (resource_preview.state != RESOURCE_RECOVERY_PENDING ||
	     strcmp(resource_preview.reason, "confirmed") == 0))
		resource_pending("expired");
}

bool appearance_preview_outer_pending(void)
{
	return resource_preview.state == RESOURCE_PREPARING ||
		resource_preview.state == RESOURCE_APPLYING ||
		resource_preview.state == RESOURCE_RECOVERY_PENDING;
}

void appearance_preview_outer_boundary(void)
{
	const char *error = NULL;

	resource_service();
	if (resource_preview.state == RESOURCE_PREPARING)
	{
		if (!menustyle_preview_pair_capture(&resource_preview.menus))
			error = "preview_menu_unavailable";
		else
			resource_preview.bundle = colorset_preview_prepare(
				resource_preview.colors, &error);
		if (error)
		{
			resource_preview.preparation_error = error;
			resource_pending("refused");
		}
		else
			resource_preview.state = RESOURCE_PREPARED;
	}
	resource_service();
	if (resource_preview.state == RESOURCE_APPLYING)
	{
		if (!resource_preview.bundle ||
		    !preview_generation_room(resource_preview.generation) ||
		    !menustyle_preview_pair_set(&resource_preview.menus, false))
			resource_pending("conflicted");
		else
		{
			/* Both menu identities and every source generation validated.
			 * These setters and record moves cannot dispatch callbacks. */
			colorset_preview_move(resource_preview.bundle);
			resource_preview.applied = true;
			resource_preview.state = RESOURCE_ACTIVE;
			resource_preview.deadline = preview_deadline(preview_now(),
				resource_preview.timeout_ms);
			fvwm_query_appearance_token(resource_preview.generation);
			colorset_preview_broadcast();
		}
	}
	if (resource_preview.state == RESOURCE_RECOVERY_PENDING)
	{
		bool conflict = strcmp(resource_preview.reason, "conflicted") == 0;
		bool keep = strcmp(resource_preview.reason, "confirmed") == 0;

		if (resource_preview.applied && !conflict && !keep)
		{
			if (!resource_generation_current() ||
			    !menustyle_preview_pair_set(&resource_preview.menus, true))
				conflict = true;
			else
			{
				colorset_preview_move(resource_preview.bundle);
				fvwm_query_appearance_token(resource_preview.generation);
				colorset_preview_broadcast();
			}
		}
		if (conflict)
			resource_preview.reason = "conflicted";
		resource_preview.recovery = conflict ? "refused-conflict" :
			keep ? "kept" : resource_preview.applied ?
			"restored" : "not-applied";
		colorset_preview_release(resource_preview.bundle);
		resource_preview.bundle = NULL;
		resource_preview.state = RESOURCE_TERMINAL;
	}
}

static void preview_recover(enum preview_state state)
{
	char generation[49];
	enum preview_recovery decision;
	int previous;

	if (preview.state != PREVIEW_ACTIVE)
		return;
	fvwm_query_appearance_token(generation);
	decision = preview_recovery_check(preview_now(), preview.deadline,
		preview.generation, generation, true);
	if (decision == PREVIEW_CONFLICT || !menustyle_preview_opacity(
		preview.target, (int)preview.previous, &previous))
	{
		preview.state = PREVIEW_CONFLICTED;
		preview.recovery = "refused-conflict";
		return;
	}
	preview.state = state;
	preview.recovery = "restored";
	fvwm_query_appearance_token(preview.generation);
}

void appearance_preview_service(void)
{
	char generation[49];
	enum preview_recovery decision;

	resource_service();
	if (preview.state != PREVIEW_ACTIVE)
		return;
	fvwm_query_appearance_token(generation);
	decision = preview_recovery_check(preview_now(), preview.deadline,
		preview.generation, generation, false);
	if (decision == PREVIEW_CONFLICT)
	{
		preview.state = PREVIEW_CONFLICTED;
		preview.recovery = "refused-conflict";
	}
	else if (decision == PREVIEW_RESTORE)
		preview_recover(PREVIEW_EXPIRED);
}

int appearance_preview_timeout_ms(void)
{
	appearance_preview_service();
	if (resource_live())
		return resource_preview.state == RESOURCE_RECOVERY_PENDING ? -1 :
			preview_remaining(preview_now(), resource_preview.deadline);
	return preview.state == PREVIEW_ACTIVE ?
		preview_remaining(preview_now(), preview.deadline) : -1;
}

void appearance_preview_owner_gone(struct fmodule *owner)
{
	if (resource_preview.owner == owner)
	{
		/* Detach before module death-row cleanup can free the record.
		 * The next outer turn owns cleanup, even inside nested waits. */
		resource_preview.owner = NULL;
		if (resource_live())
			resource_pending("abandoned");
	}
	if (preview.owner != owner)
		return;
	preview_recover(PREVIEW_ABANDONED);
	memset(&preview, 0, sizeof(preview));
}

static bool preview_actor(struct fmodule *owner,
	const struct preview_request *request)
{
	return preview.owner == owner &&
		strcmp(preview.cookie, request->cookie) == 0 &&
		strcmp(preview.view, request->view) == 0;
}

cJSON *appearance_preview_capability(void)
{
	cJSON *result = cJSON_CreateObject();

	if (result &&
	    cJSON_AddNumberToObject(result, "version", 1) &&
	    cJSON_AddStringToObject(result, "scope", "high-contrast") &&
	    cJSON_AddNumberToObject(result, "role_count", 14) &&
	    cJSON_AddNumberToObject(result, "style_count", 2) &&
	    cJSON_AddNumberToObject(result, "metadata_max", PREVIEW_METADATA_MAX) &&
	    cJSON_AddNumberToObject(result, "preparation_timeout_ms",
		PREVIEW_PREPARATION_MS) &&
	    cJSON_AddNumberToObject(result, "lease_limit", 1) &&
	    cJSON_AddBoolToObject(result, "slot_available",
		!resource_live() && preview.state != PREVIEW_ACTIVE))
		return result;
	cJSON_Delete(result);
	return NULL;
}

static cJSON *resource_result(const char **error)
{
	static const char *states[] = {"inactive", "preparing", "prepared",
		"applying", "active", "recovery-pending", "terminal"};
	cJSON *result = cJSON_CreateObject();

	if (result &&
	    cJSON_AddStringToObject(result, "scope", "high-contrast") &&
	    cJSON_AddNumberToObject(result, "plan_version", 1) &&
	    cJSON_AddStringToObject(result, "state",
		states[resource_preview.state]) &&
	    cJSON_AddStringToObject(result, "lease", resource_preview.id) &&
	    cJSON_AddStringToObject(result, "generation",
		resource_preview.generation) &&
	    cJSON_AddStringToObject(result, "terminal_reason",
		resource_preview.state == RESOURCE_TERMINAL ?
		resource_preview.reason : "") &&
	    cJSON_AddStringToObject(result, "recovery", resource_preview.recovery) &&
	    cJSON_AddStringToObject(result, "preparation_error",
		resource_preview.preparation_error) &&
	    cJSON_AddStringToObject(result, "persistence", "not-attempted") &&
	    cJSON_AddStringToObject(result, "rendered_completion", "unknown") &&
	    cJSON_AddNumberToObject(result, "lease_limit", 1) &&
	    cJSON_AddNumberToObject(result, "preparation_limit_ms",
		PREVIEW_PREPARATION_MS) &&
	    cJSON_AddNumberToObject(result, "remaining_ms", resource_live() ?
		preview_remaining(preview_now(), resource_preview.deadline) : 0))
	{
		*error = NULL;
		return result;
	}
	cJSON_Delete(result);
	return NULL;
}

static cJSON *resource_request(struct fmodule *owner,
	const struct preview_request *request, const char **error)
{
	char generation[49], id[49];
	uint64_t now;

	if (request->operation == PREVIEW_PREPARE)
	{
		*error = "preview_busy";
		if (resource_live() || preview.state == PREVIEW_ACTIVE)
			return NULL;
		*error = "generation_unavailable";
		if (!fvwm_query_appearance_token(generation) || !next_preview ||
		    !fvwm_query_token(next_preview, id) ||
		    !preview_generation_room(generation))
			return NULL;
		*error = "stale_generation";
		if (strcmp(generation, request->generation) != 0)
			return NULL;
		now = preview_now();
		*error = "clock_unavailable";
		if (now == UINT64_MAX)
			return NULL;
		memset(&resource_preview, 0, sizeof(resource_preview));
		memset(&preview, 0, sizeof(preview));
		resource_preview.owner = owner;
		strcpy(resource_preview.cookie, request->cookie);
		strcpy(resource_preview.view, request->view);
		strcpy(resource_preview.id, id);
		strcpy(resource_preview.generation, generation);
		memcpy(resource_preview.colors, request->colors,
			sizeof(resource_preview.colors));
		resource_preview.deadline = preview_deadline(now,
			PREVIEW_PREPARATION_MS);
		resource_preview.timeout_ms = request->timeout_ms;
		resource_preview.state = RESOURCE_PREPARING;
		resource_preview.reason = "";
		resource_preview.recovery = "pending";
		resource_preview.preparation_error = "";
		next_preview++;
	}
	else
	{
		*error = "preview_unavailable";
		if (!resource_actor(owner, request))
			return NULL;
		if (request->operation == PREVIEW_QUERY)
			return resource_result(error);
		if (request->operation == PREVIEW_ABANDON)
		{
			if (resource_live())
				resource_pending("abandoned");
			return resource_result(error);
		}
		if (strcmp(resource_preview.id, request->lease) != 0)
			return NULL;
		*error = "preview_conflict";
		if (strcmp(resource_preview.reason, "conflicted") == 0)
			return NULL;
		*error = "stale_generation";
		if (strcmp(resource_preview.generation, request->generation) != 0)
			return NULL;
		*error = "preview_finished";
		if (request->operation == PREVIEW_APPLY)
		{
			if (resource_preview.state != RESOURCE_PREPARED)
				return NULL;
			resource_preview.state = RESOURCE_APPLYING;
		}
		else if (request->operation == PREVIEW_REVERT && resource_live())
			resource_pending("reverted");
		else if (request->operation == PREVIEW_CONFIRM &&
		    resource_preview.state == RESOURCE_ACTIVE)
			resource_pending("confirmed");
		else
			return NULL;
	}
	*error = "allocation_failed";
	return resource_result(error);
}

static cJSON *preview_result(bool owned, const char **error)
{
	static const char *states[] = {
		"inactive", "active", "confirmed", "reverted",
		"expired", "conflicted", "abandoned"};
	cJSON *result = cJSON_CreateObject();

	if (!result)
		return NULL;
	if (!cJSON_AddStringToObject(result, "scope", "menu-opacity") ||
	    !cJSON_AddStringToObject(result, "state",
		owned ? states[preview.state] : "inactive") ||
	    !cJSON_AddStringToObject(result, "persistence", "not-attempted") ||
	    !cJSON_AddStringToObject(
		result, "rendered_completion", "unknown") ||
	    !cJSON_AddNumberToObject(result, "lease_limit", 1))
		goto fail;
	if (owned &&
	    (!cJSON_AddStringToObject(result, "lease", preview.id) ||
	     !(preview.generation[0] ? cJSON_AddStringToObject(result,
		"generation", preview.generation) :
		cJSON_AddNullToObject(result, "generation")) ||
	     !cJSON_AddStringToObject(result, "target", preview.target) ||
	     !cJSON_AddNumberToObject(
		result, "previous_opacity", preview.previous) ||
	     !cJSON_AddNumberToObject(
		result, "preview_opacity", preview.opacity) ||
	     !cJSON_AddNumberToObject(result, "remaining_ms",
		preview.state == PREVIEW_ACTIVE ?
		preview_remaining(preview_now(), preview.deadline) : 0) ||
	     !cJSON_AddStringToObject(result, "recovery", preview.recovery)))
		goto fail;
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

cJSON *appearance_preview_request(struct fmodule *owner,
	const struct preview_request *request, const char **error)
{
	char generation[49], id[49];
	uint64_t now;
	int previous;
	bool owned;

	appearance_preview_service();
	if (request->operation == PREVIEW_PREPARE ||
	    request->operation == PREVIEW_APPLY ||
	    (request->operation != PREVIEW_START && resource_actor(owner, request)))
		return resource_request(owner, request, error);
	owned = preview_actor(owner, request);
	*error = "allocation_failed";
	if (request->operation == PREVIEW_QUERY)
		return preview_result(owned, error);
	if (request->operation == PREVIEW_ABANDON)
	{
		if (owned)
			preview_recover(PREVIEW_ABANDONED);
		return preview_result(owned, error);
	}
	if (request->operation == PREVIEW_START)
	{
		*error = "preview_busy";
		if (preview.state == PREVIEW_ACTIVE || resource_live())
			return NULL;
		*error = "generation_unavailable";
		if (!fvwm_query_appearance_token(generation) || !next_preview ||
		    !fvwm_query_token(next_preview, id))
			return NULL;
		*error = "stale_generation";
		if (strcmp(generation, request->generation) != 0)
			return NULL;
		now = preview_now();
		*error = "clock_unavailable";
		if (now == UINT64_MAX)
			return NULL;
		*error = "preview_unavailable";
		if (!menustyle_preview_opacity(request->target,
			(int)request->opacity, &previous))
			return NULL;
		/* No callbacks run inside the setter. On revision exhaustion,
		 * restore the scalar inside this same atomic command. */
		if (!fvwm_query_appearance_token(generation))
		{
			int ignored;

			menustyle_preview_opacity(
				request->target, previous, &ignored);
			*error = "generation_unavailable";
			return NULL;
		}
		memset(&preview, 0, sizeof(preview));
		memset(&resource_preview, 0, sizeof(resource_preview));
		preview.owner = owner;
		strcpy(preview.cookie, request->cookie);
		strcpy(preview.view, request->view);
		strcpy(preview.id, id);
		strcpy(preview.generation, generation);
		strcpy(preview.target, request->target);
		preview.previous = (unsigned int)previous;
		preview.opacity = request->opacity;
		preview.deadline = preview_deadline(now, request->timeout_ms);
		preview.state = PREVIEW_ACTIVE;
		preview.recovery = "pending";
		next_preview++;
	}
	else
	{
		*error = "preview_unavailable";
		if (!owned || strcmp(request->lease, preview.id) != 0)
			return NULL;
		*error = "preview_conflict";
		if (preview.state == PREVIEW_CONFLICTED)
			return NULL;
		*error = "preview_finished";
		if (preview.state != PREVIEW_ACTIVE)
			return NULL;
		*error = "stale_generation";
		if (strcmp(request->generation, preview.generation) != 0)
			return NULL;
		if (request->operation == PREVIEW_REVERT)
			preview_recover(PREVIEW_REVERTED);
		else
		{
			preview.state = PREVIEW_CONFIRMED;
			preview.recovery = "kept";
		}
	}
	*error = "allocation_failed";
	return preview_result(true, error);
}
