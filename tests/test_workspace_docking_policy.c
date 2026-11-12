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

static void test_exact_window_selector(void)
{
	const struct workspace_docking_selector selector = {"work", "Terminal"};
	const struct workspace_docking_selector live[] = {
		{"work", "Editor"},
		{"home", "Terminal"},
		{"Work", "Terminal"},
		{"work", "Terminal"},
		{"work", "terminal"},
	};
	size_t index = 99;

	assert(workspace_docking_match_window(&selector, live, 5, &index) ==
		WORKSPACE_DOCKING_MATCH && index == 3);
	assert(workspace_docking_match_window(&selector, live, 3, &index) ==
		WORKSPACE_DOCKING_MISSING && index == SIZE_MAX);
	assert(workspace_docking_match_window(&selector, NULL, 0, &index) ==
		WORKSPACE_DOCKING_MISSING && index == SIZE_MAX);
	assert(workspace_docking_match_window(&selector, live + 3, 1, &index) ==
		WORKSPACE_DOCKING_MATCH && index == 0);
}

static void test_ambiguous_window_selector(void)
{
	const struct workspace_docking_selector selector = {"work", "Terminal"};
	const struct workspace_docking_selector live[] = {
		{"work", "Terminal"}, {"home", "Terminal"}, {"work", "Terminal"},
	};
	size_t index = 99;

	assert(workspace_docking_match_window(&selector, live, 3, &index) ==
		WORKSPACE_DOCKING_AMBIGUOUS && index == SIZE_MAX);
}

static void test_window_selector_bounds(void)
{
	struct workspace_docking_selector selector = {"work", "Terminal"};
	struct workspace_docking_selector live[WORKSPACE_DOCKING_WINDOW_MAX];
	size_t index = 99;

	for (size_t i = 0; i < WORKSPACE_DOCKING_WINDOW_MAX; i++)
		live[i] = (struct workspace_docking_selector){"home", "Editor"};
	memset(selector.instance, 'x', WORKSPACE_DOCKING_NAME_MAX);
	selector.instance[WORKSPACE_DOCKING_NAME_MAX] = '\0';
	live[WORKSPACE_DOCKING_WINDOW_MAX - 1] = selector;
	assert(workspace_docking_match_window(&selector, live,
		WORKSPACE_DOCKING_WINDOW_MAX, &index) ==
		WORKSPACE_DOCKING_MATCH &&
		index == WORKSPACE_DOCKING_WINDOW_MAX - 1);
	assert(workspace_docking_match_window(&selector, live,
		WORKSPACE_DOCKING_WINDOW_MAX + 1, &index) ==
		WORKSPACE_DOCKING_INVALID && index == SIZE_MAX);
	selector.instance[WORKSPACE_DOCKING_NAME_MAX] = 'x';
	assert(workspace_docking_match_window(&selector, live, 1, &index) ==
		WORKSPACE_DOCKING_INVALID && index == SIZE_MAX);
}

static void test_invalid_window_selectors(void)
{
	struct workspace_docking_selector selector = {"work", "Terminal"};
	struct workspace_docking_selector live[] = {
		{"work", "Terminal"}, {"", "Editor"},
	};
	size_t index = 99;

	assert(workspace_docking_match_window(&selector, live, 1, NULL) ==
		WORKSPACE_DOCKING_INVALID);
	assert(workspace_docking_match_window(NULL, live, 1, &index) ==
		WORKSPACE_DOCKING_INVALID && index == SIZE_MAX);
	assert(workspace_docking_match_window(&selector, NULL, 1, &index) ==
		WORKSPACE_DOCKING_INVALID && index == SIZE_MAX);
	assert(workspace_docking_match_window(&selector, live, 2, &index) ==
		WORKSPACE_DOCKING_INVALID && index == SIZE_MAX);
	strcpy(selector.class_name, "Terminal\n");
	assert(workspace_docking_match_window(&selector, live, 1, &index) ==
		WORKSPACE_DOCKING_INVALID && index == SIZE_MAX);
	selector.class_name[0] = '\0';
	assert(workspace_docking_match_window(&selector, live, 1, &index) ==
		WORKSPACE_DOCKING_INVALID && index == SIZE_MAX);
}

int main(void)
{
	test_exact_connector_identity();
	test_ambiguous_connector();
	test_input_bounds();
	test_invalid_facts();
	test_exact_window_selector();
	test_ambiguous_window_selector();
	test_window_selector_bounds();
	test_invalid_window_selectors();
	return 0;
}
