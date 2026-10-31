/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "fvwm/preview_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char generation[] =
	"000000000000000000000000000000000000000000000001";

static void test_resource_policy(void)
{
	struct preview_request request;
	uint32_t colors[14][4];
	char plan[337], text[1001];
	size_t copied = 0;

	for (unsigned int i = 0; i < 56; i++)
		memcpy(plan + 6 * i, "1a2b3c", 6);
	plan[336] = '\0';
	assert(preview_plan_parse(plan, colors));
	for (unsigned int i = 0; i < 14; i++)
		for (unsigned int j = 0; j < 4; j++)
			assert(colors[i][j] == 0x1a2b3c);
	snprintf(text, sizeof(text),
		"a 0000000000000001 0000000000000001 "
		"prepare %s - 0 15000 1:%s", generation, plan);
	assert(preview_request_parse(text, &request));
	assert(request.operation == PREVIEW_PREPARE);
	assert(request.resource_kind == PREVIEW_RESOURCE_HIGH_CONTRAST);
	assert(request.timeout_ms == 15000 &&
		request.colors[13][3] == 0x1a2b3c);
	snprintf(text, sizeof(text),
		"a 0000000000000001 0000000000000001 "
		"prepare %s - 0 15000 2:%s", generation, plan);
	assert(preview_request_parse(text, &request));
	assert(request.resource_kind == PREVIEW_RESOURCE_OPAQUE_COLORS);
	assert(request.timeout_ms == 15000 &&
		request.colors[13][3] == 0x1a2b3c);
	plan[335] = '\0';
	assert(!preview_plan_parse(plan, colors));
	assert(colors[13][3] == 0);
	plan[335] = 'C';
	assert(!preview_plan_parse(plan, colors));
	plan[335] = 'c';
	assert(preview_plan_parse(plan, colors));
	for (unsigned int i = 0; i < 4; i++)
	{
		snprintf(text, sizeof(text),
			"a 0000000000000001 0000000000000001 "
			"prepare %s - %u %u %s:%s", generation,
			i == 0 ? 1 : 0, i == 1 ? 999 : 15000,
			i == 2 ? "3" : "1", i == 3 ? "00" : plan);
		assert(!preview_request_parse(text, &request));
		assert(request.cookie[0] == '\0');
	}
	for (unsigned int i = 0; i < 4; i++)
	{
		static const char *versions[] = {"0", "3", "01", "1.0"};

		snprintf(text, sizeof(text),
			"a 0000000000000001 0000000000000001 "
			"prepare %s - 0 15000 %s:%s",
			generation, versions[i], plan);
		assert(!preview_request_parse(text, &request));
		assert(request.resource_kind == 0 && request.cookie[0] == '\0');
	}
	snprintf(text, sizeof(text),
		"a 0000000000000001 0000000000000001 "
		"apply %s %s 0 0 -", generation, generation);
	assert(preview_request_parse(text, &request));
	assert(request.operation == PREVIEW_APPLY);
	assert(preview_metadata_add(&copied, 65535));
	assert(preview_metadata_add(&copied, 1) && copied == 65536);
	assert(!preview_metadata_add(&copied, 1) && copied == 65536);
	assert(!preview_metadata_add(&copied, SIZE_MAX));
	assert(preview_generation_room(generation));
	assert(!preview_generation_room(NULL));
	assert(preview_generation_room(
		"00000000000000000000000000000000fffffffffffffff9"));
	assert(!preview_generation_room(
		"00000000000000000000000000000000fffffffffffffffa"));
	assert(!preview_generation_room(
		"00000000000000000000000000000000fffffffffffffffc"));
	assert(preview_resource_colors_supported(0, 0, 100));
	assert(!preview_resource_colors_supported(1, 0, 100));
	assert(!preview_resource_colors_supported(0, 1, 100));
	assert(!preview_resource_colors_supported(0, 0, 99));
	assert(preview_resource_guard_check(99, 100, true, true) ==
		PREVIEW_RESOURCE_VALID);
	assert(preview_resource_guard_check(100, 100, true, true) ==
		PREVIEW_RESOURCE_EXPIRED);
	assert(preview_resource_guard_check(99, 100, true, false) ==
		PREVIEW_RESOURCE_OWNER_GONE);
	assert(preview_resource_guard_check(100, 100, false, false) ==
		PREVIEW_RESOURCE_CONFLICT);
}

static void test_complete_plan(void)
{
	struct preview_request request;
	char roles[337], path[345], text[1001];

	memset(roles, 'a', 336);
	roles[336] = '\0';
	/* 256 pathname bytes have 344 canonical padded base64 bytes. */
	memset(path, 'Y', 340);
	memcpy(path + 340, "YQ==", 5);
	snprintf(text, sizeof(text),
		"a 0000000000000001 0000000000000001 prepare %s "
		"- 0 15000 3:%s:0d:031:042:123abc:%s",
		generation, roles, path);
	assert(preview_request_parse(text, &request));
	assert(request.resource_kind == PREVIEW_RESOURCE_COMPLETE);
	assert(request.plan.flags == 13 && request.plan.menu_opacity == 31 &&
		request.plan.launcher_opacity == 42);
	assert(strlen(request.plan.path) == 256);
	assert(PREVIEW_PLAN_TOKEN_MAX == 701);
	assert(!background_plan_parse("10:100:100:000000:-", &request.plan));
	assert(!background_plan_parse("06:100:100:000000:-", &request.plan));
	assert(!background_plan_parse("04:100:100:000000:YQ=", &request.plan));
	assert(!background_plan_parse("04:100:100:000000:YR==", &request.plan));
	assert(!background_plan_parse("04:100:100:000000:AA==", &request.plan));
	assert(!background_plan_parse("00:100:100:000000:YQ==", &request.plan));
	assert(background_plan_parse("01:000:100:000000:-", &request.plan));
}

