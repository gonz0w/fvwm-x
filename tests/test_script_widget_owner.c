/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include "modules/FvwmScript/Widgets/Tools.h"
#include "libs/flocale_font_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *widget_types[] = {
	"PushButton", "RadioButton", "ItemDraw", "CheckBox", "TextField",
	"HScrollBar", "VScrollBar", "PopupMenu", "Rectangle", "MiniScroll",
	"SwallowExec", "HDipstick", "VDipstick", "List", "Menu"
};
static unsigned int font_releases, gc_releases, window_releases;
static unsigned int cursor_creates, cursor_attaches, cursor_releases;
static Cursor pending_cursor;
static FlocaleFont shared_font = {.height = 12, .ascent = 9, .descent = 3};
static FlocaleFont rejected_font = {.height = 65536};
static int font_mode;
static unsigned int shared_refs, rejected_refs, named_releases;
static unsigned int icon_releases, pixel_releases, client_kills, draws;
static void *owners[512];
static unsigned int owner_count, owner_frees;
static GC borrowed_gc;

void __real_free(void *pointer);

static void watch(void *pointer)
{
	assert(pointer != NULL && owner_count < 512);
	owners[owner_count++] = pointer;
}

void __wrap_free(void *pointer)
{
	for (unsigned int i = 0; i < owner_count; i++)
	{
		if (pointer == owners[i])
		{
			owners[i] = owners[--owner_count];
			owner_frees++;
			break;
		}
	}
	__real_free(pointer);
}

FlocaleFont *__wrap_FlocaleLoadFont(Display *display, char *name, char *module)
{
	(void)display;
	(void)name;
	(void)module;
	if (font_mode == 2)
		return NULL;
	if (font_mode == 1)
	{
		rejected_refs++;
		return &rejected_font;
	}
	shared_refs++;
	return &shared_font;
}

int __wrap_FlocaleTextWidth(FlocaleFont *font, char *text, int length)
{
	(void)font;
	(void)text;
	return length * 6;
}

void __wrap_PictureFreeColors(Display *display, Colormap cmap, Pixel *pixels,
	int count, unsigned long planes, Bool no_limit)
{
	(void)display;
	(void)cmap;
	(void)planes;
	(void)no_limit;
	assert(pixels != NULL);
	if (count == 1)
		named_releases++;
	else
	{
		assert(count == 2);
		pixel_releases++;
	}
}

int __wrap_XFreePixmap(Display *display, Pixmap pixmap)
{
	(void)display;
	assert(pixmap != None);
	icon_releases++;
	return 1;
}

int __wrap_XClearWindow(Display *display, Window window)
{
	(void)display;
	assert(window != None);
	return 1;
}

void __wrap_FlocaleUnloadFont(Display *display, FlocaleFont *font)
{
	(void)display;
#ifndef SCRIPT_WIDGET_BASELINE_CONTROL
	if (font == NULL)
		return;
	if (font == &shared_font)
	{
		assert(shared_refs > 0);
		shared_refs--;
	}
	else if (font == &rejected_font)
	{
		assert(rejected_refs > 0);
		rejected_refs--;
	}
	else
		assert(0);
#else
	assert(font != NULL);
#endif
	font_releases++;
}

int __wrap_XFreeGC(Display *display, GC gc)
{
	(void)display;
	assert(gc != NULL);
#ifndef SCRIPT_WIDGET_BASELINE_CONTROL
	assert(gc != borrowed_gc);
#endif
	gc_releases++;
	return 1;
}

int __wrap_XDestroyWindow(Display *display, Window window)
{
	(void)display;
	assert(window != None);
	window_releases++;
	return 1;
}

int __wrap_XKillClient(Display *display, XID resource)
{
	(void)display;
	assert(resource != None);
#ifndef SCRIPT_WIDGET_BASELINE_CONTROL
	client_kills++;
#endif
	return 1;
}

Cursor __wrap_XCreateFontCursor(Display *display, unsigned int shape)
{
	(void)display;
	(void)shape;
	pending_cursor = 100 + ++cursor_creates;
	return pending_cursor;
}

int __wrap_XFreeCursor(Display *display, Cursor cursor)
{
	(void)display;
	assert(cursor == pending_cursor);
	assert(cursor_attaches == cursor_creates);
	cursor_releases++;
	return 1;
}

Window __wrap_XCreateWindow(Display *display, Window parent, int x, int y,
	unsigned int width, unsigned int height, unsigned int border, int depth,
	unsigned int class, Visual *visual, unsigned long mask,
	XSetWindowAttributes *attributes)
{
	(void)display;
	(void)parent;
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	(void)border;
	(void)depth;
	(void)class;
	(void)visual;
	if (mask & CWCursor)
	{
		assert(attributes->cursor == pending_cursor);
		cursor_attaches++;
	}
	return 7;
}

GC __wrap_fvwmlib_XCreateGC(Display *display, Drawable drawable,
	unsigned long mask, XGCValues *values)
{
	(void)display;
	(void)drawable;
	(void)mask;
	(void)values;
	return (GC)&gc_releases;
}

