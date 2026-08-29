/* -*-c-*- */

#include "config.h"

#include "event_loop_policy.h"

int fvwm_bounded_queue_drain(
	fqueue *queue, fvwm_queue_drain_state *state, unsigned int budget,
	fvwm_queue_consumer_t consume, void *context)
{
	void *object;

	if (state->depth == 0)
	{
		state->remaining = budget;
	}
	state->depth++;
	for (
		;
		state->remaining > 0 &&
		fqueue_get_first(queue, &object) == 1;)
	{
		fqueue_remove_or_operate_from_front(
			queue, NULL, NULL, NULL, NULL);
		state->remaining--;
		consume(object, context);
	}
	state->depth--;
	if (state->depth == 0)
	{
		state->remaining = 0;
	}

	return !FQUEUE_IS_EMPTY(queue);
}

void fvwm_queue_drain_after_poll(
	fvwm_queue_drain_state *state, unsigned int budget)
{
	if (state->depth > 0)
	{
		state->remaining = budget;
	}
}

fvwm_x_event_action fvwm_x_event_policy(
	int event_pending, unsigned int *consecutive_events)
{
	if (!event_pending)
	{
		*consecutive_events = 0;
		return FVWM_X_EVENT_NONE;
	}
	if (*consecutive_events < FVWM_X_EVENT_BUDGET)
	{
		(*consecutive_events)++;
		return FVWM_X_EVENT_DELIVER;
	}

	*consecutive_events = 0;
	return FVWM_X_EVENT_POLL;
}
