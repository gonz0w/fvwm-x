/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmMFL/request.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void test_schema(void)
{
	struct mfl_request request;
	const char *valid[] = {
		"{\"version\":1,\"id\":\"A-1_.\",\"method\":\"query\","
		"\"resource\":\"capabilities\"}",
		"{\"version\":1,\"id\":\"A-1_.\",\"method\":\"query\","
		"\"resource\":\"connection\"}",
		"{\"version\":1,\"id\":\"A-1_.\",\"method\":\"set_mode\","
		"\"mode\":\"read-only\"}",
		"{\"version\":1,\"id\":\"A-1_.\",\"method\":\"query\","
		"\"resource\":\"desktop\"}",
		"{\"version\":1,\"id\":\"A-1_.\",\"method\":\"barrier\"}"
	};

	for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
	{
		assert(mfl_request_parse(valid[i], &request));
		assert(request.kind == (enum mfl_request_kind)(i + 1));
		assert(strcmp(request.id, "A-1_.") == 0 && request.error == NULL);
	}
	const char *invalid[] = {
		"", "{}", "[]", "null", "{", "{} false",
		"{\"version\":1,\"id\":1,\"method\":\"barrier\"}",
		"{\"version\":1,\"id\":\"\",\"method\":\"barrier\"}",
		"{\"version\":1,\"id\":\"a b\",\"method\":\"barrier\"}",
		"{\"version\":1,\"id\":\"a\\n\",\"method\":\"barrier\"}",
		"{\"version\":1,\"id\":\"a\\u0000b\",\"method\":\"barrier\"}",
		"{\"version\":1,\"id\\u0000x\":\"a\",\"method\":\"barrier\"}",
		"{\"version\":1,\"id\":\"a\",\"id\":\"b\",\"method\":\"barrier\"}",
		"{\"version\":1,\"id\":\"a\",\"method\":\"barrier\",\"extra\":0}",
		"{\"version\":1,\"id\":\"a\",\"method\":\"barrier\",\"mode\":\"read-only\"}",
		"{\"version\":1,\"id\":\"a\",\"method\":\"query\"}",
		"{\"version\":1,\"id\":\"a\",\"method\":\"query\",\"resource\":[]}",
		"{\"version\":1,\"id\":\"a\",\"method\":\"set_mode\"}",
		"{\"version\":1,\"id\":\"a\",\"method\":\"set_mode\",\"mode\":false}",
		"{\"version\":1,\"id\":\"a\",\"method\":[[[[[]]]]]}"
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(!mfl_request_parse(invalid[i], &request));
		assert(request.kind == MFL_REQUEST_INVALID && request.error);
	}
	assert(!mfl_request_parse(NULL, &request));
	assert(request.id[0] == '\0');
	assert(!mfl_request_parse("{\"version\":true,\"id\":\"a\","
		"\"method\":\"barrier\"}", &request));
	assert(strcmp(request.id, "a") == 0);
	assert(strcmp(request.error, "unsupported_version") == 0);
	assert(!mfl_request_parse("{\"version\":1,\"id\":\"a\","
		"\"method\":\"execute\"}", &request));
	assert(strcmp(request.error, "unsupported_method") == 0);
	assert(!mfl_request_parse("{\"version\":1,\"id\":\"a\","
		"\"method\":\"query\",\"resource\":\"\\\\u0000\"}", &request));
	assert(strcmp(request.error, "unsupported_resource") == 0);
	assert(!mfl_request_parse("{\"version\":1,\"id\":\"a\","
		"\"method\":\"set_mode\",\"mode\":\"read-write\"}", &request));
	assert(strcmp(request.error, "unsupported_mode") == 0);
}

static void test_id_bounds(void)
{
	struct mfl_request request;
	char id[MFL_REQUEST_ID_MAX + 2], text[1100];

	memset(id, 'a', MFL_REQUEST_ID_MAX + 1);
	id[MFL_REQUEST_ID_MAX] = '\0';
	snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"%s\","
		"\"method\":\"barrier\"}", id);
	assert(mfl_request_parse(text, &request));
	assert(strlen(request.id) == MFL_REQUEST_ID_MAX);
	id[MFL_REQUEST_ID_MAX] = 'a';
	id[MFL_REQUEST_ID_MAX + 1] = '\0';
	snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"%s\","
		"\"method\":\"barrier\"}", id);
	assert(!mfl_request_parse(text, &request));
	assert(request.id[0] == '\0');
	memset(text, ' ', sizeof(text));
	text[sizeof(text) - 1] = '\0';
	assert(!mfl_request_parse(text, &request));
}