Pixel __wrap_GetColor(char *name)
{
	(void)name;
	return 3;
}

int __wrap_XSetForeground(Display *display, GC gc, unsigned long pixel)
{
	(void)display;
	(void)gc;
	(void)pixel;
	return 1;
}

int __wrap_XSetBackground(Display *display, GC gc, unsigned long pixel)
{
	return __wrap_XSetForeground(display, gc, pixel);
}

int __wrap_XSetLineAttributes(Display *display, GC gc, unsigned int width,
	int line, int cap, int join)
{
	(void)display;
	(void)gc;
	(void)width;
	(void)line;
	(void)cap;
	(void)join;
	return 1;
}

int __wrap_XSelectInput(Display *display, Window window, long mask)
{
	(void)display;
	(void)window;
	(void)mask;
	return 1;
}

#ifdef SCRIPT_WIDGET_BASELINE_CONTROL
int __wrap_main(int argc, char **argv)
{
	X11base base = {0};
	Window parent = 1;
	FlocaleFont font = {.height = 12, .ascent = 9, .descent = 3};
	unsigned int missed_fonts = 0;
	const char *cursor_types[] = {"CheckBox", "RadioButton", "List",
		"HScrollBar", "VScrollBar", "TextField"};

	(void)argc;
	(void)argv;
	x11base = &base;
	for (unsigned int i = 0; i < 15; i++)
	{
		struct XObj object = {.Ffont = &font, .win = 7,
			.swallow_placeholder = 7, .gc = (GC)&font};
		unsigned int before = font_releases;

		ChooseFunction(&object, (char *)widget_types[i]);
		object.DestroyObj(&object);
		if (font_releases != before + 1)
		{
			fprintf(stderr, "actual destructor missing font: %s\n",
				widget_types[i]);
			missed_fonts++;
		}
	}
	for (unsigned int i = 0; i < 6; i++)
	{
		struct XObj object = {.Ffont = &font, .colorset = -1,
			.ParentWin = &parent, .width = 100, .height = 40,
			.title = "", .value3 = 10};

		ChooseFunction(&object, (char *)cursor_types[i]);
		object.InitObj(&object);
	}
	fprintf(stderr, "actual widget owners: missing_fonts=%u "
		"cursor_create=%u attach=%u free=%u\n", missed_fonts,
		cursor_creates, cursor_attaches, cursor_releases);
	assert(missed_fonts == 0 && cursor_creates == 6 &&
		cursor_attaches == 6 && cursor_releases == 6);
	puts("actual Script widget owners release fonts and cursor references");
	return 0;
}
#else
extern int TabIdObj[1001];
extern char **TabVVar;
extern void (*TabCom[])(int, long *);
void InitCom(void);

static char *owned_string(const char *value)
{
	char *result = strdup(value);

	watch(result);
	return result;
}

static struct XObj *make_widget(unsigned int index)
{
	struct XObj *object = calloc(1, sizeof(*object));

	watch(object);
	object->id = index + 1;
	object->colorset = -1;
	object->title = owned_string("Label|One|Two");
	object->swallow = owned_string("");
	object->icon = owned_string("icon");
	object->font = owned_string("fixed");
	object->forecolor = owned_string("black");
	object->backcolor = owned_string("white");
	object->shadcolor = owned_string("black");
	object->hilicolor = owned_string("white");
	object->width = 100;
	object->height = 40;
	object->value3 = 10;
	object->gc = borrowed_gc;
	ChooseFunction(object, (char *)widget_types[index]);
	return object;
}

static void no_draw(struct XObj *object, XEvent *event)
{
	(void)object;
	(void)event;
	draws++;
}

static void change_font(struct XObj *object, int mode, bool accepted)
{
	long arguments[] = {0, 1};
	char id[16];
	char *variables[] = {id, "replacement"};
	FlocaleFont *old_font = object->Ffont;
	uint64_t revision = object->resource_revision;
	unsigned int old_draws = draws;

	snprintf(id, sizeof(id), "%d", object->id);
	TabVVar = variables;
	font_mode = mode;
	object->DrawObj = no_draw;
	TabCom[9](2, arguments);
	if (accepted)
	{
		assert(object->Ffont == &shared_font);
		assert(object->resource_revision == revision + 1);
		assert(draws == old_draws + 1);
		watch(object->font);
	}
	else
	{
		assert(object->Ffont == old_font);
		assert(object->resource_revision == revision);
		assert(draws == old_draws);
	}
	assert(rejected_refs == 0);
	font_mode = 0;
	TabVVar = NULL;
}

