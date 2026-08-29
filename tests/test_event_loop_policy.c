#include "fvwm/event_loop_policy.h"

#include <assert.h>
#include <stddef.h>

typedef struct
{
	fqueue *queue;
	fvwm_queue_drain_state *drain_state;
	unsigned int consumed;
	unsigned int requeue_until;
	unsigned int nested_consumed;
	int recurse;
	int poll_after_recurse;
	int token;
} consumer_state;

static void consume(void *object, void *context)
{
	consumer_state *state = context;

	assert(object != NULL);
	state->consumed++;
	if (state->recurse && state->consumed == 1)
	{
		unsigned int before = state->consumed;

		assert(fvwm_bounded_queue_drain(
			state->queue, state->drain_state,
			FVWM_MODULE_COMMAND_BUDGET, consume, state));
		state->nested_consumed = state->consumed - before;
		if (state->poll_after_recurse)
		{
			fvwm_queue_drain_after_poll(
				state->drain_state,
				FVWM_MODULE_COMMAND_BUDGET);
		}
	}
	if (state->consumed < state->requeue_until)
	{
		fqueue_add_at_end(state->queue, &state->token);
	}
}

static void test_bounded_queue_drain(void)
{
	consumer_state state;
	fqueue queue;
	fvwm_queue_drain_state drain_state = FVWM_QUEUE_DRAIN_STATE_INIT;
	int objects[FVWM_MODULE_COMMAND_BUDGET + 1];
	unsigned int i;

	fqueue_init(&queue);
	state.queue = &queue;
	state.drain_state = &drain_state;
	state.consumed = 0;
	state.requeue_until = 0;
	state.recurse = 0;
	state.poll_after_recurse = 0;
	for (i = 0; i < FVWM_MODULE_COMMAND_BUDGET + 1; i++)
	{
		objects[i] = (int)i;
		fqueue_add_at_end(&queue, &objects[i]);
	}

	assert(fvwm_bounded_queue_drain(
		&queue, &drain_state, FVWM_MODULE_COMMAND_BUDGET,
		consume, &state));
	assert(state.consumed == FVWM_MODULE_COMMAND_BUDGET);
	assert(fqueue_get_length(&queue) == 1);
	assert(!fvwm_bounded_queue_drain(
		&queue, &drain_state, FVWM_MODULE_COMMAND_BUDGET,
		consume, &state));
	assert(state.consumed == FVWM_MODULE_COMMAND_BUDGET + 1);
}

static void test_reentrant_enqueue_is_bounded(void)
{
	consumer_state state;
	fqueue queue;
	fvwm_queue_drain_state drain_state = FVWM_QUEUE_DRAIN_STATE_INIT;

	fqueue_init(&queue);
	state.queue = &queue;
	state.drain_state = &drain_state;
	state.consumed = 0;
	state.requeue_until = FVWM_MODULE_COMMAND_BUDGET + 10;
	state.recurse = 0;
	state.poll_after_recurse = 0;
	state.token = 1;
	fqueue_add_at_end(&queue, &state.token);

	assert(fvwm_bounded_queue_drain(
		&queue, &drain_state, FVWM_MODULE_COMMAND_BUDGET,
		consume, &state));
	assert(state.consumed == FVWM_MODULE_COMMAND_BUDGET);
	assert(fqueue_get_length(&queue) == 1);

	state.requeue_until = 0;
	assert(!fvwm_bounded_queue_drain(
		&queue, &drain_state, FVWM_MODULE_COMMAND_BUDGET,
		consume, &state));
}

static void test_recursive_drain_shares_budget(void)
{
	consumer_state state;
	fqueue queue;
	fvwm_queue_drain_state drain_state = FVWM_QUEUE_DRAIN_STATE_INIT;
	int objects[FVWM_MODULE_COMMAND_BUDGET + 1];
	unsigned int i;

	fqueue_init(&queue);
	state.queue = &queue;
	state.drain_state = &drain_state;
	state.consumed = 0;
	state.requeue_until = 0;
	state.nested_consumed = 99;
	state.recurse = 1;
	state.poll_after_recurse = 0;
	for (i = 0; i < FVWM_MODULE_COMMAND_BUDGET + 1; i++)
	{
		objects[i] = (int)i;
		fqueue_add_at_end(&queue, &objects[i]);
	}

	assert(fvwm_bounded_queue_drain(
		&queue, &drain_state, FVWM_MODULE_COMMAND_BUDGET,
		consume, &state));
	assert(state.consumed == FVWM_MODULE_COMMAND_BUDGET);
	assert(state.nested_consumed == FVWM_MODULE_COMMAND_BUDGET - 1);
	assert(fqueue_get_length(&queue) == 1);
}

static void test_poll_replenishes_recursive_budget(void)
{
	consumer_state state;
	fqueue queue;
	fvwm_queue_drain_state drain_state = FVWM_QUEUE_DRAIN_STATE_INIT;
	int objects[FVWM_MODULE_COMMAND_BUDGET + 1];
	unsigned int i;

	fqueue_init(&queue);
	state.queue = &queue;
	state.drain_state = &drain_state;
	state.consumed = 0;
	state.requeue_until = 0;
	state.nested_consumed = 0;
	state.recurse = 1;
	state.poll_after_recurse = 1;
	for (i = 0; i < FVWM_MODULE_COMMAND_BUDGET + 1; i++)
	{
		objects[i] = (int)i;
		fqueue_add_at_end(&queue, &objects[i]);
	}

	assert(!fvwm_bounded_queue_drain(
		&queue, &drain_state, FVWM_MODULE_COMMAND_BUDGET,
		consume, &state));
	assert(state.consumed == FVWM_MODULE_COMMAND_BUDGET + 1);
	assert(state.nested_consumed == FVWM_MODULE_COMMAND_BUDGET - 1);
}

static void test_x_event_budget(void)
{
	unsigned int consecutive = 0;
	unsigned int i;

	for (i = 0; i < FVWM_X_EVENT_BUDGET; i++)
	{
		assert(fvwm_x_event_policy(1, &consecutive) ==
			FVWM_X_EVENT_DELIVER);
	}
	assert(consecutive == FVWM_X_EVENT_BUDGET);
	assert(fvwm_x_event_policy(1, &consecutive) == FVWM_X_EVENT_POLL);
	assert(consecutive == 0);
	assert(fvwm_x_event_policy(1, &consecutive) ==
		FVWM_X_EVENT_DELIVER);
	assert(fvwm_x_event_policy(0, &consecutive) == FVWM_X_EVENT_NONE);
	assert(consecutive == 0);
}

int main(void)
{
	test_bounded_queue_drain();
	test_reentrant_enqueue_is_bounded();
	test_recursive_drain_shares_budget();
	test_poll_replenishes_recursive_budget();
	test_x_event_budget();

	return 0;
}
