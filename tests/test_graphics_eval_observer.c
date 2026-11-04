/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "libs/graphics_eval_observer.h"

static void disabled_and_copies(void)
{
	graphics_eval_observer observer;
	graphics_eval_observation value = {
		.event = GRAPHICS_OBSERVER_BEGIN, .ms = 10,
		.clock_valid = true, .token = 7, .identity = 42,
		.revision = 3, .active_after = true, .timeout = 15,
		.from_rgb = 0, .to_rgb = 0xffffff, .last_rgb = 0x808080};

	memset(&observer, 0xff, sizeof(observer));
	graphics_eval_observer_init(&observer, false);
	assert(!observer.enabled && !observer.length && !observer.closed);
	assert(!graphics_eval_observer_note(&observer, value));
	graphics_eval_observer_count(&observer, GRAPHICS_OBSERVER_IDLE, 1);
	assert(!observer.counters[GRAPHICS_OBSERVER_IDLE]);
	graphics_eval_observer_init(&observer, true);
	assert(graphics_eval_observer_note(&observer, value));
	value.identity = 99;
	value.to_rgb = value.last_rgb = 0;
	assert(observer.records[0].identity == 42);
	assert(observer.records[0].sequence == 1);
	assert(observer.records[0].active_after);
	assert(observer.records[0].timeout == 15);
	assert(!observer.records[0].active_before);
	assert(observer.records[0].from_rgb == 0);
	assert(observer.records[0].to_rgb == 0xffffff);
	assert(observer.records[0].last_rgb == 0x808080);
	assert(observer.records[0].from_rgb <= 0xffffff);
	assert(observer.records[0].to_rgb <= 0xffffff);
	assert(observer.records[0].last_rgb <= 0xffffff);
}

static void terminal_export(void)
{
	graphics_eval_observer observer;
	graphics_eval_transition state = {0};
	graphics_eval_observation value = {0};
	uint32_t rgb;
	char buffer[1024];
	bool found_terminal = false, found_trailer = false;
	FILE *output = tmpfile();

	assert(output);
	graphics_eval_observer_init(&observer, true);
	assert(graphics_eval_transition_begin(&state, 100, 1, 2, 0,
		0xffffff, true, false) == GRAPHICS_EVAL_START_ACTIVE);
	assert(graphics_eval_transition_sample(&state, state.token, 1, 2,
		220, true, &rgb) == GRAPHICS_EVAL_TICK_FINISH);
	value.event = GRAPHICS_OBSERVER_FINISH;
	value.clock_valid = true;
	value.ms = 220;
	value.from_rgb = state.from_rgb;
	value.to_rgb = state.to_rgb;
	value.last_rgb = state.last_rgb;
	value.service_paints = state.paints;
	assert(graphics_eval_observer_note(&observer, value));
	memset(&state, 0, sizeof(state));
	assert(observer.records[0].to_rgb == 0xffffff);
	assert(observer.records[0].last_rgb == 0xffffff);
	assert(observer.records[0].service_paints == 1);
	value.event = GRAPHICS_OBSERVER_TERMINAL;
	assert(graphics_eval_observer_finish(&observer, value));
	assert(graphics_eval_observer_export(&observer, fileno(output), 0,
		false));
	assert(!graphics_eval_observer_export(&observer, fileno(output), 0,
		false));
	rewind(output);
	while (fgets(buffer, sizeof(buffer), output))
	{
		assert(strchr(buffer, '\n'));
		assert(strstr(buffer, "\"graphics_eval\":1"));
		assert(strstr(buffer, "\"module\":\"form\""));
		assert(strstr(buffer, "\"backend\":\"native\""));
		if (strstr(buffer, "\"event\":7"))
			found_terminal = true;
		if (strstr(buffer, "\"type\":\"trailer\""))
		{
			assert(found_terminal);
			assert(strstr(buffer, "\"complete\":1"));
			found_trailer = true;
		}
	}
	assert(found_terminal && found_trailer);
	assert(fclose(output) == 0);
	graphics_eval_observer_init(&observer, true);
	assert(graphics_eval_observer_finish(&observer, value));
	assert(!graphics_eval_observer_export(&observer, -1, 1, true));
	assert(observer.exported);
}

static void bounds_and_terminal(void)
{
	graphics_eval_observer observer;
	graphics_eval_observation value = {
		.event = GRAPHICS_OBSERVER_DRAW, .clock_valid = true};

	graphics_eval_observer_init(&observer, true);
	for (unsigned int i = 0; i < GRAPHICS_OBSERVER_CAPACITY - 2; i++)
		assert(graphics_eval_observer_note(&observer, value));
	assert(!graphics_eval_observer_note(&observer, value));
	assert(observer.overflow && observer.dropped == 1);
	assert(!graphics_eval_observer_note(&observer, value));
	assert(observer.dropped == 2);
	value.event = GRAPHICS_OBSERVER_TERMINAL;
	value.active_before = true;
	value.active_after = false;
	assert(graphics_eval_observer_finish(&observer, value));
	assert(observer.length == GRAPHICS_OBSERVER_CAPACITY);
	assert(observer.records[observer.length - 2].event ==
		GRAPHICS_OBSERVER_OVERFLOW);
	assert(observer.records[observer.length - 1].event ==
		GRAPHICS_OBSERVER_TERMINAL);
	assert(observer.records[observer.length - 1].active_before);
	assert(!observer.records[observer.length - 1].active_after);
	assert(observer.closed && !graphics_eval_observer_complete(&observer));
	assert(!graphics_eval_observer_finish(&observer, value));
	assert(!graphics_eval_observer_note(&observer, value));
}

static void saturation_and_incomplete(void)
{
	graphics_eval_observer observer;
	graphics_eval_observation value = {
		.event = GRAPHICS_OBSERVER_TERMINAL, .clock_valid = true};

	graphics_eval_observer_init(&observer, true);
	assert(!graphics_eval_observer_complete(&observer));
	graphics_eval_observer_count(&observer, GRAPHICS_OBSERVER_WAIT,
		UINT64_MAX);
	graphics_eval_observer_count(&observer, GRAPHICS_OBSERVER_WAIT, 1);
	assert(observer.counters[GRAPHICS_OBSERVER_WAIT] == UINT64_MAX);
	assert(observer.saturated);
	assert(graphics_eval_observer_finish(&observer, value));
	assert(!graphics_eval_observer_complete(&observer));
	graphics_eval_observer_init(&observer, true);
	assert(graphics_eval_observer_finish(&observer, value));
	assert(graphics_eval_observer_complete(&observer));
	graphics_eval_observer_init(&observer, true);
	value.clock_valid = false;
	assert(graphics_eval_observer_finish(&observer, value));
	assert(observer.clock_failed);
	assert(!graphics_eval_observer_complete(&observer));
	graphics_eval_observer_init(&observer, true);
	observer.sequence = UINT64_MAX;
	value.clock_valid = true;
	assert(!graphics_eval_observer_note(&observer, value));
	assert(observer.saturated && observer.overflow);
}

int main(void)
{
	disabled_and_copies();
	bounds_and_terminal();
	saturation_and_incomplete();
	terminal_export();
	return 0;
}
