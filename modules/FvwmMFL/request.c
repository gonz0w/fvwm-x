/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "request.h"
#include "libs/defaults.h"
#include "libs/module_monitor.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Bound parser recursion before entering cJSON. Its strings cannot represent
 * embedded NUL, so refuse that escape before comparing fields or IDs. */
static bool request_shape(const char *text)
{
	size_t length = strlen(text);
	unsigned int depth = 0;
	bool string = false;

	if (length > MAX_MODULE_INPUT_TEXT_LEN)
		return false;
	for (size_t i = 0; i < length; i++)
	{
		unsigned char ch = text[i];

		if (string && ch == '\\')
		{
			if (i + 5 < length && text[i + 1] == 'u' &&
			    memcmp(text + i + 2, "0000", 4) == 0)
				return false;
			i++;
			continue;
		}
		if (ch == '"')
			string = !string;
		else if (!string && (ch == '{' || ch == '['))
		{
			if (++depth > 4)
				return false;
		}
		else if (!string && (ch == '}' || ch == ']'))
		{
			if (depth == 0)
				return false;
			depth--;
		}
	}
	return !string && depth == 0;
}

static bool request_id(const cJSON *value, char *id)
{
	size_t length;

	if (!cJSON_IsString(value))
		return false;
	length = strlen(value->valuestring);
	if (length == 0 || length > MFL_REQUEST_ID_MAX)
		return false;
	for (size_t i = 0; i < length; i++)
	{
		unsigned char ch = value->valuestring[i];

		if (!((ch >= 'a' && ch <= 'z') ||
		    (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
		    ch == '_' || ch == '-' || ch == '.'))
			return false;
	}
	memcpy(id, value->valuestring, length + 1);
	return true;
}

bool mfl_request_parse(const char *json, struct mfl_request *request)
{
	static const char *fields[] = {"version", "id", "method", "resource",
		"mode"};
	cJSON *root = NULL;
	const cJSON *values[5] = {0};
	const cJSON *item;
	unsigned int seen = 0;
	bool valid = false;

	memset(request, 0, sizeof(*request));
	request->error = "invalid_request";
	if (json == NULL || !request_shape(json))
		return false;
	root = cJSON_ParseWithOpts(json, NULL, true);
	if (!cJSON_IsObject(root))
		goto out;
	for (item = root->child; item != NULL; item = item->next)
	{
		size_t i;

		for (i = 0; i < 5; i++)
			if (strcmp(item->string, fields[i]) == 0)
				break;
		if (i == 5 || (seen & (1U << i)))
			goto out;
		seen |= 1U << i;
		values[i] = item;
	}
	if (!request_id(values[1], request->id))
		goto out;
	if (!cJSON_IsNumber(values[0]) || values[0]->valuedouble != 1)
	{
		request->error = "unsupported_version";
		goto out;
	}
	if (!cJSON_IsString(values[2]))
		goto out;
	if (strcmp(values[2]->valuestring, "query") == 0 && seen == 15 &&
	    cJSON_IsString(values[3]))
	{
		const char *resource = values[3]->valuestring;

		request->error = "unsupported_resource";
		if (strcmp(resource, "capabilities") == 0)
			request->kind = MFL_REQUEST_CAPABILITIES;
		else if (strcmp(resource, "connection") == 0)
			request->kind = MFL_REQUEST_CONNECTION;
		else if (strcmp(resource, "desktop") == 0)
			request->kind = MFL_REQUEST_DESKTOP;
	}
	else if (strcmp(values[2]->valuestring, "barrier") == 0 && seen == 7)
		request->kind = MFL_REQUEST_BARRIER;
	else if (strcmp(values[2]->valuestring, "set_mode") == 0 &&
	    seen == 23 && cJSON_IsString(values[4]))
	{
		request->error = "unsupported_mode";
		if (strcmp(values[4]->valuestring, "read-only") == 0)
			request->kind = MFL_REQUEST_READ_ONLY;
	}
	else if (strcmp(values[2]->valuestring, "query") != 0 &&
	    strcmp(values[2]->valuestring, "barrier") != 0 &&
	    strcmp(values[2]->valuestring, "set_mode") != 0)
		request->error = "unsupported_method";
	valid = request->kind != MFL_REQUEST_INVALID;
	if (valid)
		request->error = NULL;
out:
	cJSON_Delete(root);
	return valid;
}

const char *mfl_request_reserve(struct mfl_pending_request *pending,
	const struct mfl_request *request, uint64_t serial, uint64_t now_ms,
	struct mfl_pending_request **slot)
{
	*slot = NULL;
	if (serial == 0 || (request->kind != MFL_REQUEST_DESKTOP &&
	    request->kind != MFL_REQUEST_BARRIER))
		return "invalid_request";
	for (size_t i = 0; i < MFL_REQUEST_LIMIT; i++)
	{
		if (pending[i].kind != MFL_REQUEST_INVALID &&
		    strcmp(pending[i].id, request->id) == 0)
		{
			*slot = NULL;
			return "duplicate_id";
		}
		if (pending[i].kind == MFL_REQUEST_INVALID && *slot == NULL)
			*slot = pending + i;
	}
	if (*slot == NULL)
		return "too_many_requests";
	**slot = (struct mfl_pending_request){.kind = request->kind,
		.serial = serial, .deadline_ms = now_ms >
		UINT64_MAX - MFL_REQUEST_TIMEOUT_MS ? UINT64_MAX :
		now_ms + MFL_REQUEST_TIMEOUT_MS};
	memcpy((*slot)->id, request->id, sizeof(request->id));
	return NULL;
}

static int request_tag(char *output, size_t capacity,
	const struct mfl_pending_request *pending)
{
	return snprintf(output, capacity, "MFL_RPC_1 %016" PRIx64 " %s",
		pending->serial, pending->kind == MFL_REQUEST_DESKTOP ?
		"desktop " : "barrier");
}

bool mfl_request_command(char *command, size_t capacity,
	const struct mfl_pending_request *pending)
{
	char tag[64];
	int length;

	if (capacity == 0)
		return false;
	command[0] = '\0';
	if (pending->kind != MFL_REQUEST_DESKTOP &&
	    pending->kind != MFL_REQUEST_BARRIER)
		return false;
	request_tag(tag, sizeof(tag), pending);
	length = snprintf(command, capacity, "Send_Reply %s%s", tag,
		pending->kind == MFL_REQUEST_DESKTOP ?
		"$[desk.n] $[vp.x] $[vp.y] $[vp.width] $[vp.height] "
		"$[page.nx] $[page.ny] $[wa.x] $[wa.y] "
		"$[wa.width] $[wa.height]" : "");
	if (length < 0 || (size_t)length >= capacity)
	{
		command[0] = '\0';
		return false;
	}
	return true;
}

const char *mfl_request_payload(const char *message,
	const struct mfl_pending_request *pending)
{
	char tag[64];
	int length;

	if (pending->kind != MFL_REQUEST_DESKTOP &&
	    pending->kind != MFL_REQUEST_BARRIER)
		return NULL;
	length = request_tag(tag, sizeof(tag), pending);
	if (strncmp(message, tag, (size_t)length) != 0)
		return NULL;
	if (pending->kind == MFL_REQUEST_BARRIER && message[length] != '\0')
		return NULL;
	return message + length;
}

cJSON *mfl_request_core_result(enum mfl_request_kind kind, const char *payload)
{
	cJSON *result = cJSON_CreateObject();
	cJSON *viewport, *page, *area;
	int values[11] = {0};
	const char *p = payload;

	if (result == NULL)
		return NULL;
	if (kind == MFL_REQUEST_BARRIER && *payload == '\0')
	{
		if (!cJSON_AddStringToObject(result, "state", "processed") ||
		    !cJSON_AddStringToObject(result, "asynchronous_completion",
			"unknown"))
			goto fail;
		return result;
	}
	if (kind != MFL_REQUEST_DESKTOP)
		goto fail;
	/* Strict token boundaries supplement the shared checked integer parser. */
	for (size_t i = 0; i < 11; i++)
	{
		if (*p == '-')
			p++;
		if (*p < '0' || *p > '9')
			goto fail;
		while (*p >= '0' && *p <= '9')
			p++;
		if (i < 10 && *p++ != ' ')
			goto fail;
	}
	if (*p != '\0' ||
	    !module_parse_decimal_integers(payload, values, 11) ||
	    values[3] <= 0 || values[4] <= 0 ||
	    values[9] < 0 || values[10] < 0)
		goto fail;
	viewport = cJSON_AddObjectToObject(result, "viewport");
	page = cJSON_AddObjectToObject(result, "page");
	area = cJSON_AddObjectToObject(result, "work_area");
	if (!viewport || !page || !area ||
	    !cJSON_AddStringToObject(result, "scope", "core-current-monitor") ||
	    !cJSON_AddStringToObject(result, "units", "actual-pixels") ||
	    !cJSON_AddNumberToObject(result, "desk", values[0]) ||
	    !cJSON_AddNumberToObject(viewport, "x", values[1]) ||
	    !cJSON_AddNumberToObject(viewport, "y", values[2]) ||
	    !cJSON_AddNumberToObject(viewport, "width", values[3]) ||
	    !cJSON_AddNumberToObject(viewport, "height", values[4]) ||
	    !cJSON_AddNumberToObject(page, "x", values[5]) ||
	    !cJSON_AddNumberToObject(page, "y", values[6]) ||
	    !cJSON_AddNumberToObject(area, "x", values[7]) ||
	    !cJSON_AddNumberToObject(area, "y", values[8]) ||
	    !cJSON_AddNumberToObject(area, "width", values[9]) ||
	    !cJSON_AddNumberToObject(area, "height", values[10]))
		goto fail;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

cJSON *mfl_request_reply(const char *id, const char *error, cJSON *result)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *reply;

	if (root == NULL)
		goto fail;
	reply = cJSON_AddObjectToObject(root, "reply");
	if (!reply || !cJSON_AddNumberToObject(reply, "version", 1) ||
	    !(id ? cJSON_AddStringToObject(reply, "id", id) :
		cJSON_AddNullToObject(reply, "id")) ||
	    !cJSON_AddStringToObject(reply, "status", error ? "error" : "ok"))
		goto fail;
	if (error)
	{
		cJSON *details = cJSON_AddObjectToObject(reply, "error");

		if (!details || !cJSON_AddStringToObject(details, "code", error))
			goto fail;
		cJSON_Delete(result);
	}
	else if (!cJSON_AddItemToObject(reply, "result", result))
		goto fail;
	return root;
fail:
	cJSON_Delete(result);
	cJSON_Delete(root);
	return NULL;
}
