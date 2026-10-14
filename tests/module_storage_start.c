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
#include <string.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/module_list.h"

static Bool refuse_malloc;
static Bool refuse_calloc;

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);

void *__wrap_malloc(size_t size)
{
	if (refuse_malloc)
	{
		refuse_malloc = False;
		return NULL;
	}
	return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size)
{
	if (refuse_calloc)
	{
		refuse_calloc = False;
		return NULL;
	}
	return __real_calloc(count, size);
}

static fmodule *prepare(int endpoints[2])
{
	fmodule *module = module_create();

	assert(pipe(endpoints) == 0);
	MOD_WRITEFD(module) = endpoints[1];
	module_register(module);
	return module;
}

static void check_output(fmodule *module, int read_fd,
	const unsigned long *packet, size_t size)
{
	unsigned long received[5];

	assert(size == sizeof(received));
	FlushMessageQueue(module);
	assert(FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(module)));
	assert(MOD_PIPEQUEUE_BYTES(module) == 0);
	assert(read(read_fd, received, sizeof(received)) == sizeof(received));
	assert(memcmp(received, packet, size) == 0);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	unsigned mode = (unsigned)strtoul(
		getenv("FVWM_MODULE_STORAGE_MODE"), NULL, 10);
	unsigned site = mode % 3;
	Bool seeded = mode >= 3;
	unsigned long packet[5] = { START_FLAG, M_NEW_PAGE, 5, 0, 100 };
	unsigned long config[5] = { START_FLAG, M_CONFIGURE_WINDOW, 5, 0, 200 };
	int failed_endpoints[2];
	int survivor_endpoints[2];
	fmodule *module;
	fmodule *survivor;
	size_t held_bytes;

	assert(mode < 6);
	module = prepare(failed_endpoints);
	survivor = prepare(survivor_endpoints);
	module->xinputRefs++;
	PositiveWrite(survivor, packet, sizeof(packet));
	module_config_interaction_begin();
	if (seeded)
	{
		PositiveWrite(module, packet, sizeof(packet));
		assert(module_config_defer(module, config, sizeof(config)));
	}
	held_bytes = MOD_PIPEQUEUE_BYTES(module);
	config[4]++;
	if (site == 1)
	{
		refuse_calloc = True;
	}
	else
	{
		refuse_malloc = True;
	}
	if (site == 2)
	{
		assert(module_config_defer(module, config, sizeof(config)));
	}
	else
	{
		PositiveWrite(module, packet, sizeof(packet));
	}
	module_config_interaction_end();
	assert(!refuse_malloc && !refuse_calloc);
	assert(MOD_IS_RETIRED(module));
	assert(MOD_PIPEQUEUE_BYTES(module) == held_bytes);
	assert(fqueue_get_length(&MOD_PIPEQUEUE(module)) == (seeded ? 1 : 0));
	assert(module->xdeferredConfigCount == (seeded ? 1 : 0));
	assert(module_config_has_window(module, 200) == seeded);
	assert(!module_config_has_window(module, 201));
	module_cleanup();
	/* A callback pin retains prior packets, but never the refused record. */
	assert(MOD_PIPEQUEUE_BYTES(module) == held_bytes);
	assert(fcntl(failed_endpoints[1], F_GETFD) >= 0);
	module->xinputRefs--;
	module_cleanup();
	assert(fcntl(failed_endpoints[1], F_GETFD) < 0 && errno == EBADF);
	close(failed_endpoints[0]);
	assert(!MOD_IS_RETIRED(survivor));
	check_output(survivor, survivor_endpoints[0], packet, sizeof(packet));

	/* The next admission succeeds, including deferred configure ownership. */
	module_config_interaction_begin();
	assert(module_config_defer(survivor, config, sizeof(config)));
	module_config_interaction_end();
	fmodule_config_packet *snapshot = module_config_take(survivor);
	assert(snapshot && snapshot->size == sizeof(config));
	assert(memcmp(snapshot->data, config, sizeof(config)) == 0);
	free(snapshot);
	assert(MOD_PIPEQUEUE_BYTES(survivor) == 0);
	packet[4]++;
	PositiveWrite(survivor, packet, sizeof(packet));
	check_output(survivor, survivor_endpoints[0], packet, sizeof(packet));
	module_kill(survivor);
	module_cleanup();
	assert(fcntl(survivor_endpoints[1], F_GETFD) < 0 && errno == EBADF);
	close(survivor_endpoints[0]);
	puts("module-storage-refusal-and-survivor-retry: passed");
	fflush(stdout);
	_exit(0);
}
