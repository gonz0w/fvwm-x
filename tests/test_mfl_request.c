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

int main(void)
{
	test_schema();
	test_id_bounds();
	test_pending_bounds_and_reuse();
	test_private_core_tags();
	test_core_results_and_reply_ownership();
	return 0;
}
