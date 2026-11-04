/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "buttons_layout_eval.h"
#include "FvwmButtons.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef FVWM_LAYOUT_EVAL_CLAY
#include "libs/clay_layout_eval.h"
#endif

static struct
{
	bool initialized, registered, trace, refuse, saturated, poisoned;
	bool pending_valid;
	uint64_t calls, native, clay, fallback, errors, refused;
	uint32_t metadata, pending_metadata;
	size_t arena_bytes;
#ifdef FVWM_LAYOUT_EVAL_CLAY
	clay_layout_eval layout;
#endif
} buttons_layout_owner;

static void buttons_layout_observe_count(uint64_t *counter)
{
	if (!buttons_layout_owner.trace)
		return;
	if (*counter == UINT64_MAX)
		buttons_layout_owner.saturated = true;
	else
		(*counter)++;
}

static void buttons_layout_retire(void)
{
	char record[1024];
	int length;
	ssize_t written;
	bool complete = !buttons_layout_owner.saturated &&
		!buttons_layout_owner.poisoned;
#ifdef FVWM_LAYOUT_EVAL_CLAY
	const char *backend = "clay";

	complete = complete && !buttons_layout_owner.layout.failed;
	clay_layout_eval_destroy(&buttons_layout_owner.layout);
#else
	const char *backend = "native";
#endif

	if (!buttons_layout_owner.trace)
		return;
	/* All fields are copied values. Retirement resolves no widget and makes
	 * no X call; short/failed output deliberately has no complete record. */
	length = snprintf(record, sizeof(record),
		"{\"layout_eval\":1,\"pid\":%ld,\"backend\":\"%s\","
		"\"calls\":%" PRIu64 ",\"native\":%" PRIu64 ","
		"\"clay\":%" PRIu64 ",\"fallback\":%" PRIu64 ","
		"\"errors\":%" PRIu64 ",\"arena_bytes\":%zu,"
		"\"refused\":%" PRIu64 ",\"metadata\":%" PRIu32 ","
		"\"complete\":%d}\n",
		(long)getpid(), backend, buttons_layout_owner.calls,
		buttons_layout_owner.native, buttons_layout_owner.clay,
		buttons_layout_owner.fallback, buttons_layout_owner.errors,
		buttons_layout_owner.arena_bytes, buttons_layout_owner.refused,
		buttons_layout_owner.metadata,
		complete ? 1 : 0);
	if (length <= 0 || (size_t)length >= sizeof(record))
		return;
	written = write(STDERR_FILENO, record, (size_t)length);
	if (written != (ssize_t)length)
		return;
}

static void buttons_layout_initialize(void)
{
	const char *trace;

	if (buttons_layout_owner.initialized)
		return;
	buttons_layout_owner.initialized = true;
	trace = getenv("FVWM_LAYOUT_EVAL_TRACE");
	buttons_layout_owner.trace = trace && strcmp(trace, "1") == 0;
	buttons_layout_owner.refuse = getenv("FVWM_LAYOUT_EVAL_REFUSE") != NULL;
	/* No arena is acquired until its single retirement owner is registered. */
	buttons_layout_owner.registered = atexit(buttons_layout_retire) == 0;
	if (!buttons_layout_owner.registered)
	{
		buttons_layout_owner.poisoned = true;
		return;
	}
#ifdef FVWM_LAYOUT_EVAL_CLAY
	if (!buttons_layout_owner.refuse &&
		!clay_layout_eval_init(&buttons_layout_owner.layout,
			CLAY_LAYOUT_EVAL_ARENA_LIMIT))
	{
		buttons_layout_owner.poisoned = true;
		buttons_layout_observe_count(&buttons_layout_owner.errors);
	}
	buttons_layout_owner.arena_bytes =
		buttons_layout_owner.layout.arena_bytes;
#endif
}

