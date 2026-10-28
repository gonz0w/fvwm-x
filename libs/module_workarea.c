/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "module_workarea.h"
#include "libs/Parse.h"
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

void module_workarea_sync(module_workarea_state *state,
			  const module_ui_scale_snapshot *snapshot)
{
	if (state->epoch != snapshot->epoch)
	{
		*state = (module_workarea_state){0};
		state->epoch = snapshot->epoch;
	}
}

static bool module_workarea_contained(const module_monitor_area *physical,
				      const module_monitor_area *area)
{
	return physical->width > 0 && physical->height > 0 &&
	       (int64_t)physical->x + physical->width <= INT_MAX &&
	       (int64_t)physical->y + physical->height <= INT_MAX &&
	       area->width > 0 && area->height > 0 && area->x >= physical->x &&
	       area->y >= physical->y &&
	       (int64_t)area->x + area->width <=
		   (int64_t)physical->x + physical->width &&
	       (int64_t)area->y + area->height <=
		   (int64_t)physical->y + physical->height;
}

module_workarea_status
module_workarea_receive(module_workarea_state *state,
			const module_ui_scale_snapshot *snapshot,
			const char *text)
{
	char line[MODULE_UI_SCALE_LINE_MAX], name[MODULE_UI_SCALE_NAME_MAX + 1];
	char *token, *tail;
	int values[MODULE_MONITOR_VALUES];
	module_monitor_area physical, area;
	size_t length;
	unsigned int index;

	if (text == NULL || strncmp(text, "Monitor", 7) != 0 ||
	    !isspace((unsigned char)text[7]))
		return MODULE_WORKAREA_IGNORED;
	length = strnlen(text, sizeof(line));
	if (length == sizeof(line) || snapshot->epoch == 0 ||
	    snapshot->count > MODULE_UI_SCALE_MAX)
		return MODULE_WORKAREA_REFUSED;
	module_workarea_sync(state, snapshot);
	memcpy(line, text, length + 1);
	token = PeekToken(line + 7, &tail);
	if (token == NULL || strlen(token) > MODULE_UI_SCALE_NAME_MAX)
		return MODULE_WORKAREA_REFUSED;
	strcpy(name, token);
	for (index = 0; index < snapshot->count; index++)
		if (strcmp(snapshot->rows[index].name, name) == 0)
			break;
	if (index == snapshot->count)
		return MODULE_WORKAREA_REFUSED;
	for (unsigned int i = 0; i < MODULE_MONITOR_VALUES; i++)
	{
		const char *digit;

		token = PeekToken(tail, &tail);
		if (token == NULL)
			return MODULE_WORKAREA_REFUSED;
		digit = token;
		if (*digit == '+' || *digit == '-')
			digit++;
		if (*digit == '\0')
			return MODULE_WORKAREA_REFUSED;
		for (; *digit != '\0'; digit++)
			if (*digit < '0' || *digit > '9')
				return MODULE_WORKAREA_REFUSED;
		if (!module_parse_decimal_integers(token, &values[i], 1))
			return MODULE_WORKAREA_REFUSED;
	}
	if (PeekToken(tail, NULL) != NULL || values[10] < 0 || values[11] < 0 ||
	    values[12] < 0 || values[13] < 0)
		return MODULE_WORKAREA_REFUSED;
	physical = (module_monitor_area){snapshot->rows[index].rect.x,
					 snapshot->rows[index].rect.y,
					 snapshot->rows[index].rect.width,
					 snapshot->rows[index].rect.height};
	if (!module_monitor_prepare_area(values, &physical, false, &area) ||
	    !module_workarea_contained(&physical, &area))
		return MODULE_WORKAREA_REFUSED;
	state->areas[index] = area;
	state->valid[index] = true;
	return MODULE_WORKAREA_ADMITTED;
}

bool module_workarea_bounds(const module_workarea_state *state,
			    const module_ui_scale_snapshot *snapshot,
			    const module_ui_scale_row *row,
			    module_monitor_area *area)
{
	module_monitor_area physical;

	*area = (module_monitor_area){0};
	if (row == NULL || snapshot->epoch == 0 ||
	    snapshot->count > MODULE_UI_SCALE_MAX)
		return false;
	physical = (module_monitor_area){row->rect.x, row->rect.y,
					 row->rect.width, row->rect.height};
	if (!module_workarea_contained(&physical, &physical))
		return false;
	for (unsigned int i = 0; i < snapshot->count; i++)
		if (row == &snapshot->rows[i])
		{
			*area = physical;
			if (state->epoch == snapshot->epoch &&
			    state->valid[i] &&
			    module_workarea_contained(&physical,
						      &state->areas[i]))
				*area = state->areas[i];
			return true;
		}
	return false;
}
