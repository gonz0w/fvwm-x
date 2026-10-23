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

int main(void)
{
	struct preview_request request;
	char text[1001];
	const char *bad[] = {NULL, "", "a 1 1 query - - 0 0 -",
		"a 0000000000000001 0000000000000001 query - - 1 0 -",
		"a 0000000000000001 0000000000000001 query - - 0 0 - extra",
		"a;Exec 0000000000000001 0000000000000001 query - - 0 0 -"};

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
