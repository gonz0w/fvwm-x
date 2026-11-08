/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "theme_origin.h"
#include "json_print_policy.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Diagnostic retention only. The single core thread owns all operations. */
static size_t theme_origin_bytes;

size_t fvwm_theme_origin_retained_bytes(void)
{
	return theme_origin_bytes;
}

void fvwm_theme_origin_clear(fvwm_theme_origin *slot)
{
	assert(slot->charge <= theme_origin_bytes);
	theme_origin_bytes -= slot->charge;
	fvwm_config_origin_release(slot->source);
	free(slot->copied_from);
	*slot = (fvwm_theme_origin)FVWM_THEME_ORIGIN_INIT;
}

static bool donor_size(const char *donor, size_t *size)
{
	size_t length = 0;

	if (!donor || !*donor)
		return false;
	while (donor[length])
	{
		const unsigned char *text =
			(const unsigned char *)donor + length;
		size_t step = json_utf8_sequence_length(text);

		if (!step || text[0] < 0x20 || text[0] == 0x7f ||
		    (text[0] == 0xc2 && text[1] < 0xa0) ||
		    step > FVWM_THEME_DONOR_MAX_BYTES - length)
			return false;
		length += step;
	}
	*size = length + 1;
	return true;
}

static void retain_metadata(fvwm_theme_origin *next,
	const fvwm_config_origin *source, const char *donor)
{
	size_t donor_bytes = 0;
	size_t charge = fvwm_config_origin_storage_bytes(source);

	next->retention = FVWM_THEME_UNAVAILABLE;
	if (next->operation == FVWM_THEME_COPY &&
	    !donor_size(donor, &donor_bytes))
		return;
	if (charge > FVWM_THEME_ORIGIN_MAX_BYTES - theme_origin_bytes ||
	    donor_bytes > FVWM_THEME_ORIGIN_MAX_BYTES - theme_origin_bytes -
		charge)
	{
		next->retention = FVWM_THEME_BUDGET_EXHAUSTED;
		return;
	}
	if (donor_bytes)
	{
		next->copied_from = malloc(donor_bytes);
		if (!next->copied_from)
			return;
		memcpy(next->copied_from, donor, donor_bytes);
	}
	next->source = fvwm_config_origin_retain(source);
	next->charge = charge + donor_bytes;
	theme_origin_bytes += next->charge;
	next->retention = source ? FVWM_THEME_RETAINED : FVWM_THEME_UNAVAILABLE;
}

void fvwm_theme_origin_definition(fvwm_theme_origin *slot,
	const fvwm_config_origin *source, fvwm_theme_operation operation,
	const char *donor)
{
	fvwm_theme_origin next = FVWM_THEME_ORIGIN_INIT;

	assert(operation == FVWM_THEME_MERGE || operation == FVWM_THEME_COPY ||
		operation == FVWM_THEME_RESET);
	next.operation = operation;
	next.replaces_definition =
		slot->operation != FVWM_THEME_BUILTIN_DEFAULT;
	/* Retain before releasing: source and donor may belong to slot. */
	retain_metadata(&next, source, donor);
	fvwm_theme_origin_clear(slot);
	*slot = next;
}

void fvwm_theme_origin_clone(fvwm_theme_origin *slot,
	const fvwm_theme_origin *origin)
{
	fvwm_theme_origin next = *origin;

	if (slot == origin)
		return;
	next.source = NULL;
	next.copied_from = NULL;
	next.charge = 0;
	if (origin->source || origin->copied_from)
		retain_metadata(&next, origin->source, origin->copied_from);
	fvwm_theme_origin_clear(slot);
	*slot = next;
}

void fvwm_theme_origin_move(fvwm_theme_origin *slot, fvwm_theme_origin *origin)
{
	if (slot == origin)
		return;
	fvwm_theme_origin_clear(slot);
	*slot = *origin;
	*origin = (fvwm_theme_origin)FVWM_THEME_ORIGIN_INIT;
}

void fvwm_theme_origin_preview(fvwm_theme_origin *slot)
{
	bool replaces = slot->operation != FVWM_THEME_BUILTIN_DEFAULT;

	fvwm_theme_origin_clear(slot);
	slot->operation = FVWM_THEME_PREVIEW;
	slot->replaces_definition = replaces;
}
