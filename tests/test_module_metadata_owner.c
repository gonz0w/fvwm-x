/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Flocale.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private link-time observers: all work still runs through the real module. */
int __real_main(int argc, char **argv);
XSizeHints *__real_XAllocSizeHints(void);
XWMHints *__real_XAllocWMHints(void);
Status __real_XStringListToTextProperty(char **list, int count,
	XTextProperty *property);
int __real_XFree(void *pointer);
void __real_XSetWMProperties(Display *display, Window window,
	XTextProperty *name, XTextProperty *icon, char **argv, int argc,
	XSizeHints *size, XWMHints *wm, XClassHint *class_hint);
Status __real_XGetWMSizeHints(Display *display, Window window,
	XSizeHints *hints, long *supplied, Atom property);
int __real_XMapWindow(Display *display, Window window);
int __real_XMapRaised(Display *display, Window window);

static void *owners[16];
static unsigned int active, allocated, released, size_allocations;
static unsigned int wm_allocations, conversions, publications, queries;
static unsigned int refused, minimum_rows, maximum_rows;
static bool baseline, mapped, ready_written;
static const char *scenario;
static Window ready_window;
static void ready(Display *display, Window window);
#ifdef METADATA_IDENT
static bool ident_drawn;
static int next_aspect;
#endif

static void watch(void *pointer)
{
	assert(pointer != NULL && active < 16);
	for (unsigned int i = 0; i < active; i++)
		assert(owners[i] != pointer);
	owners[active++] = pointer;
	allocated++;
}

XSizeHints *__wrap_XAllocSizeHints(void)
{
	XSizeHints *result = __real_XAllocSizeHints();

	size_allocations++;
	watch(result);
	return result;
}

XWMHints *__wrap_XAllocWMHints(void)
{
	XWMHints *result = __real_XAllocWMHints();

	wm_allocations++;
	watch(result);
	return result;
}

Status __wrap_XStringListToTextProperty(char **list, int count,
	XTextProperty *property)
{
	conversions++;
	if (conversions == refused)
	{
		/* Refusal gives the caller no valid property or value. */
		memset(property, 0xa5, sizeof(*property));
		return 0;
	}
	Status result = __real_XStringListToTextProperty(list, count, property);

	assert(result != 0);
	watch(property->value);
	return result;
}

int __wrap_XFree(void *pointer)
{
	for (unsigned int i = 0; i < active; i++)
	{
		if (owners[i] != pointer)
			continue;
		owners[i] = owners[--active];
		released++;
		break;
	}
	return __real_XFree(pointer);
}

void __wrap_XSetWMProperties(Display *display, Window window,
	XTextProperty *name, XTextProperty *icon, char **argv, int argc,
	XSizeHints *size, XWMHints *wm, XClassHint *class_hint)
{
	publications++;
#ifdef METADATA_SCRIPT
	assert(size != NULL && wm != NULL && class_hint != NULL);
	assert(size->width == 260 && size->height == 150);
	assert(size->min_width == 260 && size->max_width == 260);
	assert(size->min_height == 150 && size->max_height == 150);
	assert(size->base_width == 0 && size->base_height == 0);
	assert(size->flags == (PSize | PMinSize | PMaxSize | PBaseSize |
		(strcmp(scenario, "default-position") == 0 ? 0 : PPosition)));
	if (size->flags & PPosition)
		assert(size->x == 31 && size->y == 47);
	assert(wm->flags == (InputHint | StateHint) && wm->input == True &&
		wm->initial_state == NormalState);
	assert(strcmp(class_hint->res_class, "FvwmScript") == 0);
	if (refused)
		assert(name == NULL && icon == NULL);
	else
	{
		assert(name != NULL && name == icon &&
			strcmp((char *)name->value, "MetadataOwners") == 0);
	}
#endif
	__real_XSetWMProperties(display, window, name, icon, argv, argc,
		size, wm, class_hint);
}

Status __wrap_XGetWMSizeHints(Display *display, Window window,
	XSizeHints *hints, long *supplied, Atom property)
{
	Status result = __real_XGetWMSizeHints(display, window, hints,
		supplied, property);

#ifdef METADATA_IDENT
	assert(property == XA_WM_ZOOM_HINTS);
	queries++;
	assert((result != 0) == (strcmp(scenario, "absent") != 0));
	if (result)
		assert((*supplied & PAspect) != 0);
#endif
	return result;
}

#ifdef METADATA_IDENT
void __real_FlocaleDrawString(Display *display, FlocaleFont *font,
	FlocaleWinString *text, unsigned long flags);
