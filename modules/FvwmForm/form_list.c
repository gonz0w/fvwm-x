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

/* An explicit owner focus request never clamps stale/out-of-range ordinals. */
bool form_list_focus_version(form_list_state *s, int generation, int index)
{
	if (generation <= 0 || generation != s->generation || s->refused ||
	    index < 0 || index >= s->count)
		return false;
	form_list_focus(s, index);
	return true;
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
	unsigned char packet[FORM_LIST_PACKET_MAX])
{
	size_t bytes = (s->count + 7) / 8;
	size_t prefix = s->generation != 0 ? 4 : 0;

	if (s->refused)
		return 0;
	/* Opt-in generation and focus are big-endian; selection is bit-first. */
	if (prefix)
	{
		unsigned int generation = s->generation;
		for (int i = 3; i >= 0; i--, generation >>= 8)
			packet[i] = generation & 255;
	}
	packet += prefix;
	packet[0] = s->focus >> 8;
	packet[1] = s->focus & 255;
	memcpy(packet + 2, s->bits, bytes);
	return prefix + bytes + 2;
}


bool form_list_update_begin(form_list_update *u, const form_list_state *s,
	int expected, int generation)
{
	if (s->refused || s->generation <= 0 || expected != s->generation ||
	    generation <= expected)
		return false;
	memset(u, 0, sizeof(*u));
	u->expected = expected;
	u->generation = generation;
	u->source_count = s->count;
	return true;
}

bool form_list_update_add(form_list_update *u, int source)
{
	return form_list_update_add_choice(u, source, FORM_LIST_KEEP);
}

bool form_list_update_add_choice(form_list_update *u, int source,
	enum form_list_choice choice)
{
	if (u->refused || u->generation <= 0 || u->count == FORM_LIST_MAX ||
	    source < 0 || source > u->source_count ||
	    choice < FORM_LIST_KEEP || choice > FORM_LIST_OFF ||
	    (source && (u->used[(source - 1) / 8] &
	      (1u << ((source - 1) % 8)))))
	{
		u->refused = true;
		return false;
	}
	if (source)
		u->used[(source - 1) / 8] |= 1u << ((source - 1) % 8);
	u->choices[u->count] = choice;
	u->sources[u->count++] = source;
	return true;
}

/* Prefer the surviving old position, then its next neighbor, then previous.
 * New entries are a last resort; labels never establish identity. */
static int form_list_update_position(const int *inverse, int old, int count)
{
	for (int i = old; i < count; i++)
		if (inverse[i] >= 0)
			return inverse[i];
	for (int i = old - 1; i >= 0; i--)
		if (inverse[i] >= 0)
			return inverse[i];
	return 0;
}

bool form_list_update_commit(form_list_state *s, const form_list_update *u)
{
	form_list_state candidate = *s;
	int inverse[FORM_LIST_MAX];

	if (u->refused || s->refused || s->generation != u->expected ||
	    u->generation <= u->expected || s->count != u->source_count ||
	    u->count < 0 || u->count > FORM_LIST_MAX)
		return false;
	for (int i = 0; i < s->count; i++)
		inverse[i] = -1;
	memset(candidate.bits, 0, sizeof(candidate.bits));
	candidate.count = u->count;
	candidate.selected = 0;
	candidate.generation = u->generation;
	for (int i = 0; i < u->count; i++)
	{
		int source = u->sources[i] - 1;

		if (source >= 0)
			inverse[source] = i;
		if (u->choices[i] == FORM_LIST_ON ||
		    (u->choices[i] == FORM_LIST_KEEP && source >= 0 &&
		     (s->bits[source / 8] & (1u << (source % 8)))))
		{
			if (candidate.selected == FORM_LIST_SELECTED)
				return false;
			candidate.bits[i / 8] |= 1u << (i % 8);
			candidate.selected++;
		}
	}
	candidate.focus = form_list_update_position(inverse, s->focus, s->count);
	candidate.anchor = s->count && inverse[s->anchor] >= 0 ?
		inverse[s->anchor] : candidate.focus;
	candidate.top = form_list_update_position(inverse, s->top, s->count);
	candidate.top -= candidate.top % form_list_columns(&candidate);
	/* A scrolled-away focus stays scrolled away; do not steal the viewport. */
	form_list_scroll(&candidate, 0);
	*s = candidate;
	return true;
}


bool form_list_status_token(const char *text, char token[65], bool *selection)
{
	size_t n = 0;

	token[0] = '\0';
	*selection = false;
	while (text[n] && !isspace((unsigned char)text[n]))
	{
		unsigned char c = text[n];

		if (n == 64 || !((c >= 'a' && c <= 'z') ||
		    (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		    c == '-' || c == '_'))
			return false;
		n++;
	}
	if (n == 0)
		return false;
	const char *tail = text + n;
	while (isspace((unsigned char)*tail)) tail++;
	if (*tail)
	{
		if (strncmp(tail, "selection", 9) != 0 ||
		    (tail[9] && !isspace((unsigned char)tail[9])))
			return false;
		tail += 9;
		while (isspace((unsigned char)*tail)) tail++;
		if (*tail) return false;
		*selection = true;
	}
	memcpy(token, text, n);
	token[n] = '\0';
	return true;
}
