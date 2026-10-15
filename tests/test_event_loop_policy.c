#include "fvwm/event_loop_policy.h"

#include <assert.h>
#include <limits.h>
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
	state.recurse = 0;
	assert(!fvwm_bounded_queue_drain(
		&queue, &drain_state, FVWM_MODULE_COMMAND_BUDGET,
		consume, &state));
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

typedef struct
{
	fqueue queue;
	unsigned int remaining;
	int seen[8];
	unsigned int count;
	int recurse;
	int cancel;
	int insert;
	int inserted;
} ready_state;

static int is_due(void *object, void *context)
{
	return *(int *)object < 10;
}

static int matches(void *object, void *context)
{
	return *(int *)object == *(int *)context;
}

static void consume_ready(void *object, void *context)
{
	ready_state *state = context;
	void *head;
	int cancelled = 2;

	/* The callback can safely remove other jobs or enter another wait. */
	assert(!fqueue_get_first(&state->queue, &head) || head != object);
	assert(state->count < 8);
	state->seen[state->count++] = *(int *)object;
	if (state->cancel)
	{
		state->cancel = 0;
		fqueue_remove_or_operate_all(
			&state->queue, matches, NULL, NULL, &cancelled);
	}
	if (state->insert)
	{
		state->insert = 0;
		fqueue_add_at_end(&state->queue, &state->inserted);
	}
	if (state->recurse)
	{
		state->recurse = 0;
		fvwm_ready_queue_drain(
			&state->queue, &state->remaining, is_due,
			consume_ready, state);
	}
}

static void test_due_queue_reentrancy_and_poll_budget(void)
{
	ready_state state = { .remaining = 2, .recurse = 1 };
	int jobs[] = { 1, 2, 3, 10 };

	fqueue_init(&state.queue);
	for (unsigned int i = 0; i < 4; i++)
	{
		fqueue_add_at_end(&state.queue, &jobs[i]);
	}
	fvwm_ready_queue_drain(
		&state.queue, &state.remaining, is_due, consume_ready, &state);
	assert(state.count == 2 && state.seen[0] == 1 && state.seen[1] == 2);
	assert(state.remaining == 0);
	/* Returning from a nested dispatch does not replenish the budget. */
	fvwm_ready_queue_drain(
		&state.queue, &state.remaining, is_due, consume_ready, &state);
	assert(state.count == 2);
	state.remaining = 2; /* A completed descriptor poll permits more work. */
	fvwm_ready_queue_drain(
		&state.queue, &state.remaining, is_due, consume_ready, &state);
	assert(state.count == 3 && state.seen[2] == 3);
	assert(state.remaining == 1 && fqueue_get_length(&state.queue) == 1);
	fqueue_remove_or_operate_all(&state.queue, NULL, NULL, NULL, NULL);
}

static void test_due_queue_cancellation_and_insertion(void)
{
	ready_state state = { .remaining = 2, .cancel = 1, .insert = 1,
		.inserted = 4 };
	int jobs[] = { 1, 2, 3 };

	fqueue_init(&state.queue);
	for (unsigned int i = 0; i < 3; i++)
	{
		fqueue_add_at_end(&state.queue, &jobs[i]);
	}
	fvwm_ready_queue_drain(
		&state.queue, &state.remaining, is_due, consume_ready, &state);
	assert(state.count == 2 && state.seen[0] == 1 && state.seen[1] == 3);
	assert(state.remaining == 0 && fqueue_get_length(&state.queue) == 1);
	state.remaining = 1;
	fvwm_ready_queue_drain(
		&state.queue, &state.remaining, is_due, consume_ready, &state);
	assert(state.count == 3 && state.seen[2] == 4);
	fqueue_remove_or_operate_all(&state.queue, NULL, NULL, NULL, NULL);
}

