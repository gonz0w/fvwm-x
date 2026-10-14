/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "form_input.h"
#include "libs/Module.h"
#include "libs/safemalloc.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool form_append_text(char **owned, const char *tail, size_t limit)
{
	size_t length = strlen(*owned), added = strlen(tail);
	char *copy;

	if (length > limit || added > limit - length || length + added == SIZE_MAX)
		return false;
	copy = fxmalloc(length + added + 1);
	memcpy(copy, *owned, length);
	memcpy(copy + length, tail, added + 1);
	free(*owned);
	*owned = copy;
	return true;
}

bool form_input_width(const char *text, int *width)
{
	long value;

	*width = 0;
	errno = 0;
	value = strtol(text, NULL, 10);
	/* Retain decimal-prefix parsing; nonpositive widths cannot be edited. */
	if (errno == ERANGE || value <= 0 || value > INT_MAX)
	{
		return false;
	}
	*width = (int)value;
	return true;
}

bool form_input_geometry(int width, int font_height, int horizontal_padding,
	int vertical_padding, int *size_x, int *size_y)
{
	int64_t x, y;

	*size_x = *size_y = 0;
	if (width <= 0 || font_height <= 0)
	{
		return false;
	}
	x = (int64_t)font_height * width / 2 + horizontal_padding;
	y = (int64_t)font_height + vertical_padding;
	if (x < 1 || x > UINT16_MAX || y < 1 || y > UINT16_MAX)
	{
		return false;
	}
	*size_x = (int)x;
	*size_y = (int)y;
	return true;
}

bool form_array_capacity(int current, int used, int spare, int initial,
	size_t element_size, int *capacity)
{
	size_t limit, required, next;

	*capacity = 0;
	if (current < 0 || used < 0 || spare <= 0 || initial <= 0 ||
		element_size == 0)
	{
		return false;
	}
	limit = SIZE_MAX / element_size;
	if (limit > INT_MAX)
	{
		limit = INT_MAX;
	}
	required = (size_t)used + (size_t)spare;
	if ((size_t)current > limit || required > limit)
	{
		return false;
	}
	next = current != 0 ? (size_t)current : (size_t)initial;
	if (next > limit)
	{
		next = limit;
	}
	while (next < required)
	{
		next = next > limit / 2 ? limit : next * 2;
	}
	*capacity = (int)next;
	return true;
}

bool form_packet_is_valid(unsigned long type, const unsigned long *body,
	size_t words)
{
	if (words > FvwmPacketBodyMaxSize || (words != 0 && body == NULL))
	{
		return false;
	}
	switch (type)
	{
	case M_CONFIG_INFO:
	case M_STRING:
	case M_ERROR:
		return words > 3 && memchr(body + 3, '\0',
			(words - 3) * sizeof(*body)) != NULL;
	case MX_PROPERTY_CHANGE:
		return words >= 3;
	default:
		return true;
	}
}

bool form_chomp_line(char *line)
{
	size_t length = strlen(line);

	if (length != 0 && line[length - 1] == '\n')
	{
		line[--length] = '\0';
	}
	return length != 0;
}

static size_t form_copy_source_length(const char *text, bool quoted)
{
	const char *end = text;

	while (*end != '\0')
	{
		if (quoted ? (*end == '\n' || *end == '"') :
			isspace((unsigned char)*end))
		{
			break;
		}
		if (*end == '\\' && end[1] != '\0')
		{
			end++;
		}
		end++;
	}
	return end - text;
}

bool form_parse_data(const char *text, form_data_args *args)
{
	form_data_args candidate = {0};

	*args = (form_data_args){0};
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (*text == '\0')
	{
		return false;
	}
	candidate.filename = text;
	text += form_copy_source_length(text, false);
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (*text == '\0')
	{
		return false;
	}
	candidate.prefix = text;
	*args = candidate;
	return true;
}

bool form_parse_input(const char *text, form_input_args *args)
{
	form_input_args candidate = {0};

	*args = (form_input_args){0};
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	candidate.name = text;
	text += form_copy_source_length(text, false);
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (!form_input_width(text, &candidate.width))
	{
		return false;
	}
	candidate.initial = form_next_argument(text);
	*args = candidate;
	return true;
}

char *form_copy_quoted(const char *text)
{
	char *copy = fxmalloc(form_copy_source_length(text, true) + 1);
	char *output = copy;

	while (*text != '\0' && *text != '\n' && *text != '"')
	{
		if (*text == '\\' && text[1] != '\0')
		{
			text++;
		}
		*output++ = *text++;
	}
	*output = '\0';
	return copy;
}

