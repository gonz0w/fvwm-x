/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "fvwm/event_loop_policy.h"
#include "fvwm/module_queue.h"
#include "fvwm/module_records.h"
#include <stdio.h>
#include <stdlib.h>

Bool __real_module_input_queue_ready(const fmodule *module);
Bool __real_module_command_queue_drain(
	module_command_queue *queue, fvwm_queue_drain_state *state, unsigned int budget,
	fvwm_queue_consumer_t consume, void *context);

/* Exercise the production poll/dequeue paths with a small producer set.
 * Production admission (256) is separately tested against real pipes. */
Bool __wrap_module_input_queue_ready(const fmodule *module)
{
	static Bool reported = False;

	if (module->xqueuedInputs > 16)
	{
		fprintf(stderr,
			"queue-test: producer exceeded 16 pending inputs\n");
		abort();
	}
	if (module->xqueuedInputs == 16)
	{
		if (!reported)
		{
			fprintf(stderr,
				"queue-test: producer paused at 16 inputs\n");
			reported = True;
		}
		return False;
	}

	return __real_module_input_queue_ready(module);
}

Bool __wrap_module_command_queue_drain(
	module_command_queue *queue, fvwm_queue_drain_state *state, unsigned int budget,
	fvwm_queue_consumer_t consume, void *context)
{
	return __real_module_command_queue_drain(
		queue, state, budget > 8 ? 8 : budget, consume, context);
}
