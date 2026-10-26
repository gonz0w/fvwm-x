/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include "libs/Strings.h"
#include "modules/FvwmScript/Widgets/Tools.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern X11base *x11base;
extern ScriptProp *scriptprop;
extern FlocaleWinString *FwinString;
extern char *Scrapt, *default_cursor;
extern char *ScriptName;
extern int nbobj;
int __real_main(int, char **);
int __real_yyparse(void);
void __real_free(void *);
void __real_CopyStringWithQuotes(char **, const char *);
void __real_ChooseFunction(struct XObj *, char *);
Cursor __real_XCreateFontCursor(Display *, unsigned int);
int __real_XFreeCursor(Display *, Cursor);
GC __real_fvwmlib_XCreateGC(Display *, Drawable, unsigned long, XGCValues *);
int __real_XFreeGC(Display *, GC);
int __real_XFlush(Display *);
int __real_XUnmapWindow(Display *, Window);
int __real_XCloseDisplay(Display *);
XErrorHandler __real_XSetErrorHandler(XErrorHandler);
void __real_PrintXErrorAndCoredump(Display *, XErrorEvent *, char *);
void __real_PictureFreeColors(Display *, Colormap, Pixel *, int,
	unsigned long, Bool);
Atom __real_XInternAtom(Display *, const char *, Bool);
int __real_XSetSelectionOwner(Display *, Atom, Window, Time);
bool __real_script_widget_prepare(struct XObj *);
void __real_SendText(int *, const char *, unsigned long);
void __real_SendFinishedStartupNotification(int *);
extern RETSIGTYPE DeadPipe(int);

static void *volatile owners[128];
static volatile size_t outstanding, allocated, released;
static unsigned int cursors, cursor_frees, gc_frees, destroys, chosen;
static struct {
	struct XObj *object;
	void (*destroy)(struct XObj *);
} widgets[16];
static GC base_gc;
static Atom registered_selection;
static unsigned int selection_releases, prepared;
static bool dead_pipe_entered;
static XErrorHandler error_handler;
static bool checking_error;
static unsigned int live_error_labels, fallback_error_labels;
static const char *scenario, *child_id;

static void watch(void *pointer)
{
	if (pointer == NULL)
		return;
	for (size_t i = 0; i < outstanding; i++)
		if (owners[i] == pointer)
			return;
	assert(outstanding < 128);
	owners[outstanding++] = pointer;
	allocated++;
}

void __wrap_free(void *pointer)
{
	for (size_t i = 0; i < outstanding; i++)
		if (owners[i] == pointer)
		{
			owners[i] = owners[--outstanding];
			released++;
			break;
		}
	__real_free(pointer);
}

static void watch_base(void)
{
	watch(x11base);
	watch(x11base->font);
	watch(x11base->forecolor);
	watch(x11base->backcolor);
	watch(x11base->shadcolor);
	watch(x11base->hilicolor);
	watch(x11base->title);
	watch(x11base->icon);
	watch(x11base->cursor);
	watch(FwinString);
	watch(Scrapt);
	for (size_t i = 0; i < sizeof(x11base->TabScriptId) /
	    sizeof(x11base->TabScriptId[0]); i++)
		watch(x11base->TabScriptId[i]);
}

void __wrap_CopyStringWithQuotes(char **destination, const char *source)
{
	__real_CopyStringWithQuotes(destination, source);
	watch(*destination);
}

int __wrap_yyparse(void)
{
	watch_base();
	int result = __real_yyparse();

	assert(result == 0);
	watch(scriptprop->font);
	watch(scriptprop->forecolor);
	watch(scriptprop->backcolor);
	watch(scriptprop->shadcolor);
	watch(scriptprop->hilicolor);
	watch(scriptprop->titlewin);
	watch(scriptprop->icon);
	return result;
}

static void destroy_widget(struct XObj *object)
{
	assert(x11base != NULL && base_gc != NULL && gc_frees == 0);
	assert(x11base->font != NULL && x11base->title != NULL);
	for (unsigned int i = 0; i < chosen; i++)
		if (widgets[i].object == object)
		{
			destroys++;
			widgets[i].destroy(object);
			return;
		}
	assert(0);
}

void __wrap_ChooseFunction(struct XObj *object, char *type)
{
	watch_base();
	__real_ChooseFunction(object, type);
	assert(chosen < 16);
	widgets[chosen].object = object;
	widgets[chosen++].destroy = object->DestroyObj;
	object->DestroyObj = destroy_widget;
}

Cursor __wrap_XCreateFontCursor(Display *display, unsigned int shape)
{
	cursors++;
	return __real_XCreateFontCursor(display, shape);
}

int __wrap_XFreeCursor(Display *display, Cursor cursor)
{
	cursor_frees++;
	return __real_XFreeCursor(display, cursor);
}

GC __wrap_fvwmlib_XCreateGC(Display *display, Drawable drawable,
	unsigned long mask, XGCValues *values)
{
	GC gc = __real_fvwmlib_XCreateGC(display, drawable, mask, values);

	if (base_gc == NULL && x11base != NULL && drawable == x11base->win)
		base_gc = gc;
	return gc;
}

int __wrap_XFreeGC(Display *display, GC gc)
{
	assert(!dead_pipe_entered);
	if (gc == base_gc)
	{
		assert(!dead_pipe_entered && destroys == prepared && gc_frees == 0);
		gc_frees++;
	}
	return __real_XFreeGC(display, gc);
}

