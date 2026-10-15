/* -*-c-*- */

#include "config.h"

#include "event_loop_policy.h"
#include <limits.h>

void fvwm_output_budget_init(fvwm_output_budget *budget)
{
	budget->bytes = FVWM_MODULE_OUTPUT_BYTE_BUDGET;
	budget->packets = FVWM_MODULE_OUTPUT_PACKET_BUDGET;
	budget->attempts = FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET;
}

size_t fvwm_output_budget_request(fvwm_output_budget *budget, size_t pending)
{
	if (pending == 0 || budget->bytes == 0 || budget->packets == 0 ||
	    budget->attempts == 0)
	{
		return 0;
	}
	budget->attempts--;
	return pending < budget->bytes ? pending : budget->bytes;
}

void fvwm_output_budget_written(fvwm_output_budget *budget, size_t bytes)
{
	budget->bytes -= bytes < budget->bytes ? bytes : budget->bytes;
}

void fvwm_output_budget_completed(fvwm_output_budget *budget)
{
	if (budget->packets > 0)
	{
		budget->packets--;
	}
}

int fvwm_module_deadline_init(fvwm_module_deadline *deadline,
	uint64_t now_ns, int seconds)
{
	deadline->previous_ns = UINT64_MAX;
	deadline->expires_ns = 0;
	if (seconds <= 0 || now_ns == UINT64_MAX)
	{
		return 0;
	}
	uint64_t duration = (uint64_t)seconds * UINT64_C(1000000000);

	if (now_ns >= UINT64_MAX - duration)
	{
		return 0;
	}
	deadline->previous_ns = now_ns;
	deadline->expires_ns = now_ns + duration;
	return 1;
}

fvwm_module_deadline_status fvwm_module_deadline_remaining(
	fvwm_module_deadline *deadline, uint64_t now_ns,
	struct timeval *remaining)
{
	remaining->tv_sec = 0;
	remaining->tv_usec = 0;
	if (now_ns == UINT64_MAX || now_ns < deadline->previous_ns)
	{
		deadline->previous_ns = UINT64_MAX;
		return FVWM_MODULE_DEADLINE_INVALID;
	}
	deadline->previous_ns = now_ns;
	if (now_ns >= deadline->expires_ns)
	{
		return FVWM_MODULE_DEADLINE_EXPIRED;
	}
	uint64_t ns = deadline->expires_ns - now_ns;
	uint64_t us = ns / 1000 + (ns % 1000 != 0);

	remaining->tv_sec = us / 1000000;
	remaining->tv_usec = us % 1000000;
	return FVWM_MODULE_DEADLINE_READY;
}

fvwm_module_deadline_status fvwm_module_deadline_poll_remaining(
	fvwm_module_deadline *deadline, uint64_t now_ns, int *remaining_ms)
{
	struct timeval remaining;
	fvwm_module_deadline_status status;

	*remaining_ms = 0;
	status = fvwm_module_deadline_remaining(deadline, now_ns, &remaining);
	if (status == FVWM_MODULE_DEADLINE_READY)
	{
		long fraction_ms = (remaining.tv_usec + 999) / 1000;

		*remaining_ms = remaining.tv_sec >
		    (INT_MAX - fraction_ms) / 1000 ? INT_MAX :
		    (int)(remaining.tv_sec * 1000 + fraction_ms);
	}
	return status;
}

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
		fqueue_take_first(queue, &object) == 1;)
	{
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

void fvwm_ready_queue_drain(
	fqueue *queue, unsigned int *remaining, fvwm_queue_ready_t ready,
	fvwm_queue_consumer_t consume, void *context)
{
	void *object;

	while (*remaining > 0 && fqueue_get_first(queue, &object) == 1 &&
		ready(object, context))
	{
		if (!fqueue_take_first(queue, &object))
		{
			break;
		}
		(*remaining)--;
		consume(object, context);
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
