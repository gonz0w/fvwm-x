/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "workspace_tools_policy.h"
#include "palette_policy.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool hex(const char *text, size_t size)
{
	return strlen(text) == size && strspn(text, "0123456789abcdef") == size;
}

static bool decode(char *out, size_t maximum, const char *text)
{
	static const char digits[] = "0123456789abcdef";
	size_t length = strlen(text);
	char checked[PALETTE_TEXT_MAX + 1];

	if (!length || length % 2 || length / 2 > maximum)
		return false;
	for (size_t i = 0; i < length; i += 2)
	{
		const char *a = strchr(digits, text[i]);
		const char *b = strchr(digits, text[i + 1]);

		if (!a || !b)
			return false;
		out[i / 2] = (char)((a - digits) * 16 + b - digits);
		if (!out[i / 2])
			return false;
	}
	out[length / 2] = '\0';
	return palette_text(checked, out, "") && strcmp(out, checked) == 0;
}

bool scratchpad_request_parse(const char *text, struct scratchpad_request *out)
{
	struct scratchpad_request parsed = {0};
	char input[1001], *tokens[10], *token, *save;
	const char *verb, *instance;
	size_t count = 0;
	static const char *verbs[] = {"toggle", "retry", "status", "launched",
		"launch-failed"};
	bool found = false;

	memset(out, 0, sizeof(*out));
	if (!text || strnlen(text, sizeof(input)) >= sizeof(input))
		return false;
	strcpy(input, text);
	for (token = strtok_r(input, " \t\r\n", &save); token && count < 10;
	     token = strtok_r(NULL, " \t\r\n", &save))
		tokens[count++] = token;
	if (count != 9 || token || strcmp(tokens[0], "v1") ||
	    !tokens[1][0] || strlen(tokens[1]) > 64 ||
	    strspn(tokens[1], "0123456789abcdef-") != strlen(tokens[1]) ||
	    !hex(tokens[2], 16) || !hex(tokens[5], 64) ||
	    !decode(parsed.name, 32, tokens[4]) ||
	    strspn(parsed.name,
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789_-") != strlen(parsed.name) ||
	    !decode(parsed.class_name, 128, tokens[6]) ||
	    strlen(tokens[8]) > 48)
		return false;
	strcpy(parsed.cookie, tokens[1]);
	strcpy(parsed.serial, tokens[2]);
	strcpy(parsed.fingerprint, tokens[5]);
	strcpy(parsed.lease, tokens[8]);
	verb = tokens[3];
	instance = tokens[7];
	for (size_t i = 0; i < sizeof(verbs) / sizeof(verbs[0]); i++)
	{
		if (strcmp(verb, verbs[i]) == 0)
		{
			parsed.verb = (enum scratchpad_verb)i;
			found = true;
			break;
		}
	}
	if (!found || (strcmp(instance, "-") != 0 &&
	    !decode(parsed.instance, 128, instance)))
		return false;
	if (parsed.verb == SCRATCHPAD_LAUNCHED ||
	    parsed.verb == SCRATCHPAD_LAUNCH_FAILED)
	{
		if (!hex(parsed.lease, 48))
			return false;
	}
	else if (strcmp(parsed.lease, "-") != 0)
		return false;
	if (strcmp(parsed.lease, "-") == 0)
		parsed.lease[0] = '\0';
	*out = parsed;
	return true;
}

void scratchpad_registry_initialize(struct scratchpad_registry *registry,
	bool restarted)
{
	memset(registry, 0, sizeof(*registry));
	registry->restarted = restarted;
}

struct scratchpad_entry *scratchpad_registry_find(
	struct scratchpad_registry *registry, const char *name)
{
	for (size_t i = 0; i < SCRATCHPAD_MAX; i++)
		if (strcmp(registry->entries[i].name, name) == 0)
			return &registry->entries[i];
	return NULL;
}

void scratchpad_registry_owner_gone(struct scratchpad_registry *registry,
	uintptr_t owner)
{
	for (size_t i = 0; i < SCRATCHPAD_MAX; i++)
	{
		struct scratchpad_entry *entry = &registry->entries[i];

		if (entry->owner != owner)
			continue;
		entry->owner = 0;
		entry->cookie[0] = entry->serial[0] = '\0';
		if (entry->state == SCRATCHPAD_PENDING)
			entry->state = SCRATCHPAD_UNCERTAIN;
	}
}

static void copy_result(const struct scratchpad_entry *entry,
	struct scratchpad_result *result, const char *reason)
{
	result->state = entry->state;
	result->desired_visible = entry->desired_visible;
	result->target = entry->target;
	strcpy(result->lease, entry->lease);
	result->reason = reason;
}

bool scratchpad_registry_apply(struct scratchpad_registry *registry,
	uintptr_t owner, const struct scratchpad_request *request,
	const struct scratchpad_observation *observation, uint64_t now,
	const char *new_lease, struct scratchpad_result *result)
{
	struct scratchpad_entry *entry = scratchpad_registry_find(registry,
		request->name);
	bool was_waiting;

	memset(result, 0, sizeof(*result));
	result->reason = "owner-conflict";
	if (!owner || (entry && entry->owner && (entry->owner != owner ||
	    strcmp(entry->cookie, request->cookie))))
		return false;
	result->reason = "definition-conflict";
	if (!entry || strcmp(entry->fingerprint, request->fingerprint) ||
	    strcmp(entry->class_name, request->class_name) ||
	    strcmp(entry->instance, request->instance))
	{
		if (entry && (entry->state != SCRATCHPAD_UNMATCHED ||
		    entry->target || entry->lease[0]))
			return false;
		/* Overlapping exact selectors cannot reserve two launches. */
		for (size_t i = 0; i < SCRATCHPAD_MAX; i++)
		{
			const struct scratchpad_entry *other =
				&registry->entries[i];

			if (other != entry && other->name[0] &&
			    strcmp(other->class_name,
				request->class_name) == 0 &&
			    (!other->instance[0] || !request->instance[0] ||
			     strcmp(other->instance, request->instance) == 0))
				return false;
		}
		if (entry)
		{
			if (entry->owner &&
			    strcmp(request->serial, entry->serial) <= 0)
			{
				copy_result(entry, result, "replay");
				return true;
			}
			strcpy(entry->fingerprint, request->fingerprint);
			strcpy(entry->class_name, request->class_name);
			strcpy(entry->instance, request->instance);
		}
	}
	if (!entry)
	{
		result->reason = "table-full";
		for (size_t i = 0; i < SCRATCHPAD_MAX; i++)
			if (!registry->entries[i].name[0])
			{
				entry = &registry->entries[i];
				break;
			}
		if (!entry)
			return false;
		strcpy(entry->name, request->name);
		strcpy(entry->fingerprint, request->fingerprint);
		strcpy(entry->class_name, request->class_name);
		strcpy(entry->instance, request->instance);
		entry->state = registry->restarted ? SCRATCHPAD_UNCERTAIN :
			SCRATCHPAD_UNMATCHED;
	}
	result->reason = "definition-conflict";
	if (strcmp(entry->class_name, request->class_name) ||
	    strcmp(entry->instance, request->instance))
		return false;
	if (entry->owner && strcmp(request->serial, entry->serial) <= 0)
	{
		copy_result(entry, result, "replay");
		return true;
	}
	entry->owner = owner;
	strcpy(entry->cookie, request->cookie);
	strcpy(entry->serial, request->serial);
	if (entry->state == SCRATCHPAD_PENDING && now >= entry->deadline)
		entry->state = SCRATCHPAD_UNCERTAIN;
	was_waiting = entry->lease[0] &&
		(entry->state == SCRATCHPAD_PENDING ||
		 entry->state == SCRATCHPAD_UNCERTAIN);
	if (request->verb == SCRATCHPAD_LAUNCHED ||
	    request->verb == SCRATCHPAD_LAUNCH_FAILED)
	{
		result->reason = "lease-mismatch";
		if (!was_waiting || !entry->lease[0] ||
		    strcmp(request->lease, entry->lease))
			return false;
		if (request->verb == SCRATCHPAD_LAUNCH_FAILED &&
		    observation->matches == 0)
		{
			entry->state = SCRATCHPAD_UNMATCHED;
			entry->lease[0] = '\0';
			entry->desired_visible = false;
			copy_result(entry, result, "launch-failed");
			return true;
		}
	}
	if (observation->matches == 1)
	{
		entry->target = observation->target;
		entry->state = observation->unsupported ?
			SCRATCHPAD_UNSUPPORTED :
			SCRATCHPAD_CLAIMED;
		entry->lease[0] = '\0';
		if (!was_waiting)
			entry->desired_visible = observation->visible;
	}
	else if (observation->matches > 1)
	{
		entry->state = SCRATCHPAD_AMBIGUOUS;
		entry->target = 0;
	}
	else if (entry->state == SCRATCHPAD_CLAIMED ||
	    entry->state == SCRATCHPAD_UNSUPPORTED ||
	    entry->state == SCRATCHPAD_AMBIGUOUS)
	{
		entry->target = 0;
		entry->state = SCRATCHPAD_UNMATCHED;
		entry->desired_visible = false;
	}
	if (request->verb == SCRATCHPAD_TOGGLE)
		entry->desired_visible = !entry->desired_visible;
	else if (request->verb == SCRATCHPAD_RETRY)
		entry->desired_visible = true;
	if (entry->state == SCRATCHPAD_CLAIMED &&
	    (was_waiting || request->verb == SCRATCHPAD_TOGGLE ||
	     request->verb == SCRATCHPAD_RETRY))
		result->action = entry->desired_visible ? SCRATCHPAD_SUMMON :
			SCRATCHPAD_DISMISS;
	else if (entry->state == SCRATCHPAD_UNMATCHED &&
	    (request->verb == SCRATCHPAD_TOGGLE ||
	     request->verb == SCRATCHPAD_RETRY) &&
	    entry->desired_visible)
	{
		result->reason = "allocation-failed";
		if (!new_lease || !hex(new_lease, 48) ||
		    now > UINT64_MAX - SCRATCHPAD_LEASE_MS)
			return false;
		strcpy(entry->lease, new_lease);
		entry->deadline = now + SCRATCHPAD_LEASE_MS;
		entry->state = SCRATCHPAD_PENDING;
		result->action = SCRATCHPAD_LAUNCH;
	}
	/* Only an explicit retry may replace an uncertain reservation. */
	else if (entry->state == SCRATCHPAD_UNCERTAIN &&
	    request->verb == SCRATCHPAD_RETRY && observation->matches == 0)
	{
		result->reason = "allocation-failed";
		if (!new_lease || !hex(new_lease, 48) ||
		    now > UINT64_MAX - SCRATCHPAD_LEASE_MS)
			return false;
		strcpy(entry->lease, new_lease);
		entry->deadline = now + SCRATCHPAD_LEASE_MS;
		entry->state = SCRATCHPAD_PENDING;
		result->action = SCRATCHPAD_LAUNCH;
	}
	copy_result(entry, result,
		entry->state == SCRATCHPAD_CLAIMED ? "matched" :
		entry->state == SCRATCHPAD_PENDING ? "pending" :
		entry->state == SCRATCHPAD_UNCERTAIN ? "uncertain" :
		entry->state == SCRATCHPAD_AMBIGUOUS ? "ambiguous" :
		entry->state == SCRATCHPAD_UNSUPPORTED ? "unsupported" :
		"unmatched");
	return true;
}

bool workspace_center(int area_x, int area_y, int area_width, int area_height,
	int width, int height, int *x, int *y)
{
	int64_t centered_x, centered_y;

	*x = *y = 0;
	if (area_width <= 0 || area_height <= 0 || width <= 0 || height <= 0)
		return false;
	centered_x = (int64_t)area_x + (width < area_width ?
		((int64_t)area_width - width) / 2 : 0);
	centered_y = (int64_t)area_y + (height < area_height ?
		((int64_t)area_height - height) / 2 : 0);
	if (centered_x < INT_MIN || centered_x > INT_MAX ||
	    centered_y < INT_MIN || centered_y > INT_MAX)
		return false;
	*x = (int)centered_x;
	*y = (int)centered_y;
	return true;
}