static void buttons_layout_metadata_byte(uint32_t *hash, unsigned char octet)
{
	*hash = (*hash ^ octet) * UINT32_C(16777619);
}

static bool buttons_layout_metadata_title(uint32_t *hash, const char *title)
{
	buttons_layout_metadata_byte(hash, title ? 1 : 0);
	if (!title)
		return true;
	/* Include the terminator within the fixed scan budget. This is a private
	 * evidence witness; longer valid titles retain native behavior but cannot
	 * qualify this observation. No title pointer survives this call. */
	for (size_t i = 0; i < 4096; i++)
	{
		unsigned char octet = (unsigned char)title[i];

		buttons_layout_metadata_byte(hash, octet);
		if (!octet)
			return true;
	}
	return false;
}

void buttons_layout_eval_metadata(struct button_info_struct *const *buttons,
	size_t count)
{
	uint32_t hash = UINT32_C(2166136261);

	buttons_layout_initialize();
	buttons_layout_owner.pending_valid = false;
	if (!buttons_layout_owner.trace)
		return;
	if (!buttons || !count || count > UI_LAYOUT_MAX_ITEMS)
		goto invalid;
	for (unsigned i = 0; i < 4; i++)
		buttons_layout_metadata_byte(&hash,
			(unsigned char)((uint32_t)count >> (8 * i)));
	for (size_t i = 0; i < count; i++)
	{
		const button_info *button = buttons[i];

		if (!button ||
			!buttons_layout_metadata_title(&hash, button->title) ||
			!buttons_layout_metadata_title(&hash, button->activeTitle) ||
			!buttons_layout_metadata_title(&hash, button->pressTitle))
			goto invalid;
	}
	buttons_layout_owner.pending_metadata = hash;
	buttons_layout_owner.pending_valid = true;
	return;

invalid:
	buttons_layout_owner.poisoned = true;
}

bool buttons_layout_eval_row(int available, int height, const int *minimum,
	const unsigned *weights, size_t count, int *widths, bool *clipped)
{
	bool native;
	uint32_t metadata;
	bool observed;

	buttons_layout_initialize();
	metadata = buttons_layout_owner.pending_metadata;
	observed = buttons_layout_owner.pending_valid;
	buttons_layout_owner.pending_valid = false;
	if (buttons_layout_owner.trace && !observed)
		buttons_layout_owner.poisoned = true;
	buttons_layout_observe_count(&buttons_layout_owner.calls);
#ifdef FVWM_LAYOUT_EVAL_CLAY
	if (buttons_layout_owner.registered &&
		!buttons_layout_owner.poisoned && !buttons_layout_owner.refuse &&
		clipped)
	{
		enum clay_layout_eval_result result = clay_layout_eval_row(
			&buttons_layout_owner.layout, available, height,
			minimum, weights, count, widths);

		if (result == CLAY_LAYOUT_EVAL_READY)
		{
			*clipped = false;
			buttons_layout_observe_count(&buttons_layout_owner.clay);
			if (observed)
				buttons_layout_owner.metadata = metadata;
			return true;
		}
		if (result == CLAY_LAYOUT_EVAL_ERROR)
		{
			buttons_layout_owner.poisoned = true;
			buttons_layout_observe_count(&buttons_layout_owner.errors);
		}
		else
			buttons_layout_observe_count(&buttons_layout_owner.refused);
	}
	else if (buttons_layout_owner.refuse)
		buttons_layout_observe_count(&buttons_layout_owner.refused);
	buttons_layout_observe_count(&buttons_layout_owner.fallback);
#else
	(void)height;
#endif
	/* The existing policy remains the single fallback and keeps its own
	 * invalid-input/output contract. Publication stays in button.c. */
	native = ui_layout_row(available, minimum, weights, count, widths, clipped);
	if (native)
	{
		buttons_layout_observe_count(&buttons_layout_owner.native);
		if (observed)
			buttons_layout_owner.metadata = metadata;
	}
	return native;
}
