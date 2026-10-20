/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <X11/SM/SMlib.h>
#include <X11/ICE/ICElib.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned saved, completed, returned;

static void phase2(SmcConn connection, SmPointer data)
{
	(void)data;
	SmcSaveYourselfDone(connection, True);
}

static void save(SmcConn connection, SmPointer data, int type,
	Bool shutdown, int interaction, Bool fast)
{
	(void)data;
	assert(type == SmSaveLocal && !shutdown &&
		interaction == SmInteractStyleNone && !fast);
	assert(++saved == 1);
	assert(SmcRequestSaveYourselfPhase2(connection, phase2, NULL));
}

static void complete(SmcConn connection, SmPointer data)
{
	(void)connection; (void)data;
	assert(saved == 1 && ++completed == 1);
}

static void unexpected(SmcConn connection, SmPointer data)
{
	(void)connection; (void)data;
	assert(!"unexpected session termination or cancellation");
}

static void properties(SmcConn connection, SmPointer data,
	int count, SmProp **props)
{
	(void)connection; (void)data;
	assert(count == 0);
	free(props);
	returned++;
}

static void await_reply(SmcConn connection, unsigned *count, unsigned expected)
{
	IceConn ice = SmcGetIceConnection(connection);
	struct timespec start, now;

	assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
	while (*count < expected)
	{
		struct pollfd input = {IceConnectionNumber(ice), POLLIN, 0};
		int ready = poll(&input, 1, 20);

		assert(ready >= 0 || errno == EINTR);
		if (ready > 0)
			assert(IceProcessMessages(ice, NULL, NULL) ==
				IceProcessMessagesSuccess);
		assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
		assert(now.tv_sec - start.tv_sec < 5);
	}
	assert(*count == expected);
}

int main(int argc, char **argv)
{
	SmcCallbacks callbacks = {0};
	char error[256], *id = NULL;
	SmcConn connection;

	assert(argc == 2);
	callbacks.save_yourself.callback = save;
	callbacks.die.callback = unexpected;
	callbacks.save_complete.callback = complete;
	callbacks.shutdown_cancelled.callback = unexpected;
	connection = SmcOpenConnection(NULL, NULL, SmProtoMajor, SmProtoMinor,
		SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask |
		SmcShutdownCancelledProcMask, &callbacks,
		strcmp(argv[1], "previous") == 0 ? "unrestored-client" : NULL,
		&id, sizeof(error), error);
	if (connection == NULL)
		fprintf(stderr, "SmcOpenConnection: %s\n", error);
	assert(connection != NULL && id != NULL && id[0] != '\0');
	assert(strcmp(id, "unrestored-client") != 0);
	await_reply(connection, &completed, 1);
	/* Mutation remains intentionally discarded by this swallow proxy. */
	SmPropValue value = {4, "test"};
	SmProp property = {"probe-property", SmARRAY8, 1, &value};
	SmProp *list[] = {&property};
	char *names[] = {"probe-property"};

	for (unsigned i = 1; i <= 3; i++)
	{
		if (i == 2)
			SmcSetProperties(connection, 1, list);
		if (i == 3)
			SmcDeleteProperties(connection, 1, names);
		assert(SmcGetProperties(connection, properties, NULL));
		await_reply(connection, &returned, i);
	}
	assert(SmcCloseConnection(connection, 0, NULL) == SmcClosedNow);
	free(id);
	printf("session-client %s: initial-save, phase2, completion, "
		"three-property-replies passed\n", argv[1]);
	return 0;
}
