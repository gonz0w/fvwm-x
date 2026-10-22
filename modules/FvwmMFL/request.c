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
static bool request_shape(const char *text, size_t limit,
	unsigned int max_depth)
{
	size_t length = strlen(text);
	unsigned int depth = 0;
	bool string = false;

	if (length > limit)
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
			if (++depth > max_depth)
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

static bool bounded_string(const cJSON *value, size_t maximum)
{
	return cJSON_IsString(value) && value->valuestring[0] != '\0' &&
		strlen(value->valuestring) <= maximum;
}

static bool settings_token(const cJSON *value)
{
	if (!bounded_string(value, 32) || strlen(value->valuestring) != 32)
		return false;
	return strspn(value->valuestring, "0123456789abcdef") == 32;
}

static bool settings_values(const cJSON *value)
{
	const cJSON *item, *previous;
	unsigned int count = 0;

	if (!cJSON_IsObject(value))
		return false;
	for (item = value->child; item; item = item->next)
	{
		size_t length = strlen(item->string);

		if (++count > 7 || length == 0 || length > 32 ||
		    strspn(item->string, "abcdefghijklmnopqrstuvwxyz_") != length ||
		    !(cJSON_IsBool(item) || bounded_string(item, 64)))
			return false;
		for (previous = value->child; previous != item;
		    previous = previous->next)
			if (strcmp(previous->string, item->string) == 0)
				return false;
	}
	return count != 0;
}

static bool settings_request(struct mfl_request *request,
	const cJSON *const *values, unsigned int seen)
{
	enum {SCOPE = 1U << 5, OPERATION = 1U << 6,
		GENERATION = 1U << 7, VALUES = 1U << 8, PROVIDER = 1U << 9};
	const unsigned int action_fields = 15 | OPERATION | GENERATION | PROVIDER;
	cJSON *payload = cJSON_CreateObject();
	cJSON *copy = NULL;
	char *text = NULL;
	const char *operation = NULL;
	bool valid = false;

	if (!payload)
		return false;
	if (strcmp(values[2]->valuestring, "query") == 0 &&
	    (seen & ~(SCOPE | PROVIDER)) == 15 &&
	    (!values[5] || bounded_string(values[5], 64)) &&
	    (!values[9] || settings_token(values[9])))
	{
		request->kind = MFL_REQUEST_SETTINGS_QUERY;
		operation = "query";
	}
	else if (strcmp(values[2]->valuestring, "validate") == 0 &&
	    seen == (15 | GENERATION | PROVIDER | VALUES) &&
	    settings_token(values[7]) && settings_token(values[9]) &&
	    settings_values(values[8]))
	{
		request->kind = MFL_REQUEST_SETTINGS_VALIDATE;
		operation = "validate";
	}
	else if (strcmp(values[2]->valuestring, "action") == 0 &&
	    cJSON_IsString(values[6]) && settings_token(values[7]) &&
	    settings_token(values[9]))
	{
		operation = values[6]->valuestring;
		if (strcmp(operation, "set") == 0 &&
		    seen == (action_fields | VALUES) &&
		    settings_values(values[8]))
			request->kind = MFL_REQUEST_SETTINGS_SET;
		else if (strcmp(operation, "reset") == 0 &&
		    seen == (action_fields | SCOPE) &&
		    bounded_string(values[5], 64))
			request->kind = MFL_REQUEST_SETTINGS_RESET;
		else if (strcmp(operation, "reload") == 0 && seen == action_fields)
			request->kind = MFL_REQUEST_SETTINGS_RELOAD;
	}
	if (!mfl_request_is_settings(request->kind) ||
	    !cJSON_AddStringToObject(payload, "operation", operation))
		goto out;
	if (request->kind == MFL_REQUEST_SETTINGS_QUERY ||
	    request->kind == MFL_REQUEST_SETTINGS_RESET)
		if (!cJSON_AddStringToObject(payload, "scope",
			values[5] ? values[5]->valuestring : "all"))
			goto out;
	if ((mfl_request_is_action(request->kind) ||
	    request->kind == MFL_REQUEST_SETTINGS_VALIDATE) &&
	    !cJSON_AddStringToObject(payload, "generation", values[7]->valuestring))
		goto out;
	if (values[9] &&
	    !cJSON_AddStringToObject(payload, "provider", values[9]->valuestring))
		goto out;
	if (request->kind == MFL_REQUEST_SETTINGS_SET ||
	    request->kind == MFL_REQUEST_SETTINGS_VALIDATE)
	{
		copy = cJSON_Duplicate(values[8], true);
		if (!copy || !cJSON_AddItemToObject(payload, "values", copy))
		{
			cJSON_Delete(copy);
			goto out;
		}
	}
	text = cJSON_PrintUnformatted(payload);
	if (!text)
		goto out;
	if (strlen(text) > MFL_SETTINGS_INPUT_MAX)
	{
		request->error = "request_too_large";
		goto out;
	}
	strcpy(request->settings_payload, text);
	valid = true;
out:
	cJSON_free(text);
	cJSON_Delete(payload);
	if (!valid)
		request->kind = MFL_REQUEST_INVALID;
	return valid;
}

bool mfl_request_parse(const char *json, struct mfl_request *request)
{
	static const char *fields[] = {"version", "id", "method", "resource",
		"mode", "scope", "operation", "generation", "values",
		"provider", "target"};
	cJSON *root = NULL;
	const cJSON *values[11] = {0};
	const cJSON *item;
	unsigned int seen = 0;
	bool valid = false;

	memset(request, 0, sizeof(*request));
	request->error = "invalid_request";
	if (json == NULL || !request_shape(json, MAX_MODULE_INPUT_TEXT_LEN, 4))
		return false;
	root = cJSON_ParseWithOpts(json, NULL, true);
	if (!cJSON_IsObject(root))
		goto out;
	for (item = root->child; item != NULL; item = item->next)
	{
		size_t i;

		for (i = 0; i < 11; i++)
			if (strcmp(item->string, fields[i]) == 0)
				break;
		if (i == 11 || (seen & (1U << i)))
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
	if (cJSON_IsString(values[3]) &&
	    strcmp(values[3]->valuestring, "settings") == 0)
	{
		valid = settings_request(request, values, seen);
		if (valid)
			request->error = NULL;
		goto out;
	}
	if (strcmp(values[2]->valuestring, "query") == 0 && seen == 15 &&
	    cJSON_IsString(values[3]))
	{
		const char *resource = values[3]->valuestring;

		request->error = "unsupported_resource";
		if (strcmp(resource, "capabilities") == 0)
			request->kind = MFL_REQUEST_CAPABILITIES;
		else if (strcmp(resource, "connection") == 0)
			request->kind = MFL_REQUEST_CONNECTION;
		else if (strcmp(resource, "pending") == 0)
			request->kind = MFL_REQUEST_PENDING;
		else if (strcmp(resource, "desktop") == 0)
			request->kind = MFL_REQUEST_DESKTOP;
	}
	else if (strcmp(values[2]->valuestring, "cancel") == 0 &&
	    seen == (7 | (1U << 10)) &&
	    request_id(values[10], request->target))
		request->kind = MFL_REQUEST_CANCEL;
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
	    strcmp(values[2]->valuestring, "set_mode") != 0 &&
	    strcmp(values[2]->valuestring, "action") != 0 &&
	    strcmp(values[2]->valuestring, "cancel") != 0 &&
	    strcmp(values[2]->valuestring, "validate") != 0)
		request->error = "unsupported_method";
	valid = request->kind != MFL_REQUEST_INVALID;
	if (valid)
		request->error = NULL;
out:
	cJSON_Delete(root);
	return valid;
}

struct mfl_pending_request *mfl_request_find(
	struct mfl_pending_request *pending, const char *id)
{
	for (size_t i = 0; i < MFL_REQUEST_LIMIT; i++)
		if (pending[i].kind != MFL_REQUEST_INVALID &&
		    strcmp(pending[i].id, id) == 0)
			return pending + i;
	return NULL;
}

cJSON *mfl_request_pending_result(const struct mfl_pending_request *pending,
	uint64_t now_ms)
{
	cJSON *result = cJSON_CreateObject();
	cJSON *list;

	if (!result)
		return NULL;
	list = cJSON_AddArrayToObject(result, "requests");
	if (!list || !cJSON_AddStringToObject(result, "scope", "connection") ||
	    !cJSON_AddStringToObject(result, "execution", "unknown"))
		goto fail;
	for (size_t i = 0; i < MFL_REQUEST_LIMIT; i++)
	{
		const struct mfl_pending_request *p = pending + i;
		cJSON *item;
		const char *operation = "query";

		if (p->kind == MFL_REQUEST_INVALID)
			continue;
		item = cJSON_CreateObject();
		if (!item)
			goto fail;
		if (!cJSON_AddItemToArray(list, item))
		{
			cJSON_Delete(item);
			goto fail;
		}
		switch (p->kind)
		{
		case MFL_REQUEST_SETTINGS_SET:
			operation = "set";
			break;
		case MFL_REQUEST_SETTINGS_RESET:
			operation = "reset";
			break;
		case MFL_REQUEST_SETTINGS_RELOAD:
			operation = "reload";
			break;
		case MFL_REQUEST_SETTINGS_VALIDATE:
			operation = "validate";
			break;
		case MFL_REQUEST_BARRIER:
			operation = "barrier";
			break;
		default: break;
		}
		if (!cJSON_AddStringToObject(item, "id", p->id) ||
		    !cJSON_AddStringToObject(item, "operation", operation) ||
		    !cJSON_AddStringToObject(item, "resource",
			mfl_request_is_settings(p->kind) ? "settings" :
			p->kind == MFL_REQUEST_DESKTOP ?
			"desktop" : "core-stream") ||
		    !cJSON_AddStringToObject(item, "state",
			now_ms >= p->deadline_ms ? "deadline-reached" :
			p->settings_parts ?
			"receiving-reply" : "awaiting-reply") ||
		    !cJSON_AddNumberToObject(item, "remaining_ms",
			now_ms >= p->deadline_ms ? 0 : p->deadline_ms - now_ms))
			goto fail;
	}
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}

const char *mfl_request_reserve(struct mfl_pending_request *pending,
	const struct mfl_request *request, uint64_t serial, uint64_t now_ms,
	struct mfl_pending_request **slot)
{
	*slot = NULL;
	if (serial == 0 || (request->kind != MFL_REQUEST_DESKTOP &&
	    request->kind != MFL_REQUEST_BARRIER &&
	    !mfl_request_is_settings(request->kind)))
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

bool mfl_request_is_settings(enum mfl_request_kind kind)
{
	return kind >= MFL_REQUEST_SETTINGS_QUERY &&
		kind <= MFL_REQUEST_SETTINGS_VALIDATE;
}

bool mfl_request_is_action(enum mfl_request_kind kind)
{
	return kind >= MFL_REQUEST_SETTINGS_SET &&
		kind <= MFL_REQUEST_SETTINGS_RELOAD;
}

void mfl_request_clear(struct mfl_pending_request *pending)
{
	cJSON_free(pending->settings_result);
	memset(pending, 0, sizeof(*pending));
}

static bool settings_cookie(const char *cookie)
{
	size_t length = strlen(cookie);

	return length > 0 && length <= 64 &&
		strspn(cookie, "0123456789abcdef-") == length;
}

bool mfl_request_settings_command(char *output, size_t capacity,
	const struct mfl_request *request, uint64_t serial, const char *cookie)
{
	static const char hex[] = "0123456789abcdef";
	char encoded[MFL_SETTINGS_INPUT_MAX * 2 + 1];
	size_t length = strlen(request->settings_payload);
	int written;

	if (capacity == 0)
		return false;
	output[0] = '\0';
	if (!mfl_request_is_settings(request->kind) || !settings_cookie(cookie) ||
	    serial == 0 || length == 0 || length > MFL_SETTINGS_INPUT_MAX)
		return false;
	for (size_t i = 0; i < length; i++)
	{
		unsigned char value = request->settings_payload[i];

		encoded[2 * i] = hex[value >> 4];
		encoded[2 * i + 1] = hex[value & 15];
	}
	encoded[2 * length] = '\0';
	written = snprintf(output, capacity,
		"SendToModule FvwmDesktop ipc-request %s %016" PRIx64 " %s",
		cookie, serial, encoded);
	if (written < 0 || (size_t)written >= capacity)
	{
		output[0] = '\0';
		return false;
	}
	return true;
}

const char *mfl_request_settings_payload(const char *message,
	const struct mfl_pending_request *pending, const char *cookie)
{
	char prefix[128];
	int length;

	if (!mfl_request_is_settings(pending->kind) || !settings_cookie(cookie))
		return NULL;
	length = snprintf(prefix, sizeof(prefix), "MFL_SETTINGS_1 %s %016"
		PRIx64 " ", cookie, pending->serial);
	if (length < 0 || (size_t)length >= sizeof(prefix) ||
	    strncmp(message, prefix, (size_t)length) != 0)
		return NULL;
	return message + length;
}

static int settings_hex(unsigned char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	return -1;
}

enum mfl_settings_part_result mfl_request_settings_part(
	struct mfl_pending_request *pending, const char *payload)
{
	bool end = strncmp(payload, "end ", 4) == 0;
	const char *p;
	unsigned int index = 0, digits = 0;
	size_t length;

	if (!mfl_request_is_settings(pending->kind) ||
	    (!end && strncmp(payload, "part ", 5) != 0) ||
	    pending->settings_length > MFL_SETTINGS_RESULT_MAX)
		return MFL_SETTINGS_INVALID;
	p = payload + (end ? 4 : 5);
	while (*p >= '0' && *p <= '9')
	{
		if (++digits > 2)
			return MFL_SETTINGS_INVALID;
		index = index * 10 + (unsigned int)(*p++ - '0');
	}
	if (digits == 0 || index != pending->settings_parts)
		return MFL_SETTINGS_INVALID;
	if (end)
		return *p == '\0' && pending->settings_length != 0 ?
			MFL_SETTINGS_DONE : MFL_SETTINGS_INVALID;
	if (*p++ != ' ' || pending->settings_parts >=
	    MFL_SETTINGS_RESULT_MAX / MFL_SETTINGS_CHUNK)
		return MFL_SETTINGS_INVALID;
	length = strlen(p);
	if (length == 0 || length % 2 || length > 2 * MFL_SETTINGS_CHUNK ||
	    length / 2 > MFL_SETTINGS_RESULT_MAX - pending->settings_length)
		return MFL_SETTINGS_INVALID;
	/* Validate before appending; decoded NUL would truncate the JSON parser. */
	for (size_t i = 0; i < length; i += 2)
	{
		int high = settings_hex(p[i]), low = settings_hex(p[i + 1]);

		if (high < 0 || low < 0 || (high == 0 && low == 0))
			return MFL_SETTINGS_INVALID;
	}
	if (!pending->settings_result)
	{
		pending->settings_result = cJSON_malloc(MFL_SETTINGS_RESULT_MAX + 1);
		if (!pending->settings_result)
			return MFL_SETTINGS_INVALID;
	}
	for (size_t i = 0; i < length; i += 2)
		pending->settings_result[pending->settings_length++] =
			(char)((settings_hex(p[i]) << 4) | settings_hex(p[i + 1]));
	pending->settings_result[pending->settings_length] = '\0';
	pending->settings_parts++;
	return MFL_SETTINGS_MORE;
}

cJSON *mfl_request_settings_result(const struct mfl_pending_request *pending)
{
	cJSON *root;
	const cJSON *value;

	if (!pending->settings_result ||
	    !request_shape(pending->settings_result, MFL_SETTINGS_RESULT_MAX, 8))
		return NULL;
	root = cJSON_ParseWithOpts(pending->settings_result, NULL, true);
	if (!cJSON_IsObject(root) || cJSON_GetArraySize(root) != 1)
		goto fail;
	value = cJSON_GetObjectItemCaseSensitive(root, "result");
	if (cJSON_IsObject(value))
		return root;
	value = cJSON_GetObjectItemCaseSensitive(root, "error");
	if (cJSON_IsObject(value) && cJSON_GetArraySize(value) == 1)
	{
		const cJSON *code = cJSON_GetObjectItemCaseSensitive(value, "code");

		if (bounded_string(code, 64) && strspn(code->valuestring,
		    "abcdefghijklmnopqrstuvwxyz_") == strlen(code->valuestring))
			return root;
	}
fail:
	cJSON_Delete(root);
	return NULL;
}
