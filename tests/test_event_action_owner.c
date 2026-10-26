/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/Parse.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int __real_main(int, char **);
void *__real_fxmalloc(size_t);
char *__real_fxstrdup(const char *);
char *__real_GetNextSimpleOption(char *, char **);
void __real_free(void *);
static void *owners[128];
static unsigned int active, line_number, packet_number, sent;
static Bool refuse, partial;
static char line[256];
static const char *lines[] = {
	"*FvwmEventCmd old", "*FvwmEventCmd Prefix",
	"*FvwmEvent startup first", "*FvwmEvent startup begin",
	"*FvwmEvent shutdown finish", "*FvwmEvent unknown fallback",
	"*FvwmEvent new_desk old", "*FvwmEvent new_desk desk",
	"*FvwmEvent monitor_changed monitor",
	"*FvwmEvent new_page", "*FvwmEvent wrong_event rejected", NULL
};

static void watch(void *pointer)
{
	if (pointer == NULL)
		return;
	for (unsigned int i = 0; i < active; i++)
		if (owners[i] == pointer)
			return;
	assert(active < 128);
	owners[active++] = pointer;
}

void *__wrap_fxmalloc(size_t size)
{
	void *pointer = __real_fxmalloc(size);
	watch(pointer);
	return pointer;
}

char *__wrap_fxstrdup(const char *text)
{
	char *pointer = __real_fxstrdup(text);
	watch(pointer);
	return pointer;
}

char *__wrap_GetNextSimpleOption(char *text, char **option)
{
	char *next = __real_GetNextSimpleOption(text, option);
	watch(*option);
	return next;
}

void __wrap_free(void *pointer)
{
	for (unsigned int i = 0; i < active; i++)
		if (owners[i] == pointer)
		{
			owners[i] = owners[--active];
			break;
		}
	__real_free(pointer);
}

ModuleArgs *__wrap_ParseModuleArgs(int argc, char **argv, int alias)
{
	static ModuleArgs module;
	(void)argc; (void)argv; (void)alias;
	return refuse ? NULL : &module;
}

void __wrap_InitGetConfigLine(int *fd, char *match)
{
	(void)fd;
	assert(strcmp(match, "*FvwmEvent") == 0);
}

void __wrap_GetConfigLine(int *fd, char **text)
{
	(void)fd;
	if ((partial && line_number == 4) || lines[line_number] == NULL)
	{
		*text = NULL;
		return;
	}
	strcpy(line, lines[line_number++]);
	*text = line;
}

FvwmPacket *__wrap_ReadFvwmPacket(int fd)
{
	static unsigned long storage[8];
	FvwmPacket *packet = (FvwmPacket *)storage;
	(void)fd;
	if (partial || packet_number == 3)
		return NULL;
	memset(storage, 0, sizeof(storage));
	packet->size = 5;
	packet->type = packet_number == 0 ? M_NEW_DESK :
		packet_number == 1 ? MX_MONITOR_CHANGED : 0;
	packet_number++;
	return packet;
}

void __wrap_SendText(int *fd, const char *text, unsigned long window)
{
	static const char *expected[] = {"Prefix begin", "Prefix desk",
		"Prefix monitor monitor_changed", "Prefix fallback", "Prefix finish"};
	(void)fd; (void)window;
	assert(active > 0);
	if (partial)
		assert(sent == 0 && strncmp(text, expected[0], 12) == 0);
	else
	{
		assert(sent < 5);
		assert(strncmp(text, expected[sent], strlen(expected[sent])) == 0);
	}
	sent++;
}

void __wrap_SetMessageMask(int *fd, unsigned long mask)
{
	(void)fd; (void)mask;
}

void __wrap_SetSyncMask(int *fd, unsigned long mask)
{
	(void)fd; (void)mask;
}

void __wrap_SendFinishedStartupNotification(int *fd) { (void)fd; }
void __wrap_SendUnlockNotification(int *fd) { (void)fd; }

int __wrap_main(int argc, char **argv)
{
	char *arguments[] = {"FvwmEvent", "0", "1", "", "0", "0", NULL};
	Bool baseline = getenv("MODULE_LIFECYCLE_BASELINE") != NULL;
	(void)argc; (void)argv;
	for (unsigned int cycle = 0; cycle < (baseline ? 1U : 4U); cycle++)
	{
		line_number = packet_number = sent = 0;
		refuse = cycle == 2;
		partial = cycle == 3;
		assert(__real_main(6, arguments) == (refuse ? 1 : 0));
		assert(sent == (refuse ? 0U : partial ? 1U : 5U));
		fprintf(stderr, "actual Event cycle %u: selected retained=%u\n",
			cycle, active);
		assert(active == (baseline ? 7U : 0U));
	}
	/* Baseline controls observe retained records, then release fixture owners. */
	while (active)
		__real_free(owners[--active]);
	return 0;
}
