/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "modules/FvwmIconMan/FvwmIconMan.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void __real_free(void *pointer);
static void *owners[9];
static unsigned int active;

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

int __wrap_main(int argc, char **argv)
{
	Bool baseline = getenv("MODULE_OWNER_BASELINE") != NULL;
	WinData *win = new_windata();
	char **fields[] = {&win->resname, &win->classname, &win->iconname,
		&win->titlename, &win->display_string, &win->visible_name,
		&win->visible_icon_name, &win->monitor};

	(void)argc; (void)argv;
	OpenConsole(NULL);
	owners[active++] = win;
	for (unsigned int i = 0; i < 8; i++)
	{
		copy_string(fields[i], "first");
		/* Exercise the production producer's replacement contract too. */
		copy_string(fields[i], "replacement");
		owners[active++] = *fields[i];
	}
	free_windata(win);
	fprintf(stderr, "actual IconMan record: retained=%u of 9 owners\n",
		active);
	assert(active == (baseline ? 3 : 0));
	while (active)
		__real_free(owners[--active]);
	win = new_windata();
	free_windata(win); /* partial records retain no strings */
	return 0;
}
