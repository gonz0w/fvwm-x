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
#include <stdlib.h>
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
	assert(mfl_request_part(pending, text) == MFL_PART_MORE);
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

	assert(mfl_request_part_payload(
		"MFL_SETTINGS_1 a 0000000000000001 part 0 7b", &pending, "a"));
	assert(!mfl_request_part_payload(
		"MFL_SETTINGS_1 b 0000000000000001 part 0 7b", &pending, "a"));
	assert(!mfl_request_part_payload(
		"MFL_SETTINGS_1 a 0000000000000002 end 1",
		&pending, "a"));
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		assert(mfl_request_part(&pending, invalid[i]) ==
			MFL_PART_INVALID);
	settings_part(&pending, 0, "{\"result\":{");
	assert(mfl_request_part(&pending, "part 0 7b") ==
		MFL_PART_INVALID);
	settings_part(&pending, 1, "\"owner\":\"FvwmDesktop\"}}");
	assert(mfl_request_part(&pending, "end 1") == MFL_PART_INVALID);
	assert(mfl_request_part(&pending, "end 2") == MFL_PART_DONE);
	result = mfl_request_result(&pending);
	assert(result);
	cJSON_Delete(result);
	mfl_request_clear(&pending);
	assert(!pending.reply_data && !pending.reply_length &&
		pending.kind == MFL_REQUEST_INVALID);
	const char *bad_json[] = {"{\"result\":{},\"result\":{}}",
		"{\"error\":{\"code\":\"bad;Exec\"}}", "{\"result\":null}",
		"{\"result\":{\"a\":\"\\u0000\"}}"};
	for (size_t i = 0; i < sizeof(bad_json) / sizeof(bad_json[0]); i++)
	{
		pending.kind = MFL_REQUEST_SETTINGS_SET;
		settings_part(&pending, 0, bad_json[i]);
		assert(!mfl_request_result(&pending));
		mfl_request_clear(&pending);
	}
	pending.kind = MFL_REQUEST_SETTINGS_QUERY;
	char chunk[MFL_SETTINGS_CHUNK + 1];
	memset(chunk, 'x', sizeof(chunk) - 1);
	chunk[sizeof(chunk) - 1] = '\0';
	for (unsigned int i = 0; i < MFL_SETTINGS_RESULT_MAX / MFL_SETTINGS_CHUNK; i++)
		settings_part(&pending, i, chunk);
	assert(pending.reply_length == MFL_SETTINGS_RESULT_MAX);
	assert(mfl_request_part(&pending, "part 32 78") ==
		MFL_PART_INVALID);
	mfl_request_clear(&pending);
	mfl_request_clear(&pending);
}

static void test_lifecycle(void)
{
	struct mfl_request request;
	struct mfl_pending_request pending[MFL_REQUEST_LIMIT] = {0}, *slot;
	cJSON *result, *list, *item;
	char text[1001], command[1001];
	const char *token = "0123456789abcdef0123456789abcdef";

	assert(mfl_request_parse("{\"version\":1,\"id\":\"p\","
		"\"method\":\"query\",\"resource\":\"pending\"}", &request));
	assert(request.kind == MFL_REQUEST_PENDING);
	assert(mfl_request_parse("{\"version\":1,\"id\":\"c\","
		"\"method\":\"cancel\",\"target\":\"original-1\"}", &request));
	assert(request.kind == MFL_REQUEST_CANCEL);
	assert(strcmp(request.target, "original-1") == 0);
	const char *bad[] = {"null", "0", "\"\"", "\"a b\"",
		"\"a\\u0000b\"", "[]"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"c\","
			"\"method\":\"cancel\",\"target\":%s}", bad[i]);
		assert(!mfl_request_parse(text, &request));
	}
	snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"v\","
		"\"method\":\"validate\",\"resource\":\"settings\","
		"\"provider\":\"%s\",\"generation\":\"%s\","
		"\"values\":{\"motion\":false}}", token, token);
	assert(mfl_request_parse(text, &request));
	assert(request.kind == MFL_REQUEST_SETTINGS_VALIDATE);
	assert(mfl_request_is_settings(request.kind));
	assert(!mfl_request_is_action(request.kind));
	assert(mfl_request_settings_command(command, sizeof(command), &request,
		42, "a-b"));
	assert(strstr(request.settings_payload, "\"operation\":\"validate\""));
	assert(strstr(request.settings_payload, "\"generation\""));
	assert(strstr(request.settings_payload, "\"values\""));
	assert(mfl_request_reserve(pending, &request, 42, 100, &slot) == NULL);
	assert(mfl_request_find(pending, "v") == slot);
	assert(mfl_request_find(pending, "missing") == NULL);
	assert(mfl_request_part(slot, "part 0 7b") ==
		MFL_PART_MORE);
	result = mfl_request_pending_result(pending, 200);
	assert(result);
	list = cJSON_GetObjectItemCaseSensitive(result, "requests");
	assert(cJSON_GetArraySize(list) == 1);
	item = cJSON_GetArrayItem(list, 0);
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(item,
		"state")->valuestring,
		"receiving-reply") == 0);
	assert(cJSON_GetObjectItemCaseSensitive(item,
		"remaining_ms")->valueint == 4900);
	cJSON_Delete(result);
	result = mfl_request_pending_result(pending, UINT64_MAX);
	item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(result,
		"requests"), 0);
	assert(cJSON_GetObjectItemCaseSensitive(item,
		"remaining_ms")->valueint == 0);
	assert(strcmp(cJSON_GetObjectItemCaseSensitive(item,
		"state")->valuestring,
		"deadline-reached") == 0);
	cJSON_Delete(result);
	mfl_request_clear(slot);
	assert(mfl_request_find(pending, "v") == NULL);
	result = mfl_request_pending_result(pending, 201);
	assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(result,
		"requests")) == 0);
	cJSON_Delete(result);
}

