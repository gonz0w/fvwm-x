/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "fvwm/workspace_tools_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void request(struct scratchpad_request *r, enum scratchpad_verb verb,
	unsigned int serial)
{
	memset(r, 0, sizeof(*r));
	strcpy(r->cookie, "abc");
	snprintf(r->serial, sizeof(r->serial), "%016x", serial);
	strcpy(r->name, "terminal");
	memset(r->fingerprint, 'a', 64);
	strcpy(r->class_name, "Terminal");
	r->verb = verb;
}

int main(void)
{
	struct scratchpad_registry registry;
	struct scratchpad_request r;
	struct scratchpad_result result;
	struct scratchpad_observation empty = {0};
	struct scratchpad_observation matched = {.matches = 1, .target = 7};
	const char *lease = "0123456789abcdef0123456789abcdef0000000000000001";
	char text[1001];
	int x, y;

	snprintf(text, sizeof(text), "v1 abc 0000000000000001 toggle "
		"7465726d "
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa "
		"5465726d696e616c - -");
	assert(scratchpad_request_parse(text, &r));
	assert(strcmp(r.name, "term") == 0);
	/* A field width may not manufacture a separator inside one word. */
	assert(!scratchpad_request_parse("v1 abc 0000000000000001toggle "
		"7465726d "
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 5465726d696e616c - -", &r));
	assert(!scratchpad_request_parse("v1 x 1 toggle - - - - -", &r));
	assert(!r.cookie[0]);
	scratchpad_registry_initialize(&registry, false);
	request(&r, SCRATCHPAD_TOGGLE, 1);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 1,
		lease, &result));
	assert(result.state == SCRATCHPAD_PENDING &&
		result.action == SCRATCHPAD_LAUNCH);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 2,
		lease, &result));
	assert(result.action == SCRATCHPAD_NONE && result.desired_visible);
	request(&r, SCRATCHPAD_TOGGLE, 2);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 2,
		lease, &result));
	assert(!result.desired_visible && result.action == SCRATCHPAD_NONE);
	request(&r, SCRATCHPAD_STATUS, 3);
	assert(scratchpad_registry_apply(&registry, 1, &r, &matched, 3,
		lease, &result));
	assert(result.state == SCRATCHPAD_CLAIMED &&
		result.action == SCRATCHPAD_DISMISS);
	request(&r, SCRATCHPAD_TOGGLE, 4);
	matched.visible = false;
	assert(scratchpad_registry_apply(&registry, 1, &r, &matched, 4,
		lease, &result));
	assert(result.action == SCRATCHPAD_SUMMON);
	request(&r, SCRATCHPAD_STATUS, 5);
	assert(!scratchpad_registry_apply(&registry, 2, &r, &matched, 4,
		lease, &result));
	assert(strcmp(result.reason, "owner-conflict") == 0);
	scratchpad_registry_initialize(&registry, false);
	request(&r, SCRATCHPAD_TOGGLE, 1);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 0,
		lease, &result));
	request(&r, SCRATCHPAD_STATUS, 2);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 30000,
		lease, &result));
	assert(result.state == SCRATCHPAD_UNCERTAIN &&
		result.action == SCRATCHPAD_NONE);
	request(&r, SCRATCHPAD_RETRY, 3);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 30001,
		lease, &result));
	assert(result.action == SCRATCHPAD_LAUNCH);
	scratchpad_registry_owner_gone(&registry, 1);
	assert(registry.entries[0].state == SCRATCHPAD_UNCERTAIN);
	scratchpad_registry_initialize(&registry, true);
	request(&r, SCRATCHPAD_TOGGLE, 1);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 1,
		lease, &result));
	assert(result.state == SCRATCHPAD_UNCERTAIN &&
		result.action == SCRATCHPAD_NONE);
	matched.unsupported = true;
	request(&r, SCRATCHPAD_STATUS, 2);
	assert(scratchpad_registry_apply(&registry, 1, &r, &matched, 2,
		lease, &result));
	assert(result.state == SCRATCHPAD_UNSUPPORTED &&
		result.action == SCRATCHPAD_NONE);
	matched.unsupported = false;
	matched.matches = 2;
	request(&r, SCRATCHPAD_RETRY, 3);
	assert(scratchpad_registry_apply(&registry, 1, &r, &matched, 3,
		lease, &result));
	assert(result.state == SCRATCHPAD_AMBIGUOUS &&
		result.action == SCRATCHPAD_NONE);
	/* Initial status observes visibility without a mutation intent. */
	scratchpad_registry_initialize(&registry, false);
	matched.matches = 1;
	matched.visible = true;
	request(&r, SCRATCHPAD_STATUS, 1);
	assert(scratchpad_registry_apply(&registry, 1, &r, &matched, 1,
		lease, &result));
	assert(result.action == SCRATCHPAD_NONE && result.desired_visible);
	request(&r, SCRATCHPAD_STATUS, 2);
	matched.visible = false;
	assert(scratchpad_registry_apply(&registry, 1, &r, &matched, 2,
		lease, &result));
	assert(result.action == SCRATCHPAD_NONE && !result.desired_visible);
	/* Destruction makes a fresh toggle eligible for exactly one launch. */
	request(&r, SCRATCHPAD_TOGGLE, 3);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 3,
		lease, &result));
	assert(result.action == SCRATCHPAD_LAUNCH);
	request(&r, SCRATCHPAD_LAUNCHED, 4);
	strcpy(r.lease, lease);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 4,
		lease, &result));
	assert(result.action == SCRATCHPAD_NONE);
	request(&r, SCRATCHPAD_LAUNCH_FAILED, 5);
	strcpy(r.lease, lease);
	r.lease[0] = 'f';
	assert(!scratchpad_registry_apply(&registry, 1, &r, &empty, 5,
		lease, &result));
	assert(strcmp(result.reason, "lease-mismatch") == 0);
	request(&r, SCRATCHPAD_LAUNCH_FAILED, 6);
	strcpy(r.lease, lease);
	assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 6,
		lease, &result));
	assert(result.state == SCRATCHPAD_UNMATCHED);
	/* Overlapping selectors cannot launch under two names. */
	request(&r, SCRATCHPAD_TOGGLE, 7);
	strcpy(r.name, "duplicate");
	assert(!scratchpad_registry_apply(&registry, 1, &r, &empty, 7,
		lease, &result));
	assert(strcmp(result.reason, "definition-conflict") == 0);
	/* Sixteen different exact selectors fit; the seventeenth is refused. */
	scratchpad_registry_initialize(&registry, false);
	for (unsigned int i = 0; i <= SCRATCHPAD_MAX; i++)
	{
		request(&r, SCRATCHPAD_STATUS, i + 1);
		snprintf(r.name, sizeof(r.name), "pad%u", i);
		snprintf(r.class_name, sizeof(r.class_name), "Class%u", i);
		assert(scratchpad_registry_apply(&registry, 1, &r, &empty, 0,
			lease, &result) == (i < SCRATCHPAD_MAX));
	}
	assert(strcmp(result.reason, "table-full") == 0);
	assert(workspace_center(-1920, 20, 1920, 1040, 800, 600, &x, &y));
	assert(x == -1360 && y == 240);
	assert(workspace_center(0, 0, 800, 600, 1200, 900, &x, &y));
	assert(x == 0 && y == 0);
	assert(!workspace_center(0, 0, 0, 600, 800, 600, &x, &y));
	return 0;
}
