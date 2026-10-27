/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "module_ui_scale.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	const char *text;
	size_t length;
} scale_token;

static bool row_valid(const module_ui_scale_row *row)
{
	size_t length = strnlen(row->name, sizeof(row->name));

	return row->name_atom != None && row->name_atom <= UINT32_MAX &&
		row->quarters >= MONITOR_SCALE_QUARTERS_MIN &&
		row->quarters <= MONITOR_SCALE_QUARTERS_MAX && length > 0 &&
		length <= MODULE_UI_SCALE_NAME_MAX &&
		row->rect.width > 0 && row->rect.height > 0 &&
		row->rect.x <= INT_MAX - row->rect.width &&
		row->rect.y <= INT_MAX - row->rect.height;
}

bool module_ui_scale_validate(const module_ui_scale_snapshot *snapshot)
{
	if (snapshot == NULL || snapshot->epoch == 0 || snapshot->count == 0 ||
	    snapshot->count > MODULE_UI_SCALE_MAX)
		return false;
	if (snapshot->root_fallback && (snapshot->count != 1 ||
	    snapshot->rows[0].rect.x != 0 || snapshot->rows[0].rect.y != 0 ||
	    !snapshot->rows[0].primary))
		return false;
	for (unsigned int i = 0; i < snapshot->count; i++)
	{
		if (!row_valid(&snapshot->rows[i]))
			return false;
		for (unsigned int j = 0; j < i; j++)
			if (snapshot->rows[i].name_atom ==
			    snapshot->rows[j].name_atom ||
			    strcmp(snapshot->rows[i].name,
				snapshot->rows[j].name) == 0)
				return false;
	}
	return true;
}

bool module_ui_scale_format(const module_ui_scale_snapshot *snapshot,
	module_ui_scale_bundle *bundle)
{
	static const char digits[] = "0123456789abcdef";
	int length;

	if (bundle == NULL)
		return false;
	bundle->count = 0;
	if (!module_ui_scale_validate(snapshot))
		return false;
	length = snprintf(bundle->lines[0], MODULE_UI_SCALE_LINE_MAX,
		"MonitorUI 1 BEGIN %016" PRIx64 " %u %u", snapshot->epoch,
		snapshot->count, snapshot->root_fallback ? 1U : 0U);
	if (length < 0 || length >= MODULE_UI_SCALE_LINE_MAX)
		return false;
	for (unsigned int i = 0; i < snapshot->count; i++)
	{
		const module_ui_scale_row *row = &snapshot->rows[i];
		char hex[2 * MODULE_UI_SCALE_NAME_MAX + 1];
		size_t bytes = strlen(row->name);

		for (size_t j = 0; j < bytes; j++)
		{
			unsigned char byte = (unsigned char)row->name[j];

			hex[2 * j] = digits[byte >> 4];
			hex[2 * j + 1] = digits[byte & 15];
		}
		hex[2 * bytes] = '\0';
		length = snprintf(bundle->lines[i + 1],
			MODULE_UI_SCALE_LINE_MAX,
			"MonitorUI 1 ROW %016" PRIx64 " %u %lu %u %u "
			"%d %d %d %d %s", snapshot->epoch, i, row->name_atom,
			row->quarters, row->primary ? 1U : 0U, row->rect.x,
			row->rect.y, row->rect.width, row->rect.height, hex);
		if (length < 0 || length >= MODULE_UI_SCALE_LINE_MAX)
			return false;
	}
	length = snprintf(bundle->lines[snapshot->count + 1],
		MODULE_UI_SCALE_LINE_MAX, "MonitorUI 1 END %016" PRIx64 " %u",
		snapshot->epoch, snapshot->count);
	if (length < 0 || length >= MODULE_UI_SCALE_LINE_MAX)
		return false;
	bundle->count = snapshot->count + 2;
	return true;
}

static bool token_is(scale_token token, const char *literal)
{
	return token.length == strlen(literal) &&
		memcmp(token.text, literal, token.length) == 0;
}

static int hex_digit(unsigned char byte)
{
	if (byte >= '0' && byte <= '9')
		return byte - '0';
	if (byte >= 'a' && byte <= 'f')
		return byte - 'a' + 10;
	if (byte >= 'A' && byte <= 'F')
		return byte - 'A' + 10;
	return -1;
}

