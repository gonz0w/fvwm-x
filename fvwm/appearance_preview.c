/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "appearance_preview.h"
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

static uint64_t preview_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return UINT64_MAX;
	return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
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
	return preview.state == PREVIEW_ACTIVE ?
		preview_remaining(preview_now(), preview.deadline) : -1;
}

void appearance_preview_owner_gone(struct fmodule *owner)
{
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
		if (preview.state == PREVIEW_ACTIVE)
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