static void test_pending_bounds_and_reuse(void)
{
	struct mfl_pending_request pending[MFL_REQUEST_LIMIT] = {0}, *slot;
	struct mfl_request request = {.kind = MFL_REQUEST_DESKTOP};

	for (size_t i = 0; i < MFL_REQUEST_LIMIT; i++)
	{
		snprintf(request.id, sizeof(request.id), "r%zu", i);
		assert(mfl_request_reserve(pending, &request, i + 1,
			100 + i, &slot) == NULL);
		assert(slot == pending + i && slot->serial == i + 1);
		assert(slot->deadline_ms == 5100 + i);
	}
	strcpy(request.id, "full");
	assert(strcmp(mfl_request_reserve(pending, &request, 50, 100,
		&slot), "too_many_requests") == 0 && slot == NULL);
	strcpy(request.id, "r1");
	assert(strcmp(mfl_request_reserve(pending, &request, 50, 100,
		&slot), "duplicate_id") == 0 && slot == NULL);
	pending[0].kind = MFL_REQUEST_INVALID;
	assert(strcmp(mfl_request_reserve(pending, &request, 50, 100,
		&slot), "duplicate_id") == 0 && slot == NULL);
	strcpy(request.id, "new");
	assert(mfl_request_reserve(pending, &request, 50, UINT64_MAX - 1,
		&slot) == NULL);
	assert(slot == pending && slot->serial == 50);
	assert(slot->deadline_ms == UINT64_MAX);
}

static void test_private_core_tags(void)
{
	struct mfl_pending_request pending = {.kind = MFL_REQUEST_BARRIER,
		.serial = 17};
	char command[1001], message[1001];
	const char *payload;

	assert(mfl_request_command(command, sizeof(command), &pending));
	assert(strcmp(command, "Send_Reply MFL_RPC_1 0000000000000011 barrier") == 0);
	assert(!mfl_request_command(command, 4, &pending) && command[0] == '\0');
	assert(!mfl_request_command(command, 0, &pending));
	payload = mfl_request_payload("MFL_RPC_1 0000000000000011 barrier", &pending);
	assert(payload && *payload == '\0');
	assert(!mfl_request_payload("MFL_RPC_1 0000000000000010 barrier", &pending));
	assert(!mfl_request_payload("MFL_RPC_1 0000000000000011 barrier extra", &pending));
	pending.kind = MFL_REQUEST_DESKTOP;
	assert(mfl_request_command(command, sizeof(command), &pending));
	assert(strstr(command, "$[desk.n]") && strstr(command, "$[wa.height]"));
	snprintf(message, sizeof(message), "MFL_RPC_1 0000000000000011 desktop %s",
		"2 0 0 800 600 0 0 0 30 800 570");
	payload = mfl_request_payload(message, &pending);
	assert(payload && strcmp(payload, "2 0 0 800 600 0 0 0 30 800 570") == 0);
	pending.serial++;
	assert(!mfl_request_payload(message, &pending));
	pending.kind = MFL_REQUEST_CONNECTION;
	assert(!mfl_request_command(command, sizeof(command), &pending));
}