Atom __wrap_XInternAtom(Display *display, const char *name, Bool exists)
{
	assert(name != NULL);
	return __real_XInternAtom(display, name, exists);
}

int __wrap_XSetSelectionOwner(Display *display, Atom atom, Window owner,
	Time time)
{
	assert(!dead_pipe_entered && x11base != NULL && atom != None);
	if (owner == x11base->win)
	{
		watch_base();
		registered_selection = atom;
		if (child_id != NULL)
		{
			assert(x11base->TabScriptId[1] != child_id);
			assert(strcmp(x11base->TabScriptId[1], child_id) == 0);
			assert(x11base->TabScriptId[0] != NULL);
		}
	}
	else if (owner == x11base->root)
	{
		/* The established selection survives QuitFunc/message grace and
		 * is released before widget destruction. Partial startup skips it. */
		assert(atom == registered_selection && destroys == 0);
		selection_releases++;
	}
	return __real_XSetSelectionOwner(display, atom, owner, time);
}

int __wrap_XFlush(Display *display)
{
	assert(!dead_pipe_entered);
	return __real_XFlush(display);
}

int __wrap_XUnmapWindow(Display *display, Window window)
{
	assert(!dead_pipe_entered);
	return __real_XUnmapWindow(display, window);
}

int __wrap_XCloseDisplay(Display *display)
{
	XErrorEvent error = {.display = display, .error_code = BadValue};
	unsigned int before = live_error_labels + fallback_error_labels;

	assert(!dead_pipe_entered);
	assert(error_handler != NULL);
	/* Exercise the production handler at the same late dispatch boundary as
	 * XCloseDisplay, including active callbacks whose base stays retained. */
	checking_error = true;
	assert(error_handler(display, &error) == 0);
	checking_error = false;
	assert(live_error_labels + fallback_error_labels == before + 1);
	return __real_XCloseDisplay(display);
}

XErrorHandler __wrap_XSetErrorHandler(XErrorHandler handler)
{
	error_handler = handler;
	return __real_XSetErrorHandler(handler);
}

void __wrap_PrintXErrorAndCoredump(Display *display, XErrorEvent *error,
	char *label)
{
	if (!checking_error)
	{
		__real_PrintXErrorAndCoredump(display, error, label);
		return;
	}
	assert(error->error_code == BadValue && label != NULL);
	if (x11base != NULL && x11base->title != NULL)
	{
		assert(label == x11base->title);
		live_error_labels++;
	}
	else
	{
		assert(label == ScriptName);
		fallback_error_labels++;
	}
}

void __wrap_PictureFreeColors(Display *display, Colormap map, Pixel *pixels,
	int count, unsigned long planes, Bool no_limit)
{
	assert(!dead_pipe_entered);
	__real_PictureFreeColors(display, map, pixels, count, planes, no_limit);
}

bool __wrap_script_widget_prepare(struct XObj *object)
{
	bool result = __real_script_widget_prepare(object);

	prepared += result;
	return result;
}

void __wrap_SendText(int *pipes, const char *text, unsigned long window)
{
	if (x11base != NULL)
		watch_base();
	__real_SendText(pipes, text, window);
}

void __wrap_SendFinishedStartupNotification(int *pipes)
{
	watch_base();
	__real_SendFinishedStartupNotification(pipes);
	if (strcmp(scenario, "dead-pipe") == 0)
	{
		dead_pipe_entered = true;
		/* Invoke the production broken-X/pipe handler after full startup.
		 * Ordinary module EOF uses the termination path, not this handler. */
		DeadPipe(0);
		assert(0);
	}
}

static void report_owners(void)
{
	bool active = strcmp(scenario, "init-quit") == 0 ||
		strcmp(scenario, "event-quit") == 0 ||
		strcmp(scenario, "periodic-quit") == 0;
	bool dead = strcmp(scenario, "dead-pipe") == 0;
	bool partial = strncmp(scenario, "partial-", 8) == 0;
	FILE *report = fopen(getenv("SCRIPT_BASE_OWNER_REPORT"), "w");

	assert(report != NULL);
	assert(cursors == cursor_frees);
	assert(selection_releases == (registered_selection != None && !dead));
	assert(gc_frees == (base_gc != NULL && !dead && !active));
	assert(live_error_labels == (active && !dead));
	assert(fallback_error_labels == (!active && !dead));
	if (active)
	{
		assert(x11base != NULL && outstanding > 0);
		assert(x11base->title != NULL && x11base->quitfunc != NULL);
	}
	else
	{
		assert(x11base == NULL && outstanding == 0);
		assert(Scrapt == NULL && FwinString == NULL);
	}
	if (partial)
		assert(registered_selection == None && selection_releases == 0);
	fprintf(report, "{\"allocated\":%zu,\"freed\":%zu,\"retained\":%zu,"
		"\"cursor_create\":%u,\"cursor_free\":%u,\"gc_free\":%u,"
		"\"destroy_callbacks\":%u,\"selection_release\":%u,"
		"\"live_error_labels\":%u,\"fallback_error_labels\":%u}\n",
		allocated, released, outstanding, cursors, cursor_frees,
		gc_frees, destroys, selection_releases, live_error_labels,
		fallback_error_labels);
	assert(fclose(report) == 0);
}

int __wrap_main(int argc, char **argv)
{
	scenario = getenv("SCRIPT_BASE_SCENARIO");
	assert(scenario != NULL);
	if (argc > 7 && (unsigned char)argv[7][0] == 161)
		child_id = argv[7];
	assert(atexit(report_owners) == 0);
	return __real_main(argc, argv);
}