static void test_resource_menu_opacity(void)
{
	const int captured[] = {0, 31, 42, 100};
	int opacity;

	for (size_t i = 0; i < sizeof(captured) / sizeof(*captured); i++)
	{
		assert(preview_resource_opacity(PREVIEW_RESOURCE_HIGH_CONTRAST,
			false, captured[i], &opacity) && opacity == 100);
		assert(preview_resource_opacity(PREVIEW_RESOURCE_HIGH_CONTRAST,
			true, captured[i], &opacity) && opacity == captured[i]);
		assert(preview_resource_opacity(PREVIEW_RESOURCE_OPAQUE_COLORS,
			false, captured[i], &opacity) &&
			opacity == captured[i]);
		assert(preview_resource_opacity(PREVIEW_RESOURCE_OPAQUE_COLORS,
			true, captured[i], &opacity) && opacity == captured[i]);
	}
	opacity = 73;
	assert(!preview_resource_opacity(0, false, 31, &opacity) &&
		opacity == 0);
	opacity = 73;
	assert(!preview_resource_opacity(3, false, 31, &opacity) &&
		opacity == 0);
	assert(!preview_resource_opacity(PREVIEW_RESOURCE_OPAQUE_COLORS,
		false, -1, &opacity) && opacity == 0);
	assert(!preview_resource_opacity(PREVIEW_RESOURCE_HIGH_CONTRAST,
		true, 101, &opacity) && opacity == 0);
	assert(!preview_resource_opacity(PREVIEW_RESOURCE_HIGH_CONTRAST,
		false, 31, NULL));
}

int main(void)
{
	struct preview_request request;
	char text[1001];
	const char *bad[] = {NULL, "", "a 1 1 query - - 0 0 -",
		"a 0000000000000001 0000000000000001 query - - 1 0 -",
		"a 0000000000000001 0000000000000001 query - - 0 0 - extra",
		"a;Exec 0000000000000001 0000000000000001 query - - 0 0 -"};

	test_resource_policy();
	test_complete_plan();
	assert(preview_publication_queue_check(0) == PREVIEW_PUBLICATION_READY);
	assert(preview_publication_queue_check(64) == PREVIEW_PUBLICATION_READY);
	assert(preview_publication_queue_check(65) == PREVIEW_PUBLICATION_RETRY);
	assert(preview_publication_queue_check(SIZE_MAX) ==
		PREVIEW_PUBLICATION_RETRY);
	assert(preview_root_notice_matches(42, 9, 42, 9, false, true));
	assert(!preview_root_notice_matches(42, 9, 42, 9, true, true));
	assert(!preview_root_notice_matches(42, 9, 42, 9, false, false));
	assert(!preview_root_notice_matches(42, 10, 42, 9, false, true));
	assert(!preview_root_notice_matches(43, 9, 42, 9, false, true));
	assert(!preview_root_notice_matches(42, 0, 42, 0, false, true));
	test_resource_menu_opacity();

	assert(preview_request_parse(
		"a 0000000000000001 0000000000000001 query - - 0 0 -",
		&request));
	assert(request.operation == PREVIEW_QUERY);
	snprintf(text, sizeof(text),
		"a 0000000000000001 0000000000000001 "
		"start %s - 73 1000 6d656e75",
		generation);
	assert(preview_request_parse(text, &request));
	assert(strcmp(request.target, "menu") == 0 && request.opacity == 73);
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		assert(!preview_request_parse(bad[i], &request));
		assert(request.cookie[0] == '\0');
	}
	for (unsigned int i = 0; i < 4; i++)
	{
		snprintf(text, sizeof(text),
			"a 0000000000000001 0000000000000001 "
			"start %s - %s %s %s",
			generation, i == 0 ? "101" : "73",
			i == 1 ? "999" : "1000",
			i == 2 ? "00" : i == 3 ? "1" : "61");
		assert(!preview_request_parse(text, &request));
	}
	snprintf(text, sizeof(text),
		"a 0000000000000001 0000000000000001 confirm %s %s 0 0 -",
		generation, generation);
	assert(preview_request_parse(text, &request));
	assert(request.operation == PREVIEW_CONFIRM);
	assert(preview_deadline(UINT64_MAX - 1, 1000) == UINT64_MAX);
	assert(preview_remaining(100, 150) == 50);
	assert(preview_remaining(150, 150) == 0);
	assert(preview_remaining(151, 150) == 0);
	assert(preview_recovery_check(100, 150, generation,
		generation, false) ==
		PREVIEW_WAIT);
	assert(preview_recovery_check(150, 150, generation,
		generation, false) ==
		PREVIEW_RESTORE);
	assert(preview_recovery_check(100, 150, generation, generation, true) ==
		PREVIEW_RESTORE);
	assert(preview_recovery_check(150, 150, generation, NULL, true) ==
		PREVIEW_CONFLICT);
	assert(preview_recovery_check(100, 150, generation, "other", false) ==
		PREVIEW_CONFLICT);
	return 0;
}
