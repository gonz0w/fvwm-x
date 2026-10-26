/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */

#include "config.h"
#include "libs/ColorUtils.h"
#include "libs/Colorset.h"
#include "libs/PictureUtils.h"
#include "libs/PictureBase.h"
#include "libs/fvwmlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned int parses, allocations, queries;
static int refuse_parse, refuse_alloc, refuse_query;
static Pixel expected_pixel = 17;
static Bool colorset_source, live;
Status __real_XParseColor(Display *, Colormap, const char *, XColor *);
Status __real_XQueryColor(Display *, Colormap, XColor *);
int __real_PictureAllocColor(Display *, Colormap, XColor *, int);

Status __wrap_XParseColor(Display *display, Colormap map, const char *name,
	XColor *color)
{
	if (live)
		return __real_XParseColor(display, map, name, color);
	assert(name != NULL);
	parses++;
	color->pixel = 17;
	color->red = 101;
	color->green = 202;
	color->blue = 303;
	return !refuse_parse;
}

Status __wrap_XQueryColor(Display *display, Colormap map, XColor *color)
{
	if (live)
		return __real_XQueryColor(display, map, color);
	assert(colorset_source && color->pixel == expected_pixel);
	queries++;
	color->red = 401;
	color->green = 502;
	color->blue = 603;
	return !refuse_query;
}

int __wrap_PictureAllocColor(Display *display, Colormap map, XColor *color,
	int no_limit)
{
	if (live)
		return __real_PictureAllocColor(display, map, color, no_limit);
	assert(no_limit && color->pixel == expected_pixel);
	assert(color->red == (colorset_source ? 401 : 101));
	assert(color->green == (colorset_source ? 502 : 202));
	assert(color->blue == (colorset_source ? 603 : 303));
	allocations++;
	/* A successful allocated black reference is different from refusal. */
	color->pixel = refuse_alloc ? 23 : 0;
	return !refuse_alloc;
}

static int live_colors(void)
{
	Display *display = XOpenDisplay(NULL);
	char *names[] = {"#123456", "#abcdef", "#6a1b9a", "#d08020"};
	char *references[] = {"$[fg.cs1]", "$[bg.cs1]", "$[hilight.cs1]",
		"$[shadow.cs1]"};
	Pixel sources[4];

	assert(display != NULL);
	live = True;
	flib_init_graphics(display);
	assert(AllocColorset(1));
	Pixel *fields[] = {&GetColorset(1)->fg, &GetColorset(1)->bg,
		&GetColorset(1)->hilite, &GetColorset(1)->shadow};

	for (unsigned int i = 0; i < 4; i++)
	{
		assert(fvwmlib_get_color(names[i], &sources[i]));
		*fields[i] = sources[i];
	}
	for (unsigned int i = 0; i < 4; i++)
	{
		Pixel clone;
		XColor source = {.pixel = sources[i]};

		assert(XQueryColor(display, Pcmap, &source));
		assert(source.red != 0 || source.green != 0 || source.blue != 0);
		assert(fvwmlib_get_color(references[i], &clone));
		Pixel legacy = GetColor(references[i]);
		Pixel copies[] = {clone, legacy};

		for (unsigned int j = 0; j < 2; j++)
		{
			XColor copy = {.pixel = copies[j]};

			assert(XQueryColor(display, Pcmap, &copy));
			assert(copy.red == source.red && copy.green == source.green &&
				copy.blue == source.blue);
		}
		assert(*fields[i] == sources[i]);
		PictureFreeColors(display, Pcmap, copies, 2, 0, True);
	}
	PictureFreeColors(display, Pcmap, sources, 4, 0, True);
	XCloseDisplay(display);
	puts("actual X colors: four borrowed colorset RGB values clone correctly "
		"through status and legacy interfaces");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--live") == 0)
		return live_colors();
	assert(argc == 1);
	Pixel pixel = 99;

	assert(fvwmlib_get_color("black", &pixel) && pixel == 0);
	assert(GetColor("black") == 0 && GetSimpleColor("black") == 0);
	assert(parses == 3 && allocations == 3);
	refuse_parse = 1;
	assert(!fvwmlib_get_color("unknown", &pixel) && pixel == 17);
	assert(GetColor("unknown") == 17 && GetSimpleColor("unknown") == 17);
	assert(allocations == 3);
	refuse_parse = 0;
	refuse_alloc = 1;
	assert(!fvwmlib_get_color("black", &pixel) && pixel == 23);
	assert(GetColor("black") == 23 && GetSimpleColor("black") == 23);
	unsigned int before = parses;

	assert(!fvwmlib_get_color("rgb:bad", &pixel) && pixel == 0);
	assert(GetColor("rgb:bad") == 0 && GetSimpleColor("rgb:bad") == 0);
	assert(parses == before);
	refuse_alloc = 0;
	assert(AllocColorset(1));
	GetColorset(1)->fg = 91;
	GetColorset(1)->bg = 92;
	GetColorset(1)->hilite = 93;
	GetColorset(1)->shadow = 94;
	char *names[] = {"$[fg.cs1]", "$[bg.cs1]", "$[hilight.cs1]",
		"$[shadow.cs1]"};

	colorset_source = True;
	for (unsigned int i = 0; i < 4; i++)
	{
		expected_pixel = 91 + i;
		assert(fvwmlib_get_color(names[i], &pixel) && pixel == 0);
		assert(GetColor(names[i]) == 0);
		refuse_alloc = 1;
		assert(!fvwmlib_get_color(names[i], &pixel) && pixel == 0);
		assert(GetColor(names[i]) == 0);
		refuse_alloc = 0;
		refuse_query = 1;
		unsigned int prior_allocations = allocations;

		assert(!fvwmlib_get_color(names[i], &pixel) && pixel == 0);
		assert(GetColor(names[i]) == 0);
		assert(allocations == prior_allocations);
		refuse_query = 0;
	}
	assert(queries == 24 && parses == before);
	assert(GetColorset(1)->fg == 91 && GetColorset(1)->bg == 92 &&
		GetColorset(1)->hilite == 93 && GetColorset(1)->shadow == 94);
	char *invalid[] = {"$[fg.cs01]", "$[fg.cs-1]", "$[fg.cs1",
		"$[fg.cs1]tail", "$[fg.cs2147483648]"};

	before = allocations;
	for (unsigned int i = 0; i < 5; i++)
	{
		pixel = 99;
		assert(!fvwmlib_get_color(invalid[i], &pixel) && pixel == 0);
		assert(GetColor(invalid[i]) == 0);
	}
	assert(allocations == before);
	puts("shared color status: allocated pixel zero, parse/allocation refusal, "
		"colorset names and legacy Pixel results passed");
	return 0;
}
