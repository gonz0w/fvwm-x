/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include "libs/ColorUtils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern X11base *x11base;
extern ScriptProp *scriptprop;
extern ModuleArgs *module;
extern char *ScriptName, *ScriptBaseName, *default_cursor, *Scrapt;
extern FlocaleWinString *FwinString;
extern void OpenWindow(void);
extern void InitCom(void);
extern char **TabVVar;
extern void (*TabCom[])(int, long *);

static unsigned int cursors, cursor_frees, attachments, gc_frees, colors;
static void *volatile owners[64];
static volatile size_t outstanding;
void __real_free(void *pointer);

static void watch(void *pointer)
{
	assert(pointer != NULL && outstanding < 64);
	owners[outstanding++] = pointer;
}

void __wrap_free(void *pointer)
{
	for (size_t i = 0; i < outstanding; i++)
		if (owners[i] == pointer)
		{
			owners[i] = owners[--outstanding];
			break;
		}
	__real_free(pointer);
}

static char *owned(const char *value)
{
	char *result = strdup(value);

	watch(result);
	return result;
}

Cursor __wrap_XCreateFontCursor(Display *display, unsigned int shape)
{
	(void)display;
	(void)shape;
	return 10 + ++cursors;
}

int __wrap_XFreeCursor(Display *display, Cursor cursor)
{
	(void)display;
	assert(cursor == 10 + cursors && attachments == cursors);
	cursor_frees++;
	return 1;
}

Window __wrap_XCreateWindow(Display *display, Window parent, int x, int y,
	unsigned int width, unsigned int height, unsigned int border, int depth,
	unsigned int class, Visual *visual, unsigned long mask,
	XSetWindowAttributes *attributes)
{
	(void)display; (void)parent; (void)x; (void)y; (void)width;
	(void)height; (void)border; (void)depth; (void)class; (void)visual;
	if (mask & CWCursor)
	{
		assert(attributes->cursor == 10 + cursors);
		attachments++;
	}
	return 7;
}

GC __wrap_fvwmlib_XCreateGC(Display *display, Drawable drawable,
	unsigned long mask, XGCValues *values)
{
	(void)display; (void)drawable; (void)mask; (void)values;
	return (GC)&gc_frees;
}

int __wrap_XFreeGC(Display *display, GC gc)
{
	(void)display;
	assert(gc == (GC)&gc_frees);
	gc_frees++;
	return 1;
}

Bool __wrap_fvwmlib_get_color(char *name, Pixel *pixel)
{
	*pixel = 0; /* Allocated black must still be released. */
	return strcmp(name, "refused") != 0;
}

Pixel __wrap_GetColor(char *name)
{
	(void)name;
	return 0;
}

void __wrap_PictureFreeColors(Display *display, Colormap map, Pixel *pixels,
	int count, unsigned long planes, Bool no_limit)
{
	(void)display; (void)map; (void)planes; (void)no_limit;
	assert(count == 1 && *pixels == 0);
	colors++;
}

int __wrap_XSelectInput(Display *display, Window window, long mask)
{
	(void)display; (void)window; (void)mask;
	return 1;
}

Status __wrap_XStringListToTextProperty(char **list, int count,
	XTextProperty *property)
{
	(void)list; (void)count; (void)property;
	return 0;
}

void __wrap_XSetWMProperties(Display *display, Window window,
	XTextProperty *name, XTextProperty *icon, char **argv, int argc,
	XSizeHints *size, XWMHints *hints, XClassHint *class_hint)
{
	(void)display; (void)window; (void)name; (void)icon; (void)argv;
	(void)argc; (void)size; (void)hints; (void)class_hint;
}

Atom __wrap_XInternAtom(Display *display, const char *name, Bool exists)
{
	(void)display; (void)exists;
	assert(name != NULL);
	return 9;
}

Status __wrap_XSetWMProtocols(Display *display, Window window,
	Atom *protocols, int count)
{
	(void)display; (void)window; (void)protocols; (void)count;
	return 1;
}

void __wrap_SetWindowBackground(Display *display, Window window,
	int width, int height, colorset_t *colorset, unsigned int depth,
	GC gc, Bool clear)
{
	(void)display; (void)window; (void)width; (void)height;
	(void)colorset; (void)depth; (void)gc; (void)clear;
}

static void make_base(const char *cursor)
{
	x11base = calloc(1, sizeof(*x11base));
	watch(x11base);
	x11base->colorset = -1;
	x11base->font = owned("fixed");
	x11base->forecolor = owned("black");
	x11base->backcolor = owned("refused");
	x11base->shadcolor = owned("black");
	x11base->hilicolor = owned("black");
	x11base->icon = owned("icon");
	x11base->title = owned("BaseOwners");
	x11base->cursor = owned(cursor);
	default_cursor = x11base->cursor;
	/* Father, parent and the final launch slot all share one owner rule. */
	x11base->TabScriptId[0] = owned("parent");
	x11base->TabScriptId[1] = owned("self");
	x11base->TabScriptId[998] = owned("child");
	x11base->TabArg[0] = "borrowed argument";
}

int __wrap_main(int argc, char **argv)
{
	ModuleArgs arguments = {.name = "base-owner"};

	(void)argc; (void)argv;
	module = &arguments;
	ScriptName = ScriptBaseName = "base-owner";
	make_base("left_ptr");
	OpenWindow();
	fprintf(stderr, "actual OpenWindow: cursor_create=%u attach=%u free=%u\n",
		cursors, attachments, cursor_frees);
	/* This same assertion fails when linked to unchanged baseline objects. */
	assert(cursors == 1 && attachments == 1 && cursor_frees == 1);
#ifndef SCRIPT_BASE_BASELINE_CONTROL
	assert(default_cursor == NULL && x11base->cursor == NULL);
	watch(Scrapt);
	FwinString = calloc(1, sizeof(*FwinString));
	watch(FwinString);
	FwinString->str = "borrowed text";
	FwinString->gc = x11base->gc;
	assert(!script_base_release(1, True));
	assert(x11base != NULL && gc_frees == 0 && colors == 0);
	assert(script_base_release(0, True));
	assert(x11base == NULL && Scrapt == NULL && FwinString == NULL);
	assert(outstanding == 0 && gc_frees == 1 && colors == 3);
	assert(script_base_release(0, True));
	assert(gc_frees == 1 && colors == 3);

	make_base("invalid-cursor");
	OpenWindow();
	watch(Scrapt);
	assert(cursors == 1 && cursor_frees == 1);
	/* The real ChangeColorset instruction consumes the same color owner. */
	InitCom();
	char *variables[] = {"0", "0"};
	long instruction[] = {0, 1};
	TabVVar = variables;
	TabCom[24](2, instruction);
	TabVVar = NULL;
	assert(x11base->colorset == 0 && colors == 6);
	assert(script_base_release(0, True));
	assert(outstanding == 0 && colors == 6 && gc_frees == 2);

	make_base("left_ptr");
	scriptprop = calloc(1, sizeof(*scriptprop));
	scriptprop->titlewin = owned("not transferred");
	assert(script_base_release(0, False));
	assert(outstanding == 0 && colors == 6 && gc_frees == 2);
	assert(script_base_release(0, False));
	free(scriptprop);
	scriptprop = NULL;
#endif
	puts("actual Script base owners: cursor, colors, GC, strings, IDs, "
		"partial/repeated release and active-depth retention passed");
	return 0;
}
