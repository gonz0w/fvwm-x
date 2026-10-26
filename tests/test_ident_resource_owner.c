/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Flocale.h"
#include "libs/ColorUtils.h"
#include "libs/monitor_types.h"
#include "libs/flocale_font_types.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int __real_main(int argc, char **argv);
void __real_free(void *pointer);
void __real_CopyString(char **destination, const char *source);
void __real_CopyStringWithQuotes(char **destination, const char *source);
char *__real_fxstrdup(const char *source);
void __real_FlocaleAllocateWinString(FlocaleWinString **text);
FlocaleFont *__real_FlocaleLoadFont(Display *, char *, char *);
void __real_FlocaleUnloadFont(Display *, FlocaleFont *);
GC __real_fvwmlib_XCreateGC(Display *, Drawable, unsigned long, XGCValues *);
int __real_XFreeGC(Display *, GC);
int __real_XMapWindow(Display *, Window);
Bool __real_fvwmlib_get_color(char *, Pixel *);
Pixel __real_GetColor(char *);
void __real_PictureFreeColors(Display *, Colormap, Pixel *, int,
	unsigned long, Bool);
void *__real_fxmalloc(size_t size);
int __real_FlocaleTextWidth(FlocaleFont *, char *, int);
static void *owners[512];
static unsigned int active, strings, monitor_copies, maps, fonts, font_frees;
static unsigned int gcs, gc_frees, colors, color_frees, nodes;
static Bool baseline, width_seen, building, config_watch, need_popup_gc;
static GC popup_gcs[3];
static Pixel named_pixels[6];
static Bool named_pixel_released[6];
static Window target;
static const char *scenario;

