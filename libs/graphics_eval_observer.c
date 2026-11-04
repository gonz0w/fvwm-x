/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include "graphics_eval_observer.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void add(graphics_eval_observer *owner, uint64_t *value,
	uint64_t amount)
{
	if (UINT64_MAX - *value < amount)
	{
		*value = UINT64_MAX;
		owner->saturated = true;
	}
	else
		*value += amount;
}

void graphics_eval_observer_init(graphics_eval_observer *owner, bool enabled)
{
	if (owner)
	{
		memset(owner, 0, sizeof(*owner));
		owner->enabled = enabled;
	}
}

void graphics_eval_observer_count(graphics_eval_observer *owner,
	enum graphics_observer_counter counter, uint64_t amount)
{
	if (owner && owner->enabled && !owner->closed &&
	    counter >= 0 && counter < GRAPHICS_OBSERVER_COUNTER_COUNT)
		add(owner, &owner->counters[counter], amount);
}

static bool append(graphics_eval_observer *owner,
	graphics_eval_observation value)
{
	if (owner->sequence == UINT64_MAX)
	{
		owner->saturated = owner->overflow = true;
		return false;
	}
	value.sequence = ++owner->sequence;
	owner->records[owner->length++] = value;
	if (!value.clock_valid)
		owner->clock_failed = true;
	return true;
}

bool graphics_eval_observer_note(graphics_eval_observer *owner,
	graphics_eval_observation value)
{
	if (!owner || !owner->enabled || owner->closed)
		return false;
	if (owner->length >= GRAPHICS_OBSERVER_CAPACITY - 2 ||
	    owner->sequence == UINT64_MAX)
	{
		owner->overflow = true;
		add(owner, &owner->dropped, 1);
		if (owner->sequence == UINT64_MAX)
			owner->saturated = true;
		return false;
	}
	return append(owner, value);
}

bool graphics_eval_observer_finish(graphics_eval_observer *owner,
	graphics_eval_observation terminal)
{
	bool result = true;

	if (!owner || !owner->enabled || owner->closed)
		return false;
	if (owner->overflow)
	{
		graphics_eval_observation overflow = terminal;

		overflow.event = GRAPHICS_OBSERVER_OVERFLOW;
		result = append(owner, overflow);
	}
	terminal.event = GRAPHICS_OBSERVER_TERMINAL;
	result = append(owner, terminal) && result;
	owner->closed = true;
	return result;
}

bool graphics_eval_observer_complete(const graphics_eval_observer *owner)
{
	return owner && owner->enabled && owner->closed && owner->length &&
		owner->records[owner->length - 1].event ==
		GRAPHICS_OBSERVER_TERMINAL && !owner->overflow &&
		!owner->saturated && !owner->clock_failed;
}

/* Each finite line is submitted by one write to the owned harness log.
 * A partial/error write stops export, leaving no success trailer. */
static bool line(int descriptor, const char *format, ...)
{
	char buffer[1024];
	va_list args;
	int length;

	va_start(args, format);
	length = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	if (length < 0 || (size_t)length >= sizeof(buffer))
		return false;
	return write(descriptor, buffer, (size_t)length) == length;
}

bool graphics_eval_observer_export(graphics_eval_observer *owner,
	int descriptor, unsigned int module, bool cairo_backend)
{
	static const char *const counters[] = {
		"idle", "wait", "timeout_queries", "service_paints",
		"synchronous_draws", "service_draws", "issued", "paint_issued",
		"native_fallbacks"};
	const char *name = module ? "buttons" : "form";
	const char *backend = cairo_backend ? "cairo" : "native";
	long pid = (long)getpid();
	bool complete;

	if (!owner || !owner->enabled || owner->exported || module > 1)
		return false;
	owner->exported = true;
	complete = graphics_eval_observer_complete(owner);
#define PREFIX "{\"graphics_eval\":1,\"pid\":%ld,\"module\":\"%s\"," \
	"\"backend\":\"%s\","
#define KEY pid, name, backend
	if (!line(descriptor, PREFIX "\"type\":\"header\",\"capacity\":256}\n",
	    KEY))
		return false;
	for (size_t i = 0; i < owner->length; i++)
	{
		const graphics_eval_observation *r = &owner->records[i];

		if (!line(descriptor, PREFIX "\"type\":\"record\","
		    "\"seq\":%" PRIu64 ",\"ms\":%" PRIu64 ",\"clock\":%u,"
		    "\"event\":%d,\"reason\":%d,\"operation\":%d,"
		    "\"token\":%" PRIu64 ",\"next_token\":%" PRIu64 ","
		    "\"identity\":%" PRIu64 ",\"revision\":%" PRIu64 ","
		    "\"active_before\":%u,\"active_after\":%u,"
		    "\"service_paints\":%" PRIu64 ",\"timeout\":%d,"
		    "\"from_rgb\":%" PRIu32 ",\"to_rgb\":%" PRIu32 ","
		    "\"last_rgb\":%" PRIu32 ","
		    "\"issued\":%" PRIu64 ",\"paint_issued\":%" PRIu64 ","
		    "\"outcome\":%u,\"service\":%u}\n", KEY,
		    r->sequence, r->ms, (unsigned int)r->clock_valid,
		    r->event, r->reason, r->operation, r->token, r->next_token,
		    r->identity, r->revision, (unsigned int)r->active_before,
		    (unsigned int)r->active_after, r->service_paints,
		    r->timeout, r->from_rgb, r->to_rgb, r->last_rgb,
		    r->issued, r->paint_issued, r->outcome,
		    (unsigned int)r->service))
			return false;
	}
	for (unsigned int i = 0; i < GRAPHICS_OBSERVER_COUNTER_COUNT; i++)
		if (!line(descriptor, PREFIX "\"type\":\"counter\","
		    "\"name\":\"%s\",\"value\":%" PRIu64 "}\n", KEY,
		    counters[i], owner->counters[i]))
			return false;
#define EXPORT_STAT(field) \
	if (!line(descriptor, PREFIX "\"type\":\"stat\",\"name\":\"" \
	    #field "\",\"value\":%" PRIu64 "}\n", KEY, owner->stats.field)) \
		return false;
	GRAPHICS_OBSERVER_STATS(EXPORT_STAT)
#undef EXPORT_STAT
	return line(descriptor, PREFIX "\"type\":\"trailer\","
		"\"records\":%zu,\"sequence\":%" PRIu64 ","
		"\"dropped\":%" PRIu64 ",\"overflow\":%u,\"saturated\":%u,"
		"\"clock_failed\":%u,\"complete\":%u}\n", KEY,
		owner->length, owner->sequence, owner->dropped,
		(unsigned int)owner->overflow, (unsigned int)owner->saturated,
		(unsigned int)owner->clock_failed, (unsigned int)complete);
#undef KEY
#undef PREFIX
}