static void test_core_snapshot_requests(void)
{
	struct mfl_request request;
	struct mfl_pending_request pending = {.kind = MFL_REQUEST_TOPOLOGY,
		.serial = 42};
	char text[1001], command[1001];
	const char *resources[] = {"topology", "workspaces", "bindings",
		"colorsets"};
	const char *invalid[] = {
		"\"offset\":-1", "\"offset\":1", "\"offset\":4097",
		"\"offset\":0.5", "\"limit\":0", "\"limit\":17",
		"\"limit\":true", "\"generation\":\"bad\"",
		"\"generation\":\"0123456789abcdef0123456789abcdef\"",
		"\"limit\":2,\"limit\":3", "\"target\":\"x\""};

	for (size_t i = 0; i < sizeof(resources) / sizeof(resources[0]); i++)
	{
		snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"c\","
			"\"method\":\"query\",\"resource\":\"%s\"}",
			resources[i]);
		assert(mfl_request_parse(text, &request));
		assert(mfl_request_is_snapshot(request.kind));
		assert(!mfl_request_is_settings(request.kind));
		assert(!mfl_request_is_action(request.kind));
		assert(mfl_request_snapshot_command(command, sizeof(command),
			&request, 42, "a-b"));
		assert(strstr(command, "Send_Query a-b 000000000000002a "));
		assert(!mfl_request_snapshot_command(command, 4, &request,
			42, "a-b") && command[0] == '\0');
	}
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"c\","
			"\"method\":\"query\",\"resource\":\"bindings\",%s}",
			invalid[i]);
		assert(!mfl_request_parse(text, &request));
	}
	assert(mfl_request_parse("{\"version\":1,\"id\":\"c\","
		"\"method\":\"query\",\"resource\":\"bindings\","
		"\"offset\":4096,\"limit\":16,"
		"\"generation\":\"ffffffffffffffffffffffffffffffff"
		"ffffffffffffffff\"}", &request));
	assert(request.offset == 4096 && request.limit == 16);
	assert(mfl_request_snapshot_command(command, sizeof(command),
		&request, 42, "a-b"));
	assert(strstr(command, "bindings 4096 16 ffffffffffffffffffffffffffffffffffffffffffffffff"));
	const char *scopes[] = {"global", "menu", "menu-fallback",
		"placement", "policy"};
	const char *targets[] = {"bindings", "menu-bindings", "menu-defaults",
		"placement", "binding-policy"};

	for (size_t i = 0; i < sizeof(scopes) / sizeof(scopes[0]); i++)
	{
		snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"s\","
			"\"method\":\"query\",\"resource\":\"bindings\","
			"\"scope\":\"%s\"}", scopes[i]);
		assert(mfl_request_parse(text, &request));
		assert(mfl_request_snapshot_command(command, sizeof(command),
			&request, 42, "a-b"));
		assert(strstr(command, targets[i]));
		if (i < 3)
		{
			snprintf(text, sizeof(text),
				"{\"version\":1,\"id\":\"s\","
				"\"method\":\"query\","
				"\"resource\":\"bindings\",\"scope\":\"%s\","
				"\"offset\":8,\"limit\":16,\"generation\":\""
				"ffffffffffffffffffffffffffffffff"
				"ffffffffffffffff\"}", scopes[i]);
			assert(mfl_request_parse(text, &request));
			assert(request.offset == 8 && request.limit == 16);
		}
	}
	const char *bad_scopes[] = {"null", "true", "7", "\"unknown\"",
		"\"menu;Exec\"", "\"menu\",\"scope\":\"global\"",
		"\"placement\",\"offset\":0", "\"policy\",\"limit\":1"};

	for (size_t i = 0; i < sizeof(bad_scopes) / sizeof(bad_scopes[0]); i++)
	{
		snprintf(text, sizeof(text), "{\"version\":1,\"id\":\"s\","
			"\"method\":\"query\",\"resource\":\"bindings\","
			"\"scope\":%s}", bad_scopes[i]);
		assert(!mfl_request_parse(text, &request));
	}
	assert(mfl_request_part_payload(
		"MFL_CORE_1 a-b 000000000000002a part 0 7b", &pending, "a-b"));
	assert(!mfl_request_part_payload(
		"MFL_SETTINGS_1 a-b 000000000000002a part 0 7b",
		&pending, "a-b"));
	char chunk[MODULE_QUERY_CHUNK + 1];

	memset(chunk, 'x', sizeof(chunk) - 1);
	chunk[sizeof(chunk) - 1] = '\0';
	for (unsigned int i = 0; i < MODULE_QUERY_RESULT_MAX /
	    MODULE_QUERY_CHUNK; i++)
		settings_part(&pending, i, chunk);
	assert(pending.reply_length == MODULE_QUERY_RESULT_MAX);
	assert(mfl_request_part(&pending, "end 128") == MFL_PART_DONE);
	assert(mfl_request_part(&pending, "part 128 78") == MFL_PART_INVALID);
	mfl_request_clear(&pending);
}

