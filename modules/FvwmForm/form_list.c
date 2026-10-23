/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include <string.h>
#include <ctype.h>
#include "form_list.h"

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

void form_list_scroll(form_list_state *s, int amount)
{
	int last = s->count > s->rows ? s->count - s->rows : 0;

	/* Clamp before addition, including adversarial INT_MIN/INT_MAX. */
	if (amount < -s->top)
		s->top = 0;
	else if (amount > last - s->top)
		s->top = last;
	else
		s->top += amount;
}

void form_list_focus(form_list_state *s, int index)
{
	if (s->count == 0)
	{
		s->focus = s->top = 0;
		return;
	}
	s->focus = index < 0 ? 0 : index >= s->count ? s->count - 1 : index;
	if (s->focus < s->top)
		s->top = s->focus;
	if (s->focus >= s->top + s->rows)
		s->top = s->focus - s->rows + 1;
	form_list_scroll(s, 0);
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
