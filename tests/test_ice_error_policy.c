/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include "libs/ice_error_policy.h"
#include "libs/fsm.h"

static FIceIOErrorHandler installed, candidate_seen;
static unsigned prior_calls, initialize_calls;
FIceIOErrorHandler __real_IceSetIOErrorHandler(FIceIOErrorHandler);

static void prior_handler(FIceConn connection)
{
	(void)connection;
	prior_calls++;
}

static void own_handler(FIceConn connection)
{
	(void)connection;
	prior_calls += 2;
}

static void default_handler(FIceConn connection)
{
	(void)connection;
	prior_calls += 3;
}

FIceIOErrorHandler __wrap_IceSetIOErrorHandler(FIceIOErrorHandler handler)
{
	FIceIOErrorHandler previous = __real_IceSetIOErrorHandler(handler);

	if (handler == NULL)
	{
		candidate_seen = previous;
	}
	else
	{
		installed = handler;
	}
	return previous;
}

Status __wrap_SmsInitialize(const char *vendor, const char *release,
	FSmsNewClientProc client, FSmPointer data, FIceHostBasedAuthProc auth,
	int length, char *error)
{
	(void)vendor; (void)release; (void)client; (void)data;
	(void)auth; (void)length;
	error[0] = 0;
	initialize_calls++;
	return 0;
}

static void test_policy(void)
{
	assert(fvwm_ice_error_previous(own_handler, prior_handler,
		own_handler, default_handler) == prior_handler);
	assert(fvwm_ice_error_previous(own_handler, NULL,
		own_handler, default_handler) == NULL);
	assert(fvwm_ice_error_previous(own_handler, prior_handler,
		default_handler, default_handler) == NULL);
	assert(fvwm_ice_error_previous(own_handler, NULL,
		prior_handler, default_handler) == prior_handler);
	assert(fvwm_ice_error_previous(own_handler, prior_handler,
		NULL, default_handler) == NULL);
}

static void test_library_retry(void)
{
	if (!SessionSupport)
	{
		return;
	}
	FIceIOErrorHandler original = __real_IceSetIOErrorHandler(prior_handler);

	prior_calls = 0;
	for (unsigned attempt = 0; attempt < 3; attempt++)
	{
		unsigned before = initialize_calls;

		assert(!fsm_init("FVWM retry unit"));
		assert(initialize_calls == before + 1 && installed != NULL);
		assert(candidate_seen == (attempt == 0 ? prior_handler : installed));
		installed(NULL);
		assert(prior_calls == attempt + 1);
	}
	__real_IceSetIOErrorHandler(original);
	puts("ICE library retries preserve the prior callback exactly once");
}

int main(void)
{
	test_policy();
	test_library_retry();
	puts("ICE error predecessor policy passed");
	return 0;
}