static void exercise_all_widgets(void)
{
	Window parent = 1;
	unsigned int fonts = font_releases, gcs = gc_releases;
	unsigned int windows = window_releases, colors = named_releases;
	unsigned int pixmaps = icon_releases, pixels = pixel_releases;
	unsigned int frees = owner_frees, cursors = cursor_creates;

	nbobj = 15;
	for (unsigned int i = 0; i < 15; i++)
	{
		struct XObj *object = make_widget(i);
		char *swallow = object->swallow;

		tabxobj[i] = object;
		TabIdObj[i + 1] = i;
		object->ParentWin = &parent;
		assert(script_widget_prepare(object));
		/* Empty Swallow completes with its owned placeholder. */
		if (object->TypeWidget == SwallowExec)
			object->swallow = NULL;
		object->InitObj(object);
		object->swallow = swallow;
		object->iconPixmap = 21;
		object->icon_maskPixmap = 22;
		object->icon_alphaPixmap = 23;
		object->alloc_pixels = malloc(2 * sizeof(Pixel));
		watch(object->alloc_pixels);
		object->nalloc_pixels = 2;
	}
	assert(shared_refs == 10);
	assert(cursor_creates == cursors + 9 &&
		cursor_attaches == cursor_creates &&
		cursor_releases == cursor_creates);
	for (unsigned int i = 0; i < 15; i++)
	{
		change_font(tabxobj[i], 0, true);
		change_font(tabxobj[i], 0, true); /* Shared cached reference. */
		change_font(tabxobj[i], 1, false); /* Invalid metrics. */
		change_font(tabxobj[i], 2, false); /* Load refused. */
	}
	assert(shared_refs == 15 && rejected_refs == 0);
	for (unsigned int i = 0; i < 15; i++)
		tabxobj[i]->DestroyObj(tabxobj[i]);
	/* The production ShutdownX uses these same two passes. */
	assert(owner_count == 150);
	for (unsigned int i = 0; i < 15; i++)
	{
		script_widget_release(&tabxobj[i], true);
		script_widget_release(&tabxobj[i], true);
		assert(tabxobj[i] == NULL);
	}
	assert(shared_refs == 0 && owner_count == 0);
	assert(font_releases == fonts + 55);
	assert(gc_releases == gcs + 14 && window_releases == windows + 15);
	assert(named_releases == colors + 60);
	assert(icon_releases == pixmaps + 45 && pixel_releases == pixels + 15);
	/* 150 fields/records/vectors plus 30 font-name replacements. */
	assert(owner_frees == frees + 180);
}

static void exercise_partial_and_borrowed(void)
{
	unsigned int fonts = font_releases, gcs = gc_releases;
	unsigned int colors = named_releases, windows = window_releases;
	unsigned int kills = client_kills;
	unsigned int pixmaps = icon_releases, pixels = pixel_releases;
	struct XObj *object = NULL;

	script_widget_release(&object, false);
	object = calloc(1, sizeof(*object));
	watch(object);
	script_widget_release(&object, false);
	assert(object == NULL && owner_count == 0);
	for (unsigned int stage = 0; stage < 4; stage++)
	{
		object = make_widget(0);
		if (stage == 1)
			assert(script_widget_prepare(object));
		if (stage == 2)
		{
			font_mode = 1;
			assert(!script_widget_prepare(object));
		}
		if (stage == 3)
		{
			font_mode = 2;
			assert(!script_widget_prepare(object));
		}
		font_mode = 0;
		/* Icons load before prepare, including a refused load. */
		object->iconPixmap = 21;
		object->icon_maskPixmap = 22;
		object->icon_alphaPixmap = 23;
		object->alloc_pixels = malloc(2 * sizeof(Pixel));
		watch(object->alloc_pixels);
		object->nalloc_pixels = 2;
		script_widget_release(&object, false);
		script_widget_release(&object, false);
		assert(object == NULL && owner_count == 0 && shared_refs == 0);
	}
	assert(font_releases == fonts + 2 && rejected_refs == 0);
	assert(icon_releases == pixmaps + 12 && pixel_releases == pixels + 4);
	assert(gc_releases == gcs && named_releases == colors &&
		window_releases == windows);
	for (unsigned int variant = 0; variant < 3; variant++)
	{
		object = make_widget(10);
		object->colorset = 0; /* Borrowed colorset refs and base GC. */
		object->swallow_placeholder = variant == 2 ? None : 7;
		object->win = variant == 1 ? 8 : object->swallow_placeholder;
		object->DestroyObj(object);
		object->DestroyObj(object);
		script_widget_release(&object, true);
		assert(object == NULL && owner_count == 0);
	}
	assert(client_kills == kills + 1 && window_releases == windows + 2);
	assert(gc_releases == gcs && named_releases == colors);
	/* Ordinary widgets borrow colorset references too. */
	object = make_widget(0);
	object->colorset = 0;
	object->gc = (GC)&draws;
	object->win = 9;
	object->DestroyObj(object);
	object->DestroyObj(object);
	script_widget_release(&object, true);
	assert(gc_releases == gcs + 1 && window_releases == windows + 3);
	assert(named_releases == colors && owner_count == 0);
}

int __wrap_main(int argc, char **argv)
{
	X11base base = {0};

	(void)argc;
	(void)argv;
	x11base = &base;
	borrowed_gc = (GC)&base;
	base.gc = borrowed_gc;
	ScriptName = "widget-owner";
	InitCom();
	exercise_all_widgets();
	exercise_partial_and_borrowed();
	puts("actual Script widget owners: fifteen types, fonts, cursor refs, "
		"partial slots and borrowed resources release exactly once");
	return 0;
}
#endif
