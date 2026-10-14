/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "script_execution.h"
#include <assert.h>

bool script_execution_enter(script_execution *execution)
{
	if (execution->depth == 0)
	{
		execution->remaining = execution->step_limit;
		execution->failure = SCRIPT_EXECUTION_OK;
	}
	if (execution->failure != SCRIPT_EXECUTION_OK)
	{
		return false;
	}
	if (execution->depth >= execution->depth_limit)
	{
		execution->failure = SCRIPT_EXECUTION_DEPTH;
		return false;
	}
	execution->depth++;
	return true;
}

bool script_execution_step(script_execution *execution)
{
	assert(execution->depth > 0);
	if (execution->failure != SCRIPT_EXECUTION_OK)
	{
		return false;
	}
	if (execution->remaining == 0)
	{
		execution->failure = SCRIPT_EXECUTION_STEPS;
		return false;
	}
	execution->remaining--;
	return true;
}

void script_execution_leave(script_execution *execution)
{
	assert(execution->depth > 0);
	execution->depth--;
}