static bool parse_epoch(scale_token token, uint64_t *epoch)
{
	uint64_t value = 0;

	if (token.length != 16)
		return false;
	for (size_t i = 0; i < token.length; i++)
	{
		unsigned char byte = token.text[i];
		int digit = hex_digit(byte);

		if (digit < 0 || (byte >= 'A' && byte <= 'F'))
			return false;
		value = (value << 4) | (unsigned int)digit;
	}
	*epoch = value;
	return value != 0;
}

static bool parse_unsigned(scale_token token, uint64_t maximum,
	uint64_t *result)
{
	uint64_t value = 0;

	if (token.length == 0)
		return false;
	for (size_t i = 0; i < token.length; i++)
	{
		unsigned char byte = token.text[i];
		unsigned int digit;

		if (byte < '0' || byte > '9')
			return false;
		digit = byte - '0';
		if (value > maximum / 10 ||
		    (value == maximum / 10 && digit > maximum % 10))
			return false;
		value = value * 10 + digit;
	}
	*result = value;
	return true;
}

static bool parse_integer(scale_token token, int *result)
{
	bool negative = token.length > 0 && token.text[0] == '-';
	uint64_t value;

	if (negative)
	{
		token.text++;
		token.length--;
	}
	if (!parse_unsigned(token, (uint64_t)INT_MAX + negative, &value))
		return false;
	*result = negative ? (int)(-(int64_t)value) : (int)value;
	return true;
}

static bool parse_name(scale_token token, char *name)
{
	if (token.length == 0 || token.length % 2 != 0 ||
	    token.length > 2 * MODULE_UI_SCALE_NAME_MAX)
		return false;
	for (size_t i = 0; i < token.length / 2; i++)
	{
		int high = hex_digit(token.text[2 * i]);
		int low = hex_digit(token.text[2 * i + 1]);

		if (high < 0 || low < 0 || (high == 0 && low == 0))
			return false;
		name[i] = (char)(high * 16 + low);
	}
	name[token.length / 2] = '\0';
	return true;
}

static bool snapshots_equal(const module_ui_scale_snapshot *left,
	const module_ui_scale_snapshot *right)
{
	if (left->epoch != right->epoch || left->count != right->count ||
	    left->root_fallback != right->root_fallback)
		return false;
	for (unsigned int i = 0; i < left->count; i++)
	{
		const module_ui_scale_row *a = &left->rows[i];
		const module_ui_scale_row *b = &right->rows[i];

		if (a->name_atom != b->name_atom ||
		    a->quarters != b->quarters ||
		    a->primary != b->primary || a->rect.x != b->rect.x ||
		    a->rect.y != b->rect.y || a->rect.width != b->rect.width ||
		    a->rect.height != b->rect.height ||
		    strcmp(a->name, b->name))
			return false;
	}
	return true;
}

void module_ui_scale_abort(module_ui_scale_receiver *state)
{
	if (state != NULL)
	{
		state->pending = false;
		state->received = 0;
		memset(&state->staging, 0, sizeof(state->staging));
	}
}