void __wrap_FlocaleDrawString(Display *display, FlocaleFont *font,
	FlocaleWinString *text, unsigned long flags)
{
	if (strcmp(text->str, "Minimum aspect ratio:") == 0)
	{
		minimum_rows++;
		next_aspect = 1;
	}
	else if (strcmp(text->str, "Maximum aspect ratio:") == 0)
	{
		maximum_rows++;
		next_aspect = 2;
	}
	else if (next_aspect)
	{
		assert(strcmp(text->str, strcmp(scenario, "no-aspect") == 0 ?
			"0/0" : next_aspect == 1 ? "4/3" : "16/9") == 0);
		next_aspect = 0;
	}
	__real_FlocaleDrawString(display, font, text, flags);
	ident_drawn = true;
	if (mapped)
		ready(display, None);
}
#endif

static void ready(Display *display, Window window)
{
	const char *path = getenv("METADATA_READY");
	FILE *file;

	if (!mapped && window != None)
	{
		mapped = true;
		ready_window = window;
	}
	if (!mapped || ready_written)
		return;
#ifdef METADATA_IDENT
	/* A core-font Expose can arrive after map. Wait for drawing and both
	 * aspect values before allowing the harness to terminate us. */
	if (!ident_drawn || (strcmp(scenario, "absent") != 0 &&
		(minimum_rows == 0 || maximum_rows == 0 || next_aspect != 0)))
		return;
#endif
	ready_written = true;
	XSync(display, False);
	assert(path != NULL && (file = fopen(path, "w")) != NULL);
	fprintf(file, "%lu\n", ready_window);
	assert(fclose(file) == 0);
}

int __wrap_XMapWindow(Display *display, Window window)
{
	int result = __real_XMapWindow(display, window);

#if defined(METADATA_IDENT) || defined(METADATA_ICONMAN)
	ready(display, window);
#endif
	return result;
}

int __wrap_XMapRaised(Display *display, Window window)
{
	int result = __real_XMapRaised(display, window);

#ifdef METADATA_SCRIPT
	ready(display, window);
#endif
	return result;
}

static void report(void)
{
	const char *path = getenv("METADATA_REPORT");
	FILE *file;

	assert(ready_written && path != NULL);
	assert((file = fopen(path, "w")) != NULL);
#ifdef METADATA_SCRIPT
	assert(conversions == 1 && publications == 1 && queries == 0);
	assert(size_allocations == (baseline ? 1 : 0));
	assert(wm_allocations == (baseline ? 1 : 0));
	assert(active == (baseline ? 3 : 0));
	assert(allocated == (refused ? 0 : 1) + (baseline ? 2 : 0));
#elif defined(METADATA_IDENT)
	assert(queries == 1 && size_allocations == (baseline ? 1 : 0));
	assert(wm_allocations == 0);
	assert(active == (baseline && strcmp(scenario, "absent") == 0 ? 1 : 0));
	assert((minimum_rows > 0) == (strcmp(scenario, "absent") != 0));
	assert((maximum_rows > 0) == (strcmp(scenario, "absent") != 0));
#else
	assert(conversions == (refused == 1 ? 1 : 2));
	assert(publications == (refused ? 0 : 1));
	assert(size_allocations == 0 && wm_allocations == 0);
	assert(allocated == (refused == 1 ? 0 : refused == 2 ? 1 : 2));
	assert(active == (baseline && refused == 2 ? 1 : 0));
#endif
	assert(allocated == released + active);
	fprintf(file, "{\"allocated\":%u,\"released\":%u,\"retained\":%u,"
		"\"size_allocations\":%u,\"wm_allocations\":%u,"
		"\"publications\":%u,\"queries\":%u,"
		"\"minimum_rows\":%u,\"maximum_rows\":%u}\n",
		allocated, released, active, size_allocations, wm_allocations,
		publications, queries, minimum_rows, maximum_rows);
	assert(fclose(file) == 0);
	/* Reclaim old-control owners only after recording their retention. */
	while (active)
		__real_XFree(owners[--active]);
}

int __wrap_main(int argc, char **argv)
{
	const char *failure = getenv("METADATA_REFUSE");

	scenario = getenv("METADATA_SCENARIO");
	baseline = getenv("METADATA_BASELINE") != NULL;
	refused = failure == NULL ? 0 : (unsigned int)atoi(failure);
	assert(scenario != NULL && refused <= 2 && atexit(report) == 0);
	return __real_main(argc, argv);
}
