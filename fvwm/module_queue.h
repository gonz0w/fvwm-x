#ifndef FVWM_MODULE_QUEUE_H
#define FVWM_MODULE_QUEUE_H

#include "module_list.h"
#include "event_loop_policy.h"

typedef struct
{
	fmodule_input *first;
	fmodule_input *last;
	fmodule_input *cursor;
} module_command_queue;

typedef Bool (*module_input_ready_t)(const fmodule_input *input);

/* The queue owns inputs until take; callbacks only inspect readiness. */
void module_command_queue_add(
	module_command_queue *queue, fmodule_input *input);
fmodule_input *module_command_queue_take(module_command_queue *queue);
/* Scan at most remaining records, retaining progress across calls. Only a
 * producer's head is eligible, and an active producer cannot be overtaken. */
fmodule_input *module_command_queue_take_ready(
	module_command_queue *queue, unsigned int *remaining,
	module_input_ready_t ready);
Bool module_command_queue_drain(
	module_command_queue *queue, fvwm_queue_drain_state *state,
	unsigned int budget, fvwm_queue_consumer_t consume, void *context);

#endif