static void test_output_service_budgets(void)
{
	fvwm_output_budget budget;

	fvwm_output_budget_init(&budget);
	assert(fvwm_output_budget_request(&budget, 0) == 0);
	assert(budget.attempts == FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET);
	assert(fvwm_output_budget_request(&budget, (size_t)-1) ==
		FVWM_MODULE_OUTPUT_BYTE_BUDGET);
	fvwm_output_budget_written(&budget, 17);
	assert(budget.bytes == FVWM_MODULE_OUTPUT_BYTE_BUDGET - 17);
	assert(fvwm_output_budget_request(&budget, (size_t)-1) == budget.bytes);
	fvwm_output_budget_written(&budget, budget.bytes);
	assert(fvwm_output_budget_request(&budget, 1) == 0);

	fvwm_output_budget_init(&budget);
	for (unsigned i = 0; i < FVWM_MODULE_OUTPUT_PACKET_BUDGET; i++)
	{
		assert(fvwm_output_budget_request(&budget, 1) == 1);
		fvwm_output_budget_written(&budget, 1);
		fvwm_output_budget_completed(&budget);
	}
	assert(budget.bytes > 0 && budget.attempts > 0);
	assert(fvwm_output_budget_request(&budget, 1) == 0);
	fvwm_output_budget_completed(&budget);
	assert(budget.packets == 0);

	fvwm_output_budget_init(&budget);
	for (unsigned i = 0; i < FVWM_MODULE_OUTPUT_ATTEMPT_BUDGET; i++)
	{
		/* EINTR consumes an attempt but no successful bytes/packets. */
		assert(fvwm_output_budget_request(&budget, 1) == 1);
	}
	assert(budget.bytes == FVWM_MODULE_OUTPUT_BYTE_BUDGET);
	assert(budget.packets == FVWM_MODULE_OUTPUT_PACKET_BUDGET);
	assert(fvwm_output_budget_request(&budget, 1) == 0);
}

static void test_module_deadline(void)
{
	fvwm_module_deadline deadline;
	struct timeval remaining;
	uint64_t start = UINT64_C(7000000000);

	assert(fvwm_module_deadline_init(&deadline, start, 30));
	assert(fvwm_module_deadline_remaining(&deadline, start, &remaining) ==
		FVWM_MODULE_DEADLINE_READY);
	assert(remaining.tv_sec == 30 && remaining.tv_usec == 0);
	for (unsigned i = 1; i < 30; i++)
	{
		assert(fvwm_module_deadline_remaining(&deadline,
			start + i * UINT64_C(1000000000), &remaining) ==
			FVWM_MODULE_DEADLINE_READY);
		assert(remaining.tv_sec == 30 - i && remaining.tv_usec == 0);
	}
	assert(fvwm_module_deadline_remaining(&deadline,
		start + UINT64_C(30000000000) - 1, &remaining) ==
		FVWM_MODULE_DEADLINE_READY);
	assert(remaining.tv_sec == 0 && remaining.tv_usec == 1);
	assert(fvwm_module_deadline_remaining(&deadline,
		start + UINT64_C(30000000000), &remaining) ==
		FVWM_MODULE_DEADLINE_EXPIRED);
	assert(remaining.tv_sec == 0 && remaining.tv_usec == 0);
	assert(fvwm_module_deadline_remaining(&deadline,
		start + UINT64_C(31000000000), &remaining) ==
		FVWM_MODULE_DEADLINE_EXPIRED);

	assert(fvwm_module_deadline_init(&deadline, 0, 1));
	assert(fvwm_module_deadline_remaining(&deadline, 1, &remaining) ==
		FVWM_MODULE_DEADLINE_READY);
	assert(remaining.tv_sec == 1 && remaining.tv_usec == 0);
	assert(fvwm_module_deadline_remaining(&deadline, 1001, &remaining) ==
		FVWM_MODULE_DEADLINE_READY);
	assert(remaining.tv_sec == 0 && remaining.tv_usec == 999999);
}

