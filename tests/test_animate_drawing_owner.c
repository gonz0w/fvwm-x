/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/Picture.h"
#include "modules/FvwmAnimate/FvwmAnimate.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern struct ASAnimate Animate;
int __real_main(int, char **);
void DeadPipe(int);
void __real_free(void *);
void __real_InitGetConfigLine(int *, char *);
FvwmPicture *__real_PGetFvwmPicture(Display *, Window, char *, const char *,
	FvwmPictureAttributes);
Pixmap __real_XCreatePixmap(Display *, Drawable, unsigned int,
	unsigned int, unsigned int);
int __real_XFreePixmap(Display *, Pixmap);
GC __real_fvwmlib_XCreateGC(Display *, Drawable, unsigned long, XGCValues *);
int __real_XFreeGC(Display *, GC);
void __real_SendText(int *, const char *, unsigned long);
void __real_SendFinishedStartupNotification(int *);
void __real_SendUnlockNotification(int *);
static void *owners[32];
static unsigned int active, tiles, tile_frees, gc_count, gc_frees, refusals;
static unsigned int unlocks, stop_commands;
static Pixmap tile_owners[32];
static GC gc_owners[32];
static Bool next_tile, baseline, started;
static const char *scenario;

static void watch(void *pointer)
{
	if (pointer == NULL)
		return;
	for (unsigned int i = 0; i < active; i++)
		if (owners[i] == pointer)
			return;
	assert(active < 32);
	owners[active++] = pointer;
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

void __wrap_InitGetConfigLine(int *fd, char *match)
{
	watch(match); /* mname is owned; ModuleArgs and its name are borrowed. */
	__real_InitGetConfigLine(fd, match);
}

FvwmPicture *__wrap_PGetFvwmPicture(Display *display, Window window,
	char *path, const char *name, FvwmPictureAttributes attributes)
{
	FvwmPicture *picture = __real_PGetFvwmPicture(display, window, path,
		name, attributes);
	next_tile = picture != NULL;
	refusals += picture == NULL;
	return picture;
}

Pixmap __wrap_XCreatePixmap(Display *display, Drawable drawable,
	unsigned int width, unsigned int height, unsigned int depth)
{
	Pixmap result = __real_XCreatePixmap(display, drawable, width,
		height, depth);
	if (next_tile)
	{
		assert(tiles < 32 && result != None);
		tile_owners[tiles++] = result;
		next_tile = False;
	}
	return result;
}

int __wrap_XFreePixmap(Display *display, Pixmap pixmap)
{
	for (unsigned int i = 0; i < tiles; i++)
		if (tile_owners[i] == pixmap)
		{
			tile_owners[i] = None;
			tile_frees++;
			break;
		}
	return __real_XFreePixmap(display, pixmap);
}

GC __wrap_fvwmlib_XCreateGC(Display *display, Drawable drawable,
	unsigned long mask, XGCValues *values)
{
	GC result = __real_fvwmlib_XCreateGC(display, drawable, mask, values);
	unsigned long selected = GCFunction | GCForeground | GCLineWidth |
		GCSubwindowMode;
	if ((mask & selected) == selected)
	{
		assert(gc_count < 32);
		gc_owners[gc_count++] = result;
		watch(Animate.color);
		watch(Animate.pixmap);
	}
	return result;
}

int __wrap_XFreeGC(Display *display, GC gc)
{
	for (unsigned int i = 0; i < gc_count; i++)
		if (gc_owners[i] == gc)
		{
			gc_owners[i] = NULL;
			gc_frees++;
			break;
		}
	return __real_XFreeGC(display, gc);
}

void __wrap_SendText(int *fd, const char *text, unsigned long window)
{
	if (started && strncmp(text, "DestroyMenu MenuFvwmAnimate", 27) == 0)
	{
		assert(unlocks == 1 && active == 2);
		assert(gc_count - gc_frees == 1);
		stop_commands++;
	}
	__real_SendText(fd, text, window);
}

void __wrap_SendUnlockNotification(int *fd)
{
	unlocks++;
	__real_SendUnlockNotification(fd);
}

void __wrap_SendFinishedStartupNotification(int *fd)
{
	__real_SendFinishedStartupNotification(fd);
	started = True;
	FILE *file = fopen(getenv("MODULE_LIFECYCLE_READY"), "w");
	assert(file != NULL);
	fprintf(file, "ready\n");
	assert(fclose(file) == 0);
}

static void report(void)
{
	Bool missing = strcmp(scenario, "missing-signal") == 0;
	Bool stopped = strcmp(scenario, "tiled-stop") == 0;
	FILE *file = fopen(getenv("MODULE_LIFECYCLE_REPORT"), "w");
	assert(file != NULL);
	fprintf(file, "{\"tiles\":%u,\"tile_frees\":%u,\"gcs\":%u,"
		"\"gc_frees\":%u,\"active_strings\":%u,\"refusals\":%u,"
		"\"unlocks\":%u,\"stop_commands\":%u}\n", tiles,
		tile_frees, gc_count, gc_frees, active, refusals,
		unlocks, stop_commands);
	assert(fclose(file) == 0);
	assert(stop_commands == (stopped ? 1U : 0U));
	assert(unlocks == (missing ? 0U : 1U));
	assert(tiles == (missing ? 0U : 6U));
	assert(refusals == (missing ? 2U : 1U));
	assert(gc_count == (missing ? 2U : 9U));
	if (baseline)
		assert(active == 2 && gc_frees + 1 == gc_count &&
			tile_frees == (missing ? 0U : 2U));
	else
		assert(active == 0 && gc_frees == gc_count && tile_frees == tiles);
	/* Whole-module leak detection is disabled; selected owners are exact. */
}

int __wrap_main(int argc, char **argv)
{
	baseline = getenv("MODULE_LIFECYCLE_BASELINE") != NULL;
	scenario = getenv("MODULE_LIFECYCLE_SCENARIO");
	assert(scenario != NULL && atexit(report) == 0);
	assert(__real_main(argc, argv) == 0);
	/* The explicit terminal path must tolerate already retired resources. */
	DeadPipe(0);
	return 0;
}