module_ui_scale_status module_ui_scale_receive(module_ui_scale_receiver *state,
	const char *text)
{
	scale_token tokens[13];
	size_t count = 0, length;
	uint64_t epoch, number, atom, quarters, primary;
	module_ui_scale_row row = {0};

	if (state == NULL || text == NULL)
		return MODULE_UI_SCALE_REFUSED;
	if (strncmp(text, "MonitorUI", 9) != 0 ||
	    (text[9] != '\0' && !isspace((unsigned char)text[9])))
		return MODULE_UI_SCALE_IGNORED;
	length = strnlen(text, MODULE_UI_SCALE_LINE_MAX);
	if (length >= MODULE_UI_SCALE_LINE_MAX)
		goto refused;
	while (*text != '\0')
	{
		const char *start;

		while (isspace((unsigned char)*text))
			text++;
		if (*text == '\0')
			break;
		if (count == sizeof(tokens) / sizeof(tokens[0]))
			goto refused;
		start = text;
		while (*text && !isspace((unsigned char)*text))
			text++;
		tokens[count++] = (scale_token){start, text - start};
	}
	if (count < 4 || !token_is(tokens[1], "1") ||
	    !parse_epoch(tokens[3], &epoch))
		goto refused;
	/* Delayed older records cannot poison a newer in-flight snapshot. */
	if (epoch < state->published.epoch ||
	    (state->pending && epoch < state->staging.epoch))
		return MODULE_UI_SCALE_REFUSED;
	if (token_is(tokens[2], "BEGIN"))
	{
		if (count != 6 ||
		    !parse_unsigned(tokens[4], MODULE_UI_SCALE_MAX, &number) ||
		    number == 0 || !parse_unsigned(tokens[5], 1, &primary))
			goto refused;
		module_ui_scale_abort(state);
		state->staging.epoch = epoch;
		state->staging.count = number;
		state->staging.root_fallback = primary;
		state->pending = true;
		return MODULE_UI_SCALE_STAGED;
	}
	if (!state->pending || epoch != state->staging.epoch)
		return MODULE_UI_SCALE_REFUSED;
	if (token_is(tokens[2], "ROW"))
	{
		if (count != 13 ||
		    !parse_unsigned(tokens[4], MODULE_UI_SCALE_MAX - 1,
			&number) || number != state->received ||
		    number >= state->staging.count ||
		    !parse_unsigned(tokens[5], UINT32_MAX, &atom) ||
		    !parse_unsigned(tokens[6], MONITOR_SCALE_QUARTERS_MAX,
			&quarters) || !parse_unsigned(tokens[7], 1, &primary) ||
		    !parse_integer(tokens[8], &row.rect.x) ||
		    !parse_integer(tokens[9], &row.rect.y) ||
		    !parse_integer(tokens[10], &row.rect.width) ||
		    !parse_integer(tokens[11], &row.rect.height) ||
		    !parse_name(tokens[12], row.name))
			goto refused;
		row.name_atom = atom;
		row.quarters = quarters;
		row.primary = primary;
		if (!row_valid(&row))
			goto refused;
		for (unsigned int i = 0; i < state->received; i++)
			if (row.name_atom == state->staging.rows[i].name_atom ||
			    strcmp(row.name, state->staging.rows[i].name) == 0)
				goto refused;
		state->staging.rows[state->received++] = row;
		return MODULE_UI_SCALE_STAGED;
	}
	if (!token_is(tokens[2], "END") || count != 5 ||
	    !parse_unsigned(tokens[4], MODULE_UI_SCALE_MAX, &number) ||
	    number != state->staging.count || number != state->received ||
	    !module_ui_scale_validate(&state->staging))
		goto refused;
	if (epoch == state->published.epoch)
	{
		bool equal = snapshots_equal(&state->published,
			&state->staging);

		module_ui_scale_abort(state);
		return equal ? MODULE_UI_SCALE_UNCHANGED :
			MODULE_UI_SCALE_REFUSED;
	}
	state->published = state->staging;
	module_ui_scale_abort(state);
	return MODULE_UI_SCALE_PUBLISHED;
refused:
	module_ui_scale_abort(state);
	return MODULE_UI_SCALE_REFUSED;
}

const module_ui_scale_row *module_ui_scale_select(
	const module_ui_scale_snapshot *snapshot, const rectangle *window,
	Atom incumbent_atom)
{
	monitor_topology_monitor monitors[MODULE_UI_SCALE_MAX] = {{0}};
	monitor_ownership_result owner;

	if (!module_ui_scale_validate(snapshot) || window == NULL ||
	    window->width <= 0 || window->height <= 0 ||
	    window->x > INT_MAX - window->width ||
	    window->y > INT_MAX - window->height)
		return NULL;
	for (unsigned int i = 0; i < snapshot->count; i++)
	{
		const module_ui_scale_row *row = &snapshot->rows[i];

		monitors[i] = (monitor_topology_monitor){
			.name_atom = row->name_atom, .name = (char *)row->name,
			.x = row->rect.x, .y = row->rect.y,
			.width = row->rect.width, .height = row->rect.height,
			.primary = row->primary
		};
	}
	if (monitor_ownership_resolve(window, monitors, snapshot->count,
		incumbent_atom, MONITOR_OWNERSHIP_HYSTERESIS_PX, &owner) !=
	    MONITOR_OWNERSHIP_OK)
		return NULL;
	for (unsigned int i = 0; i < snapshot->count; i++)
		if (snapshot->rows[i].name_atom == owner.name_atom)
			return &snapshot->rows[i];
	return NULL;
}