static void test_module_deadline_invalid_clock_and_range(void)
{
	fvwm_module_deadline deadline;
	struct timeval remaining;

	assert(!fvwm_module_deadline_init(&deadline, UINT64_MAX, 1));
	assert(fvwm_module_deadline_remaining(&deadline, 0, &remaining) ==
		FVWM_MODULE_DEADLINE_INVALID);
	assert(!fvwm_module_deadline_init(&deadline, 0, 0));
	assert(!fvwm_module_deadline_init(&deadline, 0, -1));
	assert(!fvwm_module_deadline_init(&deadline,
		UINT64_MAX - UINT64_C(1000000000), 1));
	assert(fvwm_module_deadline_init(&deadline, 0, INT_MAX));
	assert(fvwm_module_deadline_remaining(&deadline, 0, &remaining) ==
		FVWM_MODULE_DEADLINE_READY);
	assert(remaining.tv_sec == INT_MAX && remaining.tv_usec == 0);
	assert(fvwm_module_deadline_init(&deadline, 100, 1));
	assert(fvwm_module_deadline_remaining(&deadline, 99, &remaining) ==
		FVWM_MODULE_DEADLINE_INVALID);
	assert(fvwm_module_deadline_remaining(&deadline, 101, &remaining) ==
		FVWM_MODULE_DEADLINE_INVALID);
	assert(fvwm_module_deadline_init(&deadline, 0, 1));
	assert(fvwm_module_deadline_remaining(&deadline, UINT64_MAX, &remaining) ==
		FVWM_MODULE_DEADLINE_INVALID);
	assert(remaining.tv_sec == 0 && remaining.tv_usec == 0);
}

static void test_module_poll_deadline(void)
{
	fvwm_module_deadline deadline;
	int remaining_ms = -1;

	assert(fvwm_module_deadline_init(&deadline, 0, 1));
	assert(fvwm_module_deadline_poll_remaining(&deadline, 0,
		&remaining_ms) == FVWM_MODULE_DEADLINE_READY);
	assert(remaining_ms == 1000);
	assert(fvwm_module_deadline_poll_remaining(&deadline, 1000001,
		&remaining_ms) == FVWM_MODULE_DEADLINE_READY);
	assert(remaining_ms == 999);
	assert(fvwm_module_deadline_poll_remaining(&deadline, 999999999,
		&remaining_ms) == FVWM_MODULE_DEADLINE_READY);
	assert(remaining_ms == 1);
	assert(fvwm_module_deadline_poll_remaining(&deadline, 1000000000,
		&remaining_ms) == FVWM_MODULE_DEADLINE_EXPIRED);
	assert(remaining_ms == 0);
	assert(fvwm_module_deadline_init(&deadline, 1, INT_MAX));
	assert(fvwm_module_deadline_poll_remaining(&deadline, 1,
		&remaining_ms) == FVWM_MODULE_DEADLINE_READY);
	assert(remaining_ms == INT_MAX);
	assert(fvwm_module_deadline_poll_remaining(&deadline, 0,
		&remaining_ms) == FVWM_MODULE_DEADLINE_INVALID);
	assert(remaining_ms == 0);
	assert(fvwm_module_deadline_poll_remaining(&deadline, UINT64_MAX,
		&remaining_ms) == FVWM_MODULE_DEADLINE_INVALID);
	assert(remaining_ms == 0);
}

int main(void)
{
	test_module_deadline();
	test_module_poll_deadline();
	test_module_deadline_invalid_clock_and_range();
	test_output_service_budgets();
	test_bounded_queue_drain();
	test_reentrant_enqueue_is_bounded();
	test_recursive_drain_shares_budget();
	test_poll_replenishes_recursive_budget();
	test_x_event_budget();
	test_due_queue_reentrancy_and_poll_budget();
	test_due_queue_cancellation_and_insertion();

	return 0;
}