static void test_provider_requests(void)
{
	struct mfl_request request;
	char provider[33], generation[33], line[128], query[256];
	const char *methods[] = {"query", "watch", "unwatch"};

	for (size_t i = 0; i < 3; i++)
	{
		snprintf(query, sizeof(query), "{\"version\":1,\"id\":\"p\","
			"\"method\":\"%s\",\"resource\":\"provider\"}",
			methods[i]);
		assert(mfl_request_parse(query, &request));
		assert(request.kind == (i == 0 ? MFL_REQUEST_PROVIDER :
			i == 1 ? MFL_REQUEST_WATCH : MFL_REQUEST_UNWATCH));
		assert(!mfl_request_is_action(request.kind));
	}
	const char *bad[] = {
		"\"method\":\"query\",\"resource\":\"provider\","
			"\"scope\":\"all\"",
		"\"method\":\"watch\",\"resource\":\"provider\","
			"\"provider\":true",
		"\"method\":\"unwatch\",\"resource\":\"provider\","
			"\"provider\":\"x\"",
		"\"method\":\"action\",\"resource\":\"provider\"",
		"\"method\":\"query\",\"resource\":\"settings\","
			"\"generation\":\"0123456789abcdef0123456789abcdef\""};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		snprintf(query, sizeof(query),
			"{\"version\":1,\"id\":\"p\",%s}",
			bad[i]);
		assert(!mfl_request_parse(query, &request));
	}
	snprintf(line, sizeof(line), "MFL_PROVIDER_1 %s %s",
		"0123456789abcdef0123456789abcdef",
		"fedcba9876543210fedcba9876543210");
	assert(mfl_provider_notice(line, provider, generation));
	assert(strcmp(provider, "0123456789abcdef0123456789abcdef") == 0);
	assert(strcmp(generation, "fedcba9876543210fedcba9876543210") == 0);
	line[15] = 'G';
	assert(!mfl_provider_notice(line, provider, generation));
	assert(!provider[0] && !generation[0]);
	assert(!mfl_provider_notice(NULL, provider, generation));
	assert(!mfl_provider_notice("MFL_PROVIDER_1 a b extra", provider,
		generation));
	assert(mfl_request_parse("{\"version\":1,\"id\":\"p\","
		"\"method\":\"query\",\"resource\":\"settings\","
		"\"provider\":\"0123456789abcdef0123456789abcdef\","
		"\"generation\":\"fedcba9876543210fedcba9876543210\"}",
		&request));
	assert(strstr(request.settings_payload, "generation"));
}

static size_t operation_allocations, operation_fail_at, operation_live;

static void *operation_malloc(size_t size)
{
	void *memory;

	if (operation_allocations++ == operation_fail_at)
		return NULL;
	memory = malloc(size);
	if (memory)
		operation_live++;
	return memory;
}

static void operation_free(void *memory)
{
	if (memory)
	{
		assert(operation_live > 0);
		operation_live--;
	}
	free(memory);
}

