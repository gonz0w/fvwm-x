/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include <assert.h>
#include "module_queue.h"

void module_command_queue_add(
	module_command_queue *queue, fmodule_input *input)
{
	fmodule *module = input->module;

	assert(!input->in_command_queue);
	input->queue_previous = queue->last;
	input->queue_next = NULL;
	input->producer_next = NULL;
	if (queue->last != NULL)
	{
		queue->last->queue_next = input;
	}
	else
	{
		queue->first = input;
	}
	queue->last = input;
	if (module->xcommandTail != NULL)
	{
		module->xcommandTail->producer_next = input;
	}
	else
	{
		module->xcommandHead = input;
	}
	module->xcommandTail = input;
	input->in_command_queue = True;
}

static fmodule_input *take_input(
	module_command_queue *queue, fmodule_input *input)
{
	fmodule *module = input->module;

	assert(module->xcommandHead == input);
	if (queue->cursor == input)
	{
		queue->cursor = input->queue_next;
	}
	if (input->queue_previous != NULL)
	{
		input->queue_previous->queue_next = input->queue_next;
	}
	else
	{
		queue->first = input->queue_next;
	}
	if (input->queue_next != NULL)
	{
		input->queue_next->queue_previous = input->queue_previous;
	}
	else
	{
		queue->last = input->queue_previous;
	}
	module->xcommandHead = input->producer_next;
	if (module->xcommandHead == NULL)
	{
		module->xcommandTail = NULL;
	}
	input->queue_next = NULL;
	input->queue_previous = NULL;
	input->producer_next = NULL;
	input->in_command_queue = False;
	return input;
}

fmodule_input *module_command_queue_take(module_command_queue *queue)
{
	return queue->first == NULL ? NULL : take_input(queue, queue->first);
}

fmodule_input *module_command_queue_take_ready(
	module_command_queue *queue, unsigned int *remaining,
	module_input_ready_t ready)
{
	fmodule_input *input = queue->cursor;

	if (input == NULL)
	{
		input = queue->first;
	}
	while (input != NULL && *remaining > 0)
	{
		queue->cursor = input->queue_next;
		(*remaining)--;
		if (input == input->module->xcommandHead &&
		    input->module->xactiveInputs == 0 && ready(input))
		{
			return take_input(queue, input);
		}
		input = queue->cursor;
	}
	queue->cursor = input;
	return NULL;
}

Bool module_command_queue_drain(
	module_command_queue *queue, fvwm_queue_drain_state *state,
	unsigned int budget, fvwm_queue_consumer_t consume, void *context)
{
	fmodule_input *input;

	if (state->depth == 0)
	{
		state->remaining = budget;
	}
	state->depth++;
	while (state->remaining > 0 &&
	       (input = module_command_queue_take(queue)) != NULL)
	{
		state->remaining--;
		consume(input, context);
	}
	state->depth--;
	if (state->depth == 0)
	{
		state->remaining = 0;
	}
	return queue->first != NULL;
}
