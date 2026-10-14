/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned registrations;
typedef void (*signal_handler)(int);
signal_handler __real_signal(int, signal_handler);
ssize_t __real_fvwmRead(int, void *, size_t);
ssize_t __real_fvwmWrite(int, const void *, size_t);

static void normal_return(void)
{
	assert(registrations == 3 && isTerminated);
	puts("backer-normal-flow-return: passed");
	fflush(stdout);
}

signal_handler __wrap_signal(int number, signal_handler handler)
{
	assert(number == SIGPIPE || number == SIGTERM || number == SIGINT);
	assert(handler == fvwmSetTerminate);
	if (registrations++ == 0)
	{
		assert(atexit(normal_return) == 0);
	}
	return __real_signal(number, handler);
}

ssize_t __wrap_fvwmRead(int fd, void *buffer, size_t count)
{
	static int reported;

	if (!reported)
	{
		reported = 1;
		puts("backer-configuration-read: ready");
		fflush(stdout);
	}
	return __real_fvwmRead(fd, buffer, count);
}

ssize_t __wrap_fvwmWrite(int fd, const void *buffer, size_t count)
{
	static int reported;
	const char response[] = ModuleFinishedStartupResponse;
	unsigned long length;

	if (!reported)
	{
		reported = 1;
		puts("backer-command-write: ready");
		fflush(stdout);
	}
	if (count >= 3 * sizeof(unsigned long))
	{
		memcpy(&length, (const char *)buffer + sizeof(unsigned long),
			sizeof(length));
		if (length == sizeof(response) - 1 &&
		    length <= count - 3 * sizeof(unsigned long) &&
		    memcmp((const char *)buffer + 2 * sizeof(unsigned long),
			response, length) == 0)
		{
			puts("backer-startup: ready");
			fflush(stdout);
		}
	}
	return __real_fvwmWrite(fd, buffer, count);
}
