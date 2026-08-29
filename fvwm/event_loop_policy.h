/* -*-c-*- */

#ifndef FVWM_EVENT_LOOP_POLICY_H
#define FVWM_EVENT_LOOP_POLICY_H

#include "libs/fqueue.h"

#define FVWM_X_EVENT_BUDGET 64
#define FVWM_MODULE_COMMAND_BUDGET 64

typedef void (*fvwm_queue_consumer_t)(void *object, void *context);

typedef struct
{
	unsigned int depth;
	unsigned int remaining;
} fvwm_queue_drain_state;

#define FVWM_QUEUE_DRAIN_STATE_INIT { 0, 0 }

typedef enum
{
	FVWM_X_EVENT_NONE,
	FVWM_X_EVENT_DELIVER,
	FVWM_X_EVENT_POLL
} fvwm_x_event_action;

int fvwm_bounded_queue_drain(
	fqueue *queue, fvwm_queue_drain_state *state, unsigned int budget,
	fvwm_queue_consumer_t consume, void *context);
void fvwm_queue_drain_after_poll(
	fvwm_queue_drain_state *state, unsigned int budget);
fvwm_x_event_action fvwm_x_event_policy(
	int event_pending, unsigned int *consecutive_events);

#endif