char *form_copy_token(const char *text)
{
	char *copy = fxmalloc(form_copy_source_length(text, false) + 1);
	char *output = copy;

	while (*text != '\0' && !isspace((unsigned char)*text))
	{
		if (*text == '\\' && text[1] != '\0')
		{
			*output++ = *text++;
		}
		*output++ = *text++;
	}
	*output = '\0';
	return copy;
}

static const char *form_quoted_end(const char *text)
{
	while (*text != '\0' && *text != '\n' && *text != '"')
	{
		if (*text == '\\' && text[1] != '\0')
		{
			text++;
		}
		text++;
	}
	return *text == '"' ? text : NULL;
}

const char *form_next_argument(const char *text)
{
	while (*text != '\0' && !isspace((unsigned char)*text))
	{
		text++;
	}
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	return text;
}

bool form_parse_button(const char *text, form_button_args *args)
{
	form_button_args candidate = {0};
	const char *end;

	*args = (form_button_args){0};
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (*text == '\0')
	{
		return false;
	}
	candidate.mode = text;
	text = form_next_argument(text);
	candidate.quoted_text = *text == '"';
	candidate.text = candidate.quoted_text ? text + 1 : text;
	if (candidate.quoted_text)
	{
		end = form_quoted_end(candidate.text);
		if (end == NULL)
		{
			return false;
		}
		text = end + 1;
		while (isspace((unsigned char)*text))
		{
			text++;
		}
	}
	/* An omitted label remains empty, including the legacy key-only form. */
	candidate.key = text;
	*args = candidate;
	return true;
}

int form_readiness_timeout(bool queued_x, bool timer_pending)
{
	return queued_x ? 0 : timer_pending ? 100 : -1;
}

bool form_x_service_available(unsigned int serviced,
	const struct timespec *started, const struct timespec *now)
{
	return module_x_service_available(serviced, started, now);
}

bool form_history_position(char *const *values, int slots, int current,
	int direction, int *position)
{
	int candidate;

	*position = -1;
	if (values == NULL || slots <= 0 || current < 0 || current > slots ||
		(direction != -1 && direction != 1))
	{
		return false;
	}
	/* Avoid adding one to an unrepresentable or already exhausted cursor. */
	candidate = direction < 0 ? current - 1 :
		current < slots - 1 ? current + 1 : slots;
	if (candidate >= 0 && candidate < slots && values[candidate] != NULL)
	{
		*position = candidate;
		return true;
	}
	if (direction < 0)
	{
		for (candidate = slots - 1; candidate >= 0; candidate--)
		{
			if (values[candidate] != NULL)
			{
				*position = candidate;
				return true;
			}
		}
	}
	else if (values[0] != NULL)
	{
		*position = 0;
		return true;
	}
	return false;
}

bool form_input_growth(int capacity, int bytes, int characters,
	size_t added_bytes, int added_characters, int *new_capacity,
	int *new_bytes, int *new_characters)
{
	int total;

	*new_capacity = *new_bytes = *new_characters = 0;
	if (capacity <= 0 || bytes < 0 || bytes >= capacity || characters < 0 ||
		added_characters <= 0 || added_bytes == 0 ||
		added_bytes > (size_t)(INT_MAX - 1 - bytes) ||
		added_characters > INT_MAX - characters)
	{
		return false;
	}
	total = bytes + (int)added_bytes;
	if (!form_array_capacity(capacity, total, 1, 32, sizeof(char),
		new_capacity))
	{
		return false;
	}
	*new_bytes = total;
	*new_characters = characters + added_characters;
	return true;
}

bool form_parse_timeout(const char *text, form_timeout_args *args)
{
	form_timeout_args candidate = {0};
	long ticks;
	const char *end;

	*args = (form_timeout_args){0};
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (*text == '\0')
	{
		return false;
	}
	/* Preserve decimal-prefix conversion and the existing 0..99999 range. */
	ticks = strtol(text, NULL, 10);
	candidate.ticks = ticks < 0 ? 0 : ticks > 99999 ? 99999 : (int)ticks;
	while (*text != '\0' && !isspace((unsigned char)*text))
	{
		text++;
	}
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (*text == '\0')
	{
		return false;
	}
	candidate.quoted_command = *text == '"';
	candidate.command = candidate.quoted_command ? ++text : text;
	if (candidate.quoted_command)
	{
		end = form_quoted_end(text);
		if (end == NULL)
		{
			return false;
		}
		candidate.command_length = end - text;
		text = end + 1;
	}
	else
	{
		while (*text != '\0' && !isspace((unsigned char)*text))
		{
			text++;
		}
		candidate.command_length = text - candidate.command;
	}
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (*text == '\0')
	{
		return false;
	}
	candidate.quoted_text = *text == '"';
	candidate.text = candidate.quoted_text ? text + 1 : text;
	if (candidate.quoted_text && form_quoted_end(candidate.text) == NULL)
	{
		return false;
	}
	*args = candidate;
	return true;
}
