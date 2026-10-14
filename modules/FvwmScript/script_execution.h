#ifndef FVWMSCRIPT_SCRIPT_EXECUTION_H
#define FVWMSCRIPT_SCRIPT_EXECUTION_H

#include <limits.h>
#include <stdbool.h>

#define SCRIPT_EXECUTION_DEFAULT_STEPS 10000U
#define SCRIPT_EXECUTION_DEFAULT_DEPTH 64U
#define SCRIPT_EXECUTION_MAX_STEPS INT_MAX
#define SCRIPT_EXECUTION_MAX_DEPTH 256U

typedef enum
{
	SCRIPT_EXECUTION_OK,
	SCRIPT_EXECUTION_STEPS,
	SCRIPT_EXECUTION_DEPTH,
	SCRIPT_EXECUTION_VALUE,
	SCRIPT_EXECUTION_STORAGE
} script_execution_failure;

typedef struct
{
	unsigned int step_limit;
	unsigned int depth_limit;
	unsigned int remaining;
	unsigned int depth;
	script_execution_failure failure;
} script_execution;

/* A failed nested entry does not acquire a depth slot. Failure stays latched
 * until the next outer entry, so nested calls cannot replenish the budget. */
bool script_execution_enter(script_execution *execution);
bool script_execution_step(script_execution *execution);
void script_execution_leave(script_execution *execution);

#endif
