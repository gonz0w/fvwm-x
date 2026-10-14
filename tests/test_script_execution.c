/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "modules/FvwmScript/script_execution.h"
#include <assert.h>

static void test_shared_steps_and_recovery(void)
{
	script_execution execution = {.step_limit = 3, .depth_limit = 2};
	assert(script_execution_enter(&execution));
	assert(script_execution_step(&execution));
	assert(script_execution_enter(&execution));
	assert(script_execution_step(&execution));
	script_execution_leave(&execution);
	assert(script_execution_enter(&execution));
	assert(script_execution_step(&execution));
	assert(!script_execution_step(&execution));
	assert(execution.failure == SCRIPT_EXECUTION_STEPS);
	script_execution_leave(&execution);
	assert(!script_execution_enter(&execution));
	assert(execution.depth == 1);
	assert(!script_execution_step(&execution));
	script_execution_leave(&execution);
	assert(script_execution_enter(&execution));
	assert(execution.remaining == 3);
	assert(execution.failure == SCRIPT_EXECUTION_OK);
	script_execution_leave(&execution);
}

static void test_depth_and_recovery(void)
{
	script_execution execution = {.step_limit = 100, .depth_limit = 64};
	for (unsigned int i = 0; i < 64; i++)
	{
		assert(script_execution_enter(&execution));
	}
	assert(!script_execution_enter(&execution));
	assert(execution.depth == 64);
	assert(execution.failure == SCRIPT_EXECUTION_DEPTH);
	while (execution.depth > 1)
	{
		script_execution_leave(&execution);
	}
	assert(!script_execution_enter(&execution));
	assert(!script_execution_step(&execution));
	script_execution_leave(&execution);
	assert(script_execution_enter(&execution));
	assert(script_execution_step(&execution));
	script_execution_leave(&execution);
}

static void test_limits_and_exact_completion(void)
{
	script_execution execution = {.step_limit = 0, .depth_limit = 1};
	assert(script_execution_enter(&execution));
	assert(!script_execution_step(&execution));
	script_execution_leave(&execution);
	execution.step_limit = SCRIPT_EXECUTION_MAX_STEPS;
	assert(script_execution_enter(&execution));
	assert(script_execution_step(&execution));
	assert(execution.remaining == SCRIPT_EXECUTION_MAX_STEPS - 1U);
	script_execution_leave(&execution);
	execution.step_limit = 1;
	assert(script_execution_enter(&execution));
	assert(script_execution_step(&execution));
	script_execution_leave(&execution);
	assert(execution.failure == SCRIPT_EXECUTION_OK);
	execution.depth_limit = 0;
	assert(!script_execution_enter(&execution));
	assert(execution.depth == 0);
}

int main(void)
{
	test_shared_steps_and_recovery();
	test_depth_and_recovery();
	test_limits_and_exact_completion();
	return 0;
}
