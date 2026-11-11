/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "fvwm/workspace_docking_policy.h"
#include <assert.h>
#include <string.h>

static void test_exact_connector_identity(void)
{
	const struct workspace_docking_connector first[] = {
		{"DP-10", 11}, {"DP-1", 12}, {"HDMI-1", 13},
	};
	const struct workspace_docking_connector second[] = {
		{"HDMI-1", 42}, {"DP-1", 41},
	};
	uint64_t role;

	assert(workspace_docking_resolve("DP-1", first, 3, &role) ==
		WORKSPACE_DOCKING_MATCH && role == 12);
	/* The same configured name can acquire a new live token on restart. */
	assert(workspace_docking_resolve("DP-1", second, 2, &role) ==
		WORKSPACE_DOCKING_MATCH && role == 41);
	assert(workspace_docking_resolve("dp-1", first, 3, &role) ==
		WORKSPACE_DOCKING_MISSING && role == 0);
	assert(workspace_docking_resolve("DP-10", second, 2, &role) ==
		WORKSPACE_DOCKING_MISSING && role == 0);
	assert(workspace_docking_resolve("DP-1", NULL, 0, &role) ==
		WORKSPACE_DOCKING_MISSING && role == 0);
}

static void test_ambiguous_connector(void)
{
	struct workspace_docking_connector live[] = {
		{"DP-1", 10}, {"DP-1", 20},
	};
	uint64_t role = 99;

	assert(workspace_docking_resolve("DP-1", live, 2, &role) ==
		WORKSPACE_DOCKING_AMBIGUOUS && role == 0);
	/* A repeated connector fact cannot prove a unique live match. */
	live[1].role = live[0].role;
	role = 99;
	assert(workspace_docking_resolve("DP-1", live, 2, &role) ==
		WORKSPACE_DOCKING_AMBIGUOUS && role == 0);
	/* Distinct cloned connectors can refer to the same current monitor. */
	strcpy(live[1].name, "DP-2");
	assert(workspace_docking_resolve("DP-2", live, 2, &role) ==
		WORKSPACE_DOCKING_MATCH && role == 10);
}

static void test_input_bounds(void)
{
	struct workspace_docking_connector
		live[WORKSPACE_DOCKING_CONNECTOR_MAX];
	char name[WORKSPACE_DOCKING_NAME_MAX + 2];
	uint64_t role;

	memset(name, 'x', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	assert(workspace_docking_resolve(name, NULL, 0, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
	name[WORKSPACE_DOCKING_NAME_MAX] = '\0';
	for (size_t i = 0; i < WORKSPACE_DOCKING_CONNECTOR_MAX; i++)
		live[i] = (struct workspace_docking_connector){"DP-2", 1};
	strcpy(live[WORKSPACE_DOCKING_CONNECTOR_MAX - 1].name, name);
	live[WORKSPACE_DOCKING_CONNECTOR_MAX - 1].role = UINT64_MAX;
	assert(workspace_docking_resolve(name, live,
		WORKSPACE_DOCKING_CONNECTOR_MAX, &role) ==
		WORKSPACE_DOCKING_MATCH && role == UINT64_MAX);
	assert(workspace_docking_resolve(name, live,
		WORKSPACE_DOCKING_CONNECTOR_MAX + 1, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
}

static void test_invalid_facts(void)
{
	struct workspace_docking_connector live[] = {{"DP-1", 5}, {"DP-2", 6}};
	uint64_t role;

	assert(workspace_docking_resolve(NULL, live, 2, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
	assert(workspace_docking_resolve("", live, 2, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
	assert(workspace_docking_resolve("DP-1\n", live, 2, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
	assert(workspace_docking_resolve("DP-1", NULL, 1, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
	assert(workspace_docking_resolve("DP-1", live, 2, NULL) ==
		WORKSPACE_DOCKING_INVALID);
	live[1].role = 0;
	role = 99;
	assert(workspace_docking_resolve("DP-1", live, 2, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
	live[1].role = 6;
	memset(live[1].name, 'x', sizeof(live[1].name));
	assert(workspace_docking_resolve("DP-1", live, 2, &role) ==
		WORKSPACE_DOCKING_INVALID && role == 0);
}

int main(void)
{
	test_exact_connector_identity();
	test_ambiguous_connector();
	test_input_bounds();
	test_invalid_facts();
	return 0;
}