static void test_operation_requests(void)
{
	struct mfl_request request;
	char query[512], provider[33], operation[33], generation[33], state[10];
	const char *token = "0123456789abcdef0123456789abcdef";

	snprintf(query, sizeof(query), "{\"version\":1,\"id\":\"o\","
		"\"method\":\"action\",\"resource\":\"operations\","
		"\"operation\":\"start\",\"target\":\"applications\","
		"\"provider\":\"%s\",\"generation\":\"%s\"}", token, token);
	assert(mfl_request_parse(query, &request));
	assert(request.kind == MFL_REQUEST_OPERATION_START);
	assert(mfl_request_is_settings(request.kind));
	assert(mfl_request_is_action(request.kind));
	assert(strstr(request.settings_payload, "operations"));
	cJSON_Hooks hooks = {.malloc_fn = operation_malloc,
		.free_fn = operation_free};
	bool completed = false;

	for (operation_fail_at = 0; operation_fail_at < 1024;
	     operation_fail_at++)
	{
		operation_allocations = operation_live = 0;
		cJSON_InitHooks(&hooks);
		completed = mfl_request_parse(query, &request);
		assert(operation_live == 0);
		cJSON_InitHooks(NULL);
		if (completed)
			break;
	}
	assert(completed);
	snprintf(query, sizeof(query), "{\"version\":1,\"id\":\"o\","
		"\"method\":\"action\",\"resource\":\"operations\","
		"\"operation\":\"cancel\",\"target\":\"%s\","
		"\"provider\":\"%s\",\"generation\":\"%s\"}", token,
		token, token);
	assert(mfl_request_parse(query, &request));
	assert(request.kind == MFL_REQUEST_OPERATION_CANCEL);
	assert(mfl_request_is_action(request.kind));

	snprintf(query, sizeof(query), "{\"version\":1,\"id\":\"o\","
		"\"method\":\"query\",\"resource\":\"operations\","
		"\"provider\":\"%s\",\"target\":\"%s\"}", token, token);
	assert(mfl_request_parse(query, &request));
	assert(request.kind == MFL_REQUEST_OPERATION_QUERY);
	assert(!mfl_request_is_action(request.kind));
	snprintf(query, sizeof(query), "{\"version\":1,\"id\":\"o\","
		"\"method\":\"watch\",\"resource\":\"operations\","
		"\"provider\":\"%s\"}", token);
	assert(mfl_request_parse(query, &request));
	assert(request.operation_watch);
	snprintf(query, sizeof(query), "MFL_OPERATION_1 %s %s %s completed",
		token, token, token);
	assert(mfl_operation_notice(query, provider, operation,
		generation, state));
	assert(strcmp(state, "completed") == 0);
	strcat(query, " extra");
	assert(!mfl_operation_notice(query, provider, operation,
		generation, state));
	assert(!provider[0] && !operation[0] && !generation[0] && !state[0]);
	assert(!mfl_request_parse("{\"version\":1,\"id\":\"o\","
		"\"method\":\"query\",\"resource\":\"operations\"}", &request));
	assert(!mfl_operation_notice(NULL, provider, operation,
		generation, state));
}

static void test_colorset_requests(void)
{
	struct mfl_request request;
	char text[1001], command[1001];
	const char *bad[] = {"\"target\":2147483647",
		"\"target\":\"2147483648\"", "\"target\":\"01\"",
		"\"target\":\"-1\"", "\"target\":\"1;Exec\"",
		"\"target\":\"1\",\"offset\":0",
		"\"scope\":\"summary\",\"limit\":1",
		"\"scope\":\"summary\",\"target\":\"1\"",
		"\"scope\":\"unknown\"", "\"offset\":1",
		"\"limit\":0", "\"limit\":17", "\"generation\":\"abc\""};

	assert(mfl_request_parse("{\"version\":1,\"id\":\"c\","
		"\"method\":\"query\","
		"\"resource\":\"colorsets\",\"target\":\"2147483647\"}",
		&request));
	assert(request.kind == MFL_REQUEST_COLORSETS &&
		request.offset == 2147483647);
	assert(mfl_request_snapshot_command(command, sizeof(command),
		&request, 1, "a"));
	assert(strcmp(command,
		"Send_Query a 0000000000000001 colorset 2147483647 1 -") == 0);
	assert(!mfl_request_is_action(request.kind));
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		snprintf(text, sizeof(text),
			"{\"version\":1,\"id\":\"c\",\"method\":\"query\","
			"\"resource\":\"colorsets\",%s}", bad[i]);
		assert(!mfl_request_parse(text, &request));
	}
	assert(mfl_request_parse("{\"version\":1,\"id\":\"c\","
		"\"method\":\"query\","
		"\"resource\":\"colorsets\",\"scope\":\"summary\"}", &request));
	assert(strcmp(request.snapshot_resource, "color-summary") == 0);
}

int main(void)
{
	test_colorset_requests();
	test_core_snapshot_requests();
	test_provider_requests();
	test_operation_requests();
	test_lifecycle();
	test_schema();
	test_id_bounds();
	test_pending_bounds_and_reuse();
	test_private_core_tags();
	test_core_results_and_reply_ownership();
	test_settings_requests();
	test_settings_assembly_and_cleanup();
	return 0;
}