static void test_core_results_and_reply_ownership(void)
{
	cJSON *result, *reply;

	result = mfl_request_core_result(MFL_REQUEST_DESKTOP,
		"-2 800 0 800 600 1 0 -800 30 800 570");
	assert(result);
	assert(cJSON_GetObjectItemCaseSensitive(result, "desk")->valueint == -2);
	assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(
		result, "work_area"), "x")->valueint == -800);
	reply = mfl_request_reply("owned", NULL, result);
	assert(reply && cJSON_GetObjectItemCaseSensitive(
		cJSON_GetObjectItemCaseSensitive(reply, "reply"), "result") == result);
	cJSON_Delete(reply);
	result = mfl_request_core_result(MFL_REQUEST_BARRIER, "");
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(result,
		"state")->valuestring, "processed") == 0);
	reply = mfl_request_reply(NULL, "read_only", result);
	assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(
		cJSON_GetObjectItemCaseSensitive(reply, "reply"), "id")));
	cJSON_Delete(reply);
	const char *invalid[] = {"", "1", "1 0 0 800 600 0 0 0 0 800 600 extra",
		"1 0 0 800 600 0 0 0 0 800 600 ",
		"1+0 0 800 600 0 0 0 0 800 600",
		"1 0 0 0 600 0 0 0 0 800 600",
		"1 0 0 800 -600 0 0 0 0 800 600",
		"1 0 0 800 600 0 0 0 0 -1 600",
		"2147483648 0 0 800 600 0 0 0 0 800 600",
		"9999999999999999999999999 0 0 800 600 0 0 0 0 800 600",
		"1 0 0 800 600 0 0 0 0 800",
		"1 0 0 800 600 0 0 0 0 800 600\n"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		assert(!mfl_request_core_result(MFL_REQUEST_DESKTOP, invalid[i]));
	assert(!mfl_request_core_result(MFL_REQUEST_BARRIER, "extra"));
	assert(!mfl_request_core_result(MFL_REQUEST_CONNECTION, ""));
}

static void settings_part(struct mfl_pending_request *pending,
	unsigned int index, const char *data)
{
	static const char hex[] = "0123456789abcdef";
	char text[2 * MFL_SETTINGS_CHUNK + 32];
	int prefix = snprintf(text, sizeof(text), "part %u ", index);
	size_t length = strlen(data);

	assert(length <= MFL_SETTINGS_CHUNK);
	for (size_t i = 0; i < length; i++)
	{
		unsigned char byte = data[i];

		text[prefix + 2 * i] = hex[byte >> 4];
		text[prefix + 2 * i + 1] = hex[byte & 15];
	}
	text[prefix + 2 * length] = '\0';
	assert(mfl_request_settings_part(pending, text) == MFL_SETTINGS_MORE);
}

static void test_settings_requests(void)
{
	struct mfl_request request;
	char text[1001], command[1001];
	const char *generation = "0123456789abcdef0123456789abcdef";
	const char *invalid[] = {
		"\"method\":\"query\",\"resource\":\"settings\",\"scope\":false",
		"\"method\":\"action\",\"resource\":\"settings\",\"operation\":\"set\"",
		("\"method\":\"action\",\"resource\":\"settings\",\"operation\":\"reset\","
		"\"generation\":\"bad\",\"scope\":\"all\""),
		"\"method\":\"query\",\"resource\":\"settings\",\"values\":{}"
	};

	assert(mfl_request_parse("{\"version\":1,\"id\":\"s\",\"method\":\"query\","
		"\"resource\":\"settings\"}", &request));
	assert(request.kind == MFL_REQUEST_SETTINGS_QUERY);
	assert(strcmp(request.settings_payload,
		"{\"operation\":\"query\",\"scope\":\"all\"}") == 0);
	assert(mfl_request_settings_command(command, sizeof(command), &request,
		1, "1-ab-cd"));
	const char *prefix = "SendToModule FvwmDesktop ipc-request 1-ab-cd ";

	assert(strncmp(command, prefix, strlen(prefix)) == 0);
	assert(!mfl_request_settings_command(command, 4, &request, 1, "a"));
	assert(command[0] == '\0');
	assert(!mfl_request_settings_command(command, sizeof(command), &request,
		1, "a;Exec evil"));
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"s\",%s}", invalid[i]);
		assert(!mfl_request_parse(text, &request));
		assert(request.kind == MFL_REQUEST_INVALID);
	}
	const char *values[] = {"{}", "{\"motion\":1}",
		"{\"motion\":true,\"motion\":false}", "{\"palette\":{}}"};
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
	{
		snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"s\","
			"\"method\":\"action\",\"resource\":\"settings\","
			"\"operation\":\"set\",\"generation\":\"%s\","
			"\"provider\":\"%s\",\"values\":%s}",
			generation, generation, values[i]);
		assert(!mfl_request_parse(text, &request));
	}
	snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"s\","
		"\"method\":\"action\",\"resource\":\"settings\",\"operation\":\"set\","
		"\"generation\":\"%s\",\"provider\":\"%s\","
		"\"values\":{\"palette\":\"$[w.id]; Exec evil\"}}",
		generation, generation);
	assert(mfl_request_parse(text, &request));
	assert(mfl_request_is_action(request.kind));
	assert(mfl_request_settings_command(command, sizeof(command), &request,
		1, "a"));
	assert(!strstr(command, "$") && !strstr(command, ";"));
	cJSON *root = cJSON_Parse(text);
	cJSON *large_values = cJSON_CreateObject();
	char large[65];
	const char *keys[] = {"palette", "appearance", "day", "night",
		"transparency", "motion", "keep_wallpaper"};

	memset(large, 'a', sizeof(large) - 1);
	large[sizeof(large) - 1] = '\0';
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
		assert(cJSON_AddStringToObject(large_values, keys[i], large));
	assert(cJSON_ReplaceItemInObjectCaseSensitive(root, "values", large_values));
	char *oversized = cJSON_PrintUnformatted(root);

	assert(strlen(oversized) < sizeof(text));
	assert(!mfl_request_parse(oversized, &request));
	assert(strcmp(request.error, "request_too_large") == 0);
	cJSON_free(oversized);
	cJSON_Delete(root);
}

