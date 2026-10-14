/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static FILE *owned_console;
static unsigned registrations, cleanups;
extern int box_x, box_y, box_w, box_h, current_desk;
FILE *__real_fopen(const char *, const char *);
int __real_fclose(FILE *);
unsigned __real_alarm(unsigned);
ssize_t __real_fvwmRead(int, void *, size_t);
ssize_t __real_fvwmWrite(int, const void *, size_t);
typedef void (*signal_handler)(int);
signal_handler __real_signal(int, signal_handler);

FILE *__wrap_fopen(const char *path, const char *mode)
{
	if (strcmp(path, "/dev/console") == 0)
	{
		assert(owned_console == NULL);
		owned_console = tmpfile();
		assert(owned_console != NULL);
		return owned_console;
	}
	return __real_fopen(path, mode);
}

#ifdef __linux__
/* Linux's configured large-file ABI redirects the module's fopen call. */
FILE *__wrap_fopen64(const char *path, const char *mode)
{
	return __wrap_fopen(path, mode);
}
#endif

signal_handler __wrap_signal(int number, signal_handler handler)
{
	assert(number == SIGPIPE || number == SIGALRM ||
		number == SIGTERM || number == SIGINT);
	assert(handler == fvwmSetTerminate);
	registrations++;
	return __real_signal(number, handler);
}

int __wrap_fclose(FILE *stream)
{
	if (stream == owned_console)
	{
		assert(registrations == 4 && cleanups++ == 0);
		owned_console = NULL;
		puts("rearrange-normal-flow-cleanup: passed");
		fflush(stdout);
	}
	return __real_fclose(stream);
}

unsigned __wrap_alarm(unsigned seconds)
{
	assert(seconds == 0 || seconds == 10);
	if (seconds == 10)
	{
		printf("rearrange-prepared-area: %d %d %d %d %d\n",
			box_x, box_y, box_w, box_h, current_desk);
		puts("rearrange-original-ten-second-deadline: armed");
		fflush(stdout);
	}
	return __real_alarm(seconds);
}

ssize_t __wrap_fvwmRead(int fd, void *buffer, size_t count)
{
	static int reported;

	if (!reported)
	{
		reported = 1;
		puts("rearrange-configuration-read: ready");
		fflush(stdout);
	}
	return __real_fvwmRead(fd, buffer, count);
}

ssize_t __wrap_fvwmWrite(int fd, const void *buffer, size_t count)
{
	static int reported;

	if (!reported)
	{
		reported = 1;
		puts("rearrange-command-write: ready");
		fflush(stdout);
	}
	return __real_fvwmWrite(fd, buffer, count);
}