static void watch(void *pointer)
{
	assert(pointer != NULL && active < 512);
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

void __wrap_CopyString(char **destination, const char *source)
{
	__real_CopyString(destination, source);
	if (config_watch)
	{
		watch(*destination);
		strings++;
	}
}

void __wrap_CopyStringWithQuotes(char **destination, const char *source)
{
	__real_CopyStringWithQuotes(destination, source);
	if (config_watch)
	{
		watch(*destination);
		strings++;
	}
}

struct monitor *__wrap_monitor_by_output(int output)
{
	static struct screen_info screen = {.name = "owner-monitor"};
	static struct monitor monitor = {.si = &screen};

	(void)output;
	return &monitor;
}

char *__wrap_fxstrdup(const char *source)
{
	char *result = __real_fxstrdup(source);

	if (strcmp(source, "owner-monitor") == 0)
	{
		watch(result);
		monitor_copies++;
	}
	return result;
}

void __wrap_FlocaleAllocateWinString(FlocaleWinString **text)
{
	__real_FlocaleAllocateWinString(text);
	watch(*text);
	config_watch = True;
}

int __wrap_FlocaleTextWidth(FlocaleFont *font, char *text, int length)
{
	width_seen = building;
	return __real_FlocaleTextWidth(font, text, length);
}

void *__wrap_fxmalloc(size_t size)
{
	void *result = __real_fxmalloc(size);

	/* AddToList allocates its three-pointer node after measuring text. */
	if (width_seen && size == 3 * sizeof(void *))
	{
		watch(result);
		nodes++;
	}
	width_seen = False;
	return result;
}

FlocaleFont *__wrap_FlocaleLoadFont(Display *dpy, char *name, char *module)
{
	FlocaleFont *result;

	config_watch = False;
	result = __real_FlocaleLoadFont(dpy, name, module);

	assert(result != NULL);
	fonts++;
	building = True;
	need_popup_gc = True;
	return result;
}

void __wrap_FlocaleUnloadFont(Display *dpy, FlocaleFont *font)
{
	font_frees++;
	__real_FlocaleUnloadFont(dpy, font);
}

GC __wrap_fvwmlib_XCreateGC(Display *dpy, Drawable drawable,
	unsigned long mask, XGCValues *values)
{
	GC result = __real_fvwmlib_XCreateGC(dpy, drawable, mask, values);

	if (need_popup_gc)
	{
		assert(gcs < 3);
		popup_gcs[gcs++] = result;
		need_popup_gc = False;
	}
	return result;
}

int __wrap_XFreeGC(Display *dpy, GC gc)
{
	for (unsigned int i = 0; i < gcs; i++)
		if (popup_gcs[i] == gc)
		{
			popup_gcs[i] = NULL;
			gc_frees++;
			break;
		}
	return __real_XFreeGC(dpy, gc);
}

Bool __wrap_fvwmlib_get_color(char *name, Pixel *pixel)
{
	Bool result = __real_fvwmlib_get_color(name, pixel);

	if (building && result)
	{
		assert(colors < 6);
		named_pixels[colors++] = *pixel;
	}
	return result;
}

Pixel __wrap_GetColor(char *name)
{
	Pixel result = __real_GetColor(name);

	/* Old objects use this API; known valid fixture colors always succeed. */
	if (building)
	{
		assert(colors < 6);
		named_pixels[colors++] = result;
	}
	return result;
}

void __wrap_PictureFreeColors(Display *dpy, Colormap map, Pixel *pixels,
	int count, unsigned long planes, Bool no_limit)
{
	for (int p = 0; p < count; p++)
		for (unsigned int i = 0; i < colors; i++)
			if (!named_pixel_released[i] && named_pixels[i] == pixels[p])
			{
				assert(no_limit == True);
				named_pixel_released[i] = True;
				color_frees++;
				break;
			}
	__real_PictureFreeColors(dpy, map, pixels, count, planes, no_limit);
}

int __wrap_XMapWindow(Display *dpy, Window window)
{
	int result = __real_XMapWindow(dpy, window);
	FILE *file = fopen(getenv("MODULE_OWNER_READY"), "w");

	assert(file != NULL);
	maps++;
	building = False;
	XSync(dpy, False);
	fprintf(file, "%u %lu\n", maps, window);
	assert(fclose(file) == 0);
	return result;
}

void __wrap_fvwmlib_get_target_window(Display *dpy, int screen,
	char *module, Window *window, Bool return_subwindow)
{
	(void)dpy; (void)screen; (void)module; (void)return_subwindow;
	*window = target;
	char path[4096];
	assert(snprintf(path, sizeof(path), "%s.retarget",
		getenv("MODULE_OWNER_READY")) < (int)sizeof(path));
	FILE *file = fopen(path, "w");
	assert(file != NULL && fclose(file) == 0);
}

static void report(void)
{
	FILE *file = fopen(getenv("MODULE_OWNER_REPORT"), "w");
	unsigned int expected_maps = strcmp(scenario, "early-eof") == 0 ? 0 : 3;
	unsigned int expected_strings = strcmp(scenario, "defaults") == 0 ? 2 :
		(strcmp(scenario, "fore-only") == 0 ||
		 strcmp(scenario, "back-only") == 0) ? 4 : 6;
	unsigned int expected_colors = strcmp(scenario, "colorset") == 0 ?
		0 : expected_maps * 2;

	assert(file != NULL);
	fprintf(file, "{\"active\":%u,\"strings\":%u,\"monitors\":%u,"
		"\"maps\":%u,\"fonts\":%u,\"font_frees\":%u,\"gcs\":%u,"
		"\"gc_frees\":%u,\"colors\":%u,\"color_frees\":%u,"
		"\"nodes\":%u}\n", active, strings, monitor_copies, maps, fonts,
		font_frees, gcs, gc_frees, colors, color_frees, nodes);
	assert(fclose(file) == 0);
	assert(maps == expected_maps && strings == expected_strings &&
		monitor_copies == 2 * expected_maps && fonts == expected_maps &&
		gcs == expected_maps && colors == expected_colors);
	assert(expected_maps ? nodes > 0 : nodes == 0);
	if (baseline)
		assert(active > 0 && font_frees == 0 && gc_frees == 0 &&
			color_frees == 0);
	else
		assert(active == 0 && fonts == font_frees && gcs == gc_frees &&
			colors == color_frees);
	/* Whole-module LSan is disabled: only the selected owners are asserted. */
}

int __wrap_main(int argc, char **argv)
{
	baseline = getenv("MODULE_OWNER_BASELINE") != NULL;
	scenario = getenv("MODULE_OWNER_SCENARIO");
	if (scenario == NULL)
		scenario = "named";
	assert(argc > 4);
	target = strtoul(argv[4], NULL, 16);
	assert(atexit(report) == 0);
	return __real_main(argc, argv);
}