static void test_settings_assembly_and_cleanup(void)
{
	struct mfl_pending_request pending = {.kind = MFL_REQUEST_SETTINGS_QUERY,
		.serial = 1};
	cJSON *result;
	const char *invalid[] = {"", "end 0", "part 1 7b", "part 0 ",
		"part 0 0", "part 0 gg", "part 0 00", "part 000 7b"};

	assert(mfl_request_settings_payload(
		"MFL_SETTINGS_1 a 0000000000000001 part 0 7b", &pending, "a"));
	assert(!mfl_request_settings_payload(
		"MFL_SETTINGS_1 b 0000000000000001 part 0 7b", &pending, "a"));
	assert(!mfl_request_settings_payload("MFL_SETTINGS_1 a 0000000000000002 end 1",
		&pending, "a"));
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		assert(mfl_request_settings_part(&pending, invalid[i]) ==
			MFL_SETTINGS_INVALID);
	settings_part(&pending, 0, "{\"result\":{");
	assert(mfl_request_settings_part(&pending, "part 0 7b") ==
		MFL_SETTINGS_INVALID);
	settings_part(&pending, 1, "\"owner\":\"FvwmDesktop\"}}");
	assert(mfl_request_settings_part(&pending, "end 1") == MFL_SETTINGS_INVALID);
	assert(mfl_request_settings_part(&pending, "end 2") == MFL_SETTINGS_DONE);
	result = mfl_request_settings_result(&pending);
	assert(result);
	cJSON_Delete(result);
	mfl_request_clear(&pending);
	assert(!pending.settings_result && !pending.settings_length &&
		pending.kind == MFL_REQUEST_INVALID);
	const char *bad_json[] = {"{\"result\":{},\"result\":{}}",
		"{\"error\":{\"code\":\"bad;Exec\"}}", "{\"result\":null}",
		"{\"result\":{\"a\":\"\\u0000\"}}"};
	for (size_t i = 0; i < sizeof(bad_json) / sizeof(bad_json[0]); i++)
	{
		pending.kind = MFL_REQUEST_SETTINGS_SET;
		settings_part(&pending, 0, bad_json[i]);
		assert(!mfl_request_settings_result(&pending));
		mfl_request_clear(&pending);
	}
	pending.kind = MFL_REQUEST_SETTINGS_QUERY;
	char chunk[MFL_SETTINGS_CHUNK + 1];
	memset(chunk, 'x', sizeof(chunk) - 1);
	chunk[sizeof(chunk) - 1] = '\0';
	for (unsigned int i = 0; i < MFL_SETTINGS_RESULT_MAX / MFL_SETTINGS_CHUNK; i++)
		settings_part(&pending, i, chunk);
	assert(pending.settings_length == MFL_SETTINGS_RESULT_MAX);
	assert(mfl_request_settings_part(&pending, "part 32 78") ==
		MFL_SETTINGS_INVALID);
	mfl_request_clear(&pending);
	mfl_request_clear(&pending);
}

int main(void)
{
	test_schema();
	test_id_bounds();
	test_pending_bounds_and_reuse();
	test_private_core_tags();
	test_core_results_and_reply_ownership();
	test_settings_requests();
	test_settings_assembly_and_cleanup();
	return 0;
}
