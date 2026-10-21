/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/module_records.h"
#include "fvwm/module_queue.h"
#include <assert.h>
#include <string.h>
static Bool ready(const fmodule_input *input)
{
	return strcmp(input->command, "safe") == 0;
}

static void test_origin_order_and_bounded_scan(void)
{
	module_command_queue queue = { 0 };
	fmodule producers[3] = { 0 };
	fmodule_input inputs[260] = { 0 };
	unsigned int scans;

	for (unsigned int i = 0; i < 256; i++)
	{
		inputs[i].module = &producers[0];
		inputs[i].command = i == 0 ? "deferred" : "safe";
		module_command_queue_add(&queue, &inputs[i]);
	}
	inputs[256].module = &producers[1];
	inputs[256].command = "safe";
	module_command_queue_add(&queue, &inputs[256]);
	for (unsigned int i = 0; i < 4; i++)
	{
		scans = 64;
		assert(module_command_queue_take_ready(&queue, &scans, ready) == NULL);
		assert(scans == 0);
	}
	scans = 64;
	assert(module_command_queue_take_ready(&queue, &scans, ready) == &inputs[256]);
	assert(scans == 63 && !inputs[256].in_command_queue);
	assert(producers[1].xcommandHead == NULL);
	assert(queue.first == &inputs[0] && queue.last == &inputs[255]);
	/* The same producer's safe tail cannot pass its deferred head. */
	scans = 256;
	assert(module_command_queue_take_ready(&queue, &scans, ready) == NULL);
	assert(module_command_queue_take(&queue) == &inputs[0]);
	scans = 64;
	assert(module_command_queue_take_ready(&queue, &scans, ready) == &inputs[1]);
	/* Removing normal FIFO heads must repair a retained scan cursor. */
	while (module_command_queue_take(&queue) != NULL)
	{
	}
	assert(queue.first == NULL && queue.last == NULL && queue.cursor == NULL);
	assert(producers[0].xcommandHead == NULL && producers[0].xcommandTail == NULL);
}

static void test_active_producer_and_fifo_recovery(void)
{
	module_command_queue queue = { 0 };
	fmodule a = { 0 }, b = { 0 };
	fmodule_input inputs[3] = { 0 };
	unsigned int scans = 64;

	inputs[0].module = &a;
	inputs[1].module = &b;
	inputs[2].module = &a;
	for (unsigned int i = 0; i < 3; i++)
	{
		inputs[i].command = "safe";
		module_command_queue_add(&queue, &inputs[i]);
	}
	a.xactiveInputs = 1;
	assert(module_command_queue_take_ready(&queue, &scans, ready) == &inputs[1]);
	assert(module_command_queue_take_ready(&queue, &scans, ready) == NULL);
	a.xactiveInputs = 0;
	assert(module_command_queue_take(&queue) == &inputs[0]);
	assert(module_command_queue_take(&queue) == &inputs[2]);
	assert(module_command_queue_take(&queue) == NULL);
}

typedef struct
{
	module_command_queue *queue;
	fvwm_queue_drain_state *state;
	unsigned int consumed;
	Bool nested;
} drain_context;

static void consume(void *object, void *context)
{
	drain_context *state = context;
	fmodule_input *input = object;

	assert(!input->in_command_queue);
	state->consumed++;
	if (state->nested && state->consumed == 1)
	{
		assert(module_command_queue_drain(
			state->queue, state->state, 64, consume, state));
	}
}

static void test_recursive_budget_and_enqueue(void)
{
	module_command_queue queue = { 0 };
	fvwm_queue_drain_state state = FVWM_QUEUE_DRAIN_STATE_INIT;
	drain_context context = { &queue, &state, 0, True };
	fmodule module = { 0 };
	fmodule_input inputs[65] = { 0 };

	for (unsigned int i = 0; i < 65; i++)
	{
		inputs[i].module = &module;
		module_command_queue_add(&queue, &inputs[i]);
	}
	assert(module_command_queue_drain(&queue, &state, 64, consume, &context));
	assert(context.consumed == 64 && state.depth == 0 && state.remaining == 0);
	assert(!module_command_queue_drain(&queue, &state, 64, consume, &context));
	assert(context.consumed == 65);
	/* Reusing a taken input must not retain stale links or cursor state. */
	module_command_queue_add(&queue, &inputs[0]);
	assert(module_command_queue_take(&queue) == &inputs[0]);
	assert(module.xcommandHead == NULL && module.xcommandTail == NULL);
}

int main(void)
{
	test_origin_order_and_bounded_scan();
	test_active_producer_and_fifo_recovery();
	test_recursive_budget_and_enqueue();
	return 0;
}
