/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/fsm.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

enum fault { NORMAL, NAME_FAIL, COOKIE_FAIL, NETWORK_FAIL, WATCH_FAIL,
	ENV_FAIL, AUTH_FAIL, PENDING_REJECT, PENDING_ACCEPTED };
static enum fault fault;
static unsigned name_calls, cookie_calls, published, helper_calls;
static unsigned watches, removals, released;
static int endpoints[2][2];
static FIceListenObj listeners[2];

static IceWatchProc connection_watch;
static IcePointer watch_context;
static unsigned accepted, closed_connections;
static struct
{
	int pipe[2];
	IceConnectStatus status;
	IcePointer view;
} connections[2];

IceConn __wrap_IceAcceptConnection(IceListenObj listener,
	IceAcceptStatus *status)
{
	(void)listener;
	unsigned index = accepted++;

	assert(index < 2 && pipe(connections[index].pipe) == 0);
	connections[index].status =
		fault == PENDING_ACCEPTED && index == 0 ?
		IceConnectAccepted : IceConnectPending;
	IceConn connection = (IceConn)&connections[index];
	connection_watch(connection, watch_context, True,
		&connections[index].view);
	*status = IceAcceptSuccess;
	return connection;
}

int __wrap_IceConnectionNumber(IceConn connection)
{
	for (unsigned i = 0; i < 2; i++)
		if (connection == (IceConn)&connections[i])
			return connections[i].pipe[0];
	assert(!"unknown ICE connection");
	return -1;
}

IceConnectStatus __wrap_IceConnectionStatus(IceConn connection)
{
	for (unsigned i = 0; i < 2; i++)
		if (connection == (IceConn)&connections[i])
			return connections[i].status;
	assert(!"unknown ICE connection");
	return IceConnectIOError;
}

IceCloseStatus __wrap_IceCloseConnection(IceConn connection)
{
	for (unsigned i = 0; i < 2; i++)
		if (connection == (IceConn)&connections[i])
		{
			connection_watch(connection, watch_context, False,
				&connections[i].view);
			assert(close(connections[i].pipe[0]) == 0);
			assert(close(connections[i].pipe[1]) == 0);
			closed_connections++;
			return IceClosedNow;
		}
	assert(!"unknown ICE connection");
	return IceClosedNow;
}

Status __wrap_SmsInitialize(const char *vendor, const char *release,
	SmsNewClientProc callback, SmPointer context,
	IceHostBasedAuthProc auth, int length, char *error)
{
	(void)vendor; (void)release; (void)callback; (void)context;
	(void)auth; (void)length; (void)error;
	return 1;
}

Status __wrap_IceListenForConnections(int *count, FIceListenObj **objects,
	int length, char *error)
{
	(void)length; (void)error;
	for (unsigned i = 0; i < 2; i++)
	{
		assert(pipe(endpoints[i]) == 0);
		listeners[i] = (FIceListenObj)&endpoints[i];
	}
	*count = 2;
	*objects = listeners;
	return 1;
}

int __wrap_IceGetListenConnectionNumber(FIceListenObj listener)
{
	return *(int *)listener;
}

void __wrap_IceFreeListenObjs(int count, FIceListenObj *objects)
{
	assert(count == 2 && objects == listeners);
	for (unsigned i = 0; i < 2; i++)
	{
		assert(close(endpoints[i][0]) == 0);
		assert(close(endpoints[i][1]) == 0);
	}
	released++;
}

char *__wrap_IceGetListenConnectionString(FIceListenObj listener)
{
	(void)listener;
	name_calls++;
	return fault == NAME_FAIL && name_calls == 3 ? NULL :
		strdup("local/constructor-test:1");
}

char *__wrap_IceGenerateMagicCookie(int length)
{
	cookie_calls++;
	assert(length == 16);
	if (fault == COOKIE_FAIL && cookie_calls == 3)
		return NULL;
	char *cookie = malloc(length);
	assert(cookie != NULL);
	memset(cookie, 'x', length);
	return cookie;
}

void __wrap_IceSetPaAuthData(int count, FIceAuthDataEntry *entries)
{
	assert(count == 2 && entries != NULL);
	assert(name_calls == 4 && cookie_calls == 4 && helper_calls == 1);
	for (int i = 0; i < count; i++)
		assert(entries[i].network_id && entries[i].auth_data);
	published++;
}

void __wrap_IceSetHostBasedAuthProc(FIceListenObj listener,
	IceHostBasedAuthProc callback)
{
	assert(listener != NULL && callback != NULL);
}

