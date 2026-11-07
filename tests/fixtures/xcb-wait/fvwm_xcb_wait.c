/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"

#include <stddef.h>
#include <string.h>
#include "fvwm_xcb_wait.h"

/* The fixture observes one calling thread. Other thread activity is outside
 * its coverage. No allocation, callback, logging or X request occurs here.
 */
static _Thread_local struct
{
	void *connection;
	_Atomic uint32_t *entered_wait;
	int active;
	int in_reply;
	uint64_t sequence;
	struct fvwm_xcb_wait_result result;
} observation;

static void increment(uint32_t *value)
{
	if (*value == UINT32_MAX)
	{
		observation.result.invalid = 1;
		observation.result.overflow = 1;
	}
	else
	{
		++*value;
	}
}

static int matches(void *connection)
{
	return observation.active && observation.connection == connection;
}

int fvwm_xcb_wait_begin(void *connection, _Atomic uint32_t *entered_wait)
{
	if (observation.active)
	{
		observation.result.invalid = 1;
		return 0;
	}
	memset(&observation, 0, sizeof(observation));
	observation.result.abi = FVWM_XCB_WAIT_ABI;
	if (connection == NULL)
	{
		return 0;
	}
	observation.connection = connection;
	observation.entered_wait = entered_wait;
	if (entered_wait != NULL)
	{
		atomic_store_explicit(entered_wait, 0, memory_order_release);
	}
	observation.active = 1;
	return 1;
}

int fvwm_xcb_wait_end(struct fvwm_xcb_wait_result *result)
{
	int valid;

	if (result == NULL)
	{
		if (observation.active)
		{
			observation.result.invalid = 1;
		}
		return 0;
	}
	memset(result, 0, sizeof(*result));
	result->abi = FVWM_XCB_WAIT_ABI;
	if (!observation.active)
	{
		result->invalid = 1;
		return 0;
	}
	if (observation.in_reply)
	{
		observation.result.invalid = 1;
	}
	*result = observation.result;
	valid = !result->invalid;
	memset(&observation, 0, sizeof(observation));
	return valid;
}

void fvwm_xcb_wait_reply_begin(void *connection, uint64_t sequence)
{
	struct fvwm_xcb_wait_record *record;

	if (!matches(connection))
	{
		return;
	}
	if (observation.in_reply ||
	    observation.result.records == FVWM_XCB_WAIT_CAPACITY)
	{
		observation.result.invalid = 1;
		if (observation.result.records == FVWM_XCB_WAIT_CAPACITY)
		{
			observation.result.overflow = 1;
		}
		return;
	}
	observation.in_reply = 1;
	observation.sequence = sequence;
	record = &observation.result.record[observation.result.records++];
	memset(record, 0, sizeof(*record));
	record->sequence = sequence;
	increment(&observation.result.reply_calls);
}

void fvwm_xcb_wait_reply_wait(void *connection, uint64_t sequence)
{
	struct fvwm_xcb_wait_record *record;

	if (!matches(connection))
	{
		return;
	}
	if (!observation.in_reply || observation.sequence != sequence)
	{
		observation.result.invalid = 1;
		return;
	}
	record = &observation.result.record[observation.result.records - 1];
	if (record->wait_calls == 0)
	{
		increment(&observation.result.logical_waits);
		if (observation.entered_wait != NULL)
		{
			atomic_store_explicit(observation.entered_wait, 1,
				memory_order_release);
		}
	}
	increment(&record->wait_calls);
	increment(&observation.result.wait_calls);
}

void fvwm_xcb_wait_partial(void *connection)
{
	struct fvwm_xcb_wait_record *record;

	if (!matches(connection) || !observation.in_reply)
	{
		return;
	}
	record = &observation.result.record[observation.result.records - 1];
	increment(&record->partial_wait_calls);
	increment(&observation.result.partial_wait_calls);
}

void fvwm_xcb_wait_reply_end(void *connection, uint64_t sequence,
	int reply_available)
{
	struct fvwm_xcb_wait_record *record;

	if (!matches(connection))
	{
		return;
	}
	if (!observation.in_reply || observation.sequence != sequence)
	{
		observation.result.invalid = 1;
		return;
	}
	record = &observation.result.record[observation.result.records - 1];
	record->reply_available = reply_available != 0;
	record->completed = 1;
	observation.in_reply = 0;
	observation.sequence = 0;
}
