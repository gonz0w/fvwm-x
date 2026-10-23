/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include <string.h>
#include <ctype.h>
#include "form_list.h"

bool form_list_parse_icon(const char *text, enum form_list_icon *icon)
{
	static const char *const names[] = {"", "file", "directory", "link", "other"};
	const char *end;
	size_t length;

	*icon = FORM_ICON_NONE;
	while (isspace((unsigned char)*text))
		text++;
	end = text;
	while (*end != '\0' && !isspace((unsigned char)*end))
		end++;
	length = end - text;
	while (isspace((unsigned char)*end))
		end++;
	if (*end != '\0')
		return false;
	for (int i = FORM_ICON_NONE; i <= FORM_ICON_OTHER; i++)
	{
		if (strlen(names[i]) == length && memcmp(names[i], text, length) == 0)
		{
			*icon = i;
			return true;
		}
	}
	return false;
}

bool form_list_number(const char *text, int maximum, int *value)
{
	int n = 0;

	*value = 0;
	if (*text < '0' || *text > '9')
		return false;
	for (; *text >= '0' && *text <= '9'; text++)
	{
		int digit = *text - '0';
		if (n > maximum / 10 ||
		    (n == maximum / 10 && digit > maximum % 10))
			return false;
		n = n * 10 + digit;
	}
	if (n == 0 || (*text != '\0' && !isspace((unsigned char)*text)))
		return false;
	*value = n;
	return true;
}

bool form_list_add(form_list_state *s, bool selected)
{
	if (s->refused || s->count == FORM_LIST_MAX ||
	    (selected && s->selected == FORM_LIST_SELECTED))
	{
		s->refused = true;
		return false;
	}
	if (selected)
	{
		s->bits[s->count / 8] |= 1u << (s->count % 8);
		s->selected++;
	}
	s->count++;
	return true;
}

bool form_list_toggle(form_list_state *s, int index)
{
	unsigned char mask;

	if (s->refused || index < 0 || index >= s->count)
		return false;
	mask = 1u << (index % 8);
	if (s->bits[index / 8] & mask)
		s->selected--;
	else if (s->selected == FORM_LIST_SELECTED)
		return false;
	else
		s->selected++;
	s->bits[index / 8] ^= mask;
	return true;
}

/* Zero-initialized callers retain the original one-column layout. */
int form_list_columns(const form_list_state *s)
{
	return s->columns > 0 ? s->columns : 1;
}

int form_list_lines(const form_list_state *s)
{
	int columns = form_list_columns(s);

	return (s->count + columns - 1) / columns;
}

void form_list_scroll(form_list_state *s, int amount)
{
	int columns = form_list_columns(s);
	int top = s->top / columns;
	int last = form_list_lines(s) > s->rows ?
		form_list_lines(s) - s->rows : 0;

	/* Scroll in visual rows; clamp before adding even INT_MIN/INT_MAX. */
	if (amount < -top)
		top = 0;
	else if (amount > last - top)
		top = last;
	else
		top += amount;
	s->top = top * columns;
}

void form_list_focus(form_list_state *s, int index)
{
	int columns = form_list_columns(s);
	int row, top = s->top / columns;

	if (s->count == 0)
	{
		s->focus = s->anchor = s->top = 0;
		return;
	}
	s->focus = index < 0 ? 0 : index >= s->count ? s->count - 1 : index;
	s->anchor = s->focus;
	row = s->focus / columns;
	if (row < top)
		top = row;
	if (row >= top + s->rows)
		top = row - s->rows + 1;
	s->top = top * columns;
	form_list_scroll(s, 0);
}

/* Shift extends additively from the last ordinary focus position. Selection
 * and focus publish together; an over-budget range cannot partially change it. */
bool form_list_extend(form_list_state *s, int index)
{
	form_list_state candidate = *s;
	int first, last;

	if (s->refused || s->count == 0)
		return false;
	form_list_focus(&candidate, index);
	candidate.anchor = s->anchor;
	first = candidate.focus < s->anchor ? candidate.focus : s->anchor;
	last = candidate.focus > s->anchor ? candidate.focus : s->anchor;
	for (int i = first; i <= last; i++)
	{
		unsigned char mask = 1u << (i % 8);

		if (candidate.bits[i / 8] & mask)
			continue;
		if (candidate.selected == FORM_LIST_SELECTED)
			return false;
		candidate.bits[i / 8] |= mask;
		candidate.selected++;
	}
	*s = candidate;
	return true;
}

size_t form_list_packet(const form_list_state *s,
	unsigned char packet[FORM_LIST_BYTES + 2])
{
	size_t bytes = (s->count + 7) / 8;

	if (s->refused)
		return 0;
	/* Two-byte big-endian focus, then little-bit-first selection by row. */
	packet[0] = s->focus >> 8;
	packet[1] = s->focus & 255;
	memcpy(packet + 2, s->bits, bytes);
	return bytes + 2;
}
