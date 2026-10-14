/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/module_list.h"
#include "fvwm/module_interface.h"

static fmodule *modules[3];
static int endpoints[3][2];
static unsigned calls;
static unsigned mode;

void __real_PositiveWrite(fmodule *module, unsigned long *packet, int size);

void __wrap_PositiveWrite(fmodule *module, unsigned long *packet, int size)
{
	assert(module == modules[calls == 0 ? 2 : 0]);
	calls++;
	if (calls == 1)
	{
		if (mode == 0)
		{
			module_kill(modules[1]);
		}
		else
		{
			module_kill_all();
		}
		module_cleanup();
		assert(fcntl(endpoints[1][1], F_GETFD) >= 0);
	}
	__real_PositiveWrite(module, packet, size);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	mode = (unsigned)strtoul(getenv("FVWM_MODULE_ITERATION_MODE"), NULL, 10);
	assert(mode < 2);
	for (unsigned i = 0; i < 3; i++)
	{
		assert(pipe(endpoints[i]) == 0);
		modules[i] = module_create();
		MOD_WRITEFD(modules[i]) = endpoints[i][1];
		module_register(modules[i]);
	}
	BroadcastPacket(M_NEW_PAGE, 1, 123UL);
	assert(calls == (mode == 0 ? 2 : 1));
	module_cleanup();
	assert(fcntl(endpoints[1][1], F_GETFD) < 0 && errno == EBADF);
	if (mode == 0)
	{
		for (unsigned i = 0; i < 3; i += 2)
		{
			unsigned long packet[5];

			assert(!MOD_IS_RETIRED(modules[i]));
			assert(fqueue_get_length(&MOD_PIPEQUEUE(modules[i])) == 1);
			FlushMessageQueue(modules[i]);
			assert(read(endpoints[i][0], packet, sizeof(packet)) ==
				(ssize_t)sizeof(packet));
			assert(packet[0] == START_FLAG && packet[1] == M_NEW_PAGE);
			assert(packet[2] == 5 && packet[4] == 123);
		}
		module_kill_all();
	}
	for (unsigned i = 0; i < 3; i++)
	{
		assert(fcntl(endpoints[i][1], F_GETFD) < 0 && errno == EBADF);
		close(endpoints[i][0]);
	}
	puts("module-iteration-retirement-and-survivor-output: passed");
	fflush(stdout);
	_exit(0);
}