pid_t __wrap_fork(void)
{
	helper_calls++;
	return 12345;
}

pid_t __wrap_waitpid(pid_t child, int *status, int options)
{
	assert(child == 12345 && options == 0);
	*status = fault == AUTH_FAIL && helper_calls == 1 ? 1 << 8 : 0;
	return child;
}

char *__wrap_IceComposeNetworkIdList(int count, FIceListenObj *objects)
{
	assert(count == 2 && objects == listeners);
	return fault == NETWORK_FAIL ? NULL : strdup("local/complete:1");
}

Status __wrap_IceAddConnectionWatch(IceWatchProc callback, IcePointer context)
{
	assert(callback != NULL && context == NULL);
	if (fault == WATCH_FAIL)
		return 0;
	connection_watch = callback;
	watch_context = context;
	watches++;
	return 1;
}

void __wrap_IceRemoveConnectionWatch(IceWatchProc callback, IcePointer context)
{
	assert(callback != NULL && context == NULL && watches == 1);
	removals++;
}

int __real_setenv(const char *, const char *, int);
int __wrap_setenv(const char *name, const char *value, int overwrite)
{
	assert(strcmp(name, "SESSION_MANAGER") == 0);
	if (fault == ENV_FAIL)
	{
		errno = ENOMEM;
		return -1;
	}
	return __real_setenv(name, value, overwrite);
}

static unsigned temporary_files(const char *path)
{
	unsigned count = 0;
	DIR *dir = opendir(path);
	struct dirent *entry;
	assert(dir != NULL);
	while ((entry = readdir(dir)) != NULL)
		if (strncmp(entry->d_name, ".fsm-", 5) == 0)
			count++;
	assert(closedir(dir) == 0);
	return count;
}

int main(int argc, char **argv)
{
	char directory[] = "/tmp/fvwm-session-constructor-XXXXXX";
	struct pollfd ready[2];

	assert(argc == 2);
	fault = atoi(argv[1]);
	assert(fault >= NORMAL && fault <= PENDING_ACCEPTED);
	assert(mkdtemp(directory) != NULL);
	assert(__real_setenv("SM_SAVE_DIR", directory, 1) == 0);
	assert(__real_setenv("SESSION_MANAGER", "prior-manager", 1) == 0);
	if (fault == NORMAL || fault >= PENDING_REJECT)
	{
		assert(fsm_init("constructor-test"));
		assert(published == 2 && watches == 1 && !released);
		assert(strcmp(getenv("SESSION_MANAGER"),
			"local/complete:1") == 0);
		assert(fsm_pollfds(ready, 2) == 2);
		assert(temporary_files(directory) == 1);
		if (fault >= PENDING_REJECT)
		{
			fd_set inputs;

			FD_ZERO(&inputs);
			FD_SET(endpoints[0][0], &inputs);
			FD_SET(endpoints[1][0], &inputs);
			assert(fsm_process(&inputs) && accepted == 2);
			FD_ZERO(&inputs);
			if (fault == PENDING_ACCEPTED)
			{
				/* This accepted connection is no longer pending. */
				__wrap_IceCloseConnection((IceConn)&connections[0]);
			}
			else
			{
				/* The status sweep and its close watch both retire it. */
				connections[0].status = IceConnectRejected;
			}
			assert(fsm_process(&inputs));
			assert(closed_connections == 1);
			connections[1].status = IceConnectRejected;
			assert(!fsm_process(&inputs));
			assert(closed_connections == 2);
			assert(fsm_pollfds(ready, 2) == 2);
		}
		fsm_close();
	}
	else
	{
		assert(!fsm_init("constructor-test"));
		assert(strcmp(getenv("SESSION_MANAGER"), "prior-manager") == 0);
		assert(released == 1 && fsm_pollfds(ready, 2) == 0);
		assert(removals == (fault == ENV_FAIL));
		if (fault == NAME_FAIL || fault == COOKIE_FAIL ||
		    fault == AUTH_FAIL)
			assert(published == 0);
		assert(temporary_files(directory) == 0);
		/* Failure leaves a usable retry path, with no stale watch. */
		fault = NORMAL;
		name_calls = cookie_calls = helper_calls = watches = 0;
		assert(fsm_init("constructor-retry"));
		assert(watches == 1 && fsm_pollfds(ready, 2) == 2);
		fsm_close();
	}
	assert(temporary_files(directory) == 0 && rmdir(directory) == 0);
	printf("session-startup fault=%s: passed\n", argv[1]);
	return 0;
}
