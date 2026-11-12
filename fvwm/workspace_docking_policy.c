/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "workspace_docking_policy.h"
#include <stdbool.h>
#include <string.h>

static bool docking_name_valid(const char *name)
{
	if (!name)
		return false;
	for (size_t i = 0; i <= WORKSPACE_DOCKING_NAME_MAX; i++)
	{
		unsigned char byte = name[i];

		if (byte == '\0')
			return i != 0;
		if (byte < 32 || byte == 127)
			return false;
	}
	return false;
}

enum workspace_docking_result workspace_docking_resolve(const char *connector,
	const struct workspace_docking_connector *live, size_t count,
	uint64_t *role)
{
	uint64_t matched_role = 0;
	size_t matches = 0;

	if (!role)
		return WORKSPACE_DOCKING_INVALID;
	*role = 0;
	if (count > WORKSPACE_DOCKING_CONNECTOR_MAX || (count && !live) ||
	    !docking_name_valid(connector))
		return WORKSPACE_DOCKING_INVALID;
	for (size_t i = 0; i < count; i++)
	{
		if (!live[i].role || !docking_name_valid(live[i].name))
			return WORKSPACE_DOCKING_INVALID;
		if (strcmp(connector, live[i].name) == 0)
		{
			matched_role = live[i].role;
			matches++;
		}
	}
	if (matches == 0)
		return WORKSPACE_DOCKING_MISSING;
	if (matches > 1)
		return WORKSPACE_DOCKING_AMBIGUOUS;
	*role = matched_role;
	return WORKSPACE_DOCKING_MATCH;
}

static bool selector_valid(const struct workspace_docking_selector *selector)
{
	return selector && docking_name_valid(selector->instance) &&
		docking_name_valid(selector->class_name);
}

enum workspace_docking_result workspace_docking_match_window(
	const struct workspace_docking_selector *selector,
	const struct workspace_docking_selector *live, size_t count,
	size_t *index)
{
	size_t matched_index = SIZE_MAX;
	size_t matches = 0;

	if (!index)
		return WORKSPACE_DOCKING_INVALID;
	*index = SIZE_MAX;
	if (count > WORKSPACE_DOCKING_WINDOW_MAX || (count && !live) ||
	    !selector_valid(selector))
		return WORKSPACE_DOCKING_INVALID;
	for (size_t i = 0; i < count; i++)
	{
		if (!selector_valid(&live[i]))
			return WORKSPACE_DOCKING_INVALID;
		if (strcmp(selector->instance, live[i].instance) == 0 &&
		    strcmp(selector->class_name, live[i].class_name) == 0)
		{
			matched_index = i;
			matches++;
		}
	}
	if (matches == 0)
		return WORKSPACE_DOCKING_MISSING;
	if (matches > 1)
		return WORKSPACE_DOCKING_AMBIGUOUS;
	*index = matched_index;
	return WORKSPACE_DOCKING_MATCH;
}
