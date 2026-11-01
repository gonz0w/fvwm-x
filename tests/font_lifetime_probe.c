/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/flocale_font_types.h"
#include "libs/FftInterface.h"
#include "libs/fvwmlib.h"
#include <locale.h>
#include <stdio.h>
#include <string.h>

/* Non-installed diagnostic: Script's exact font init/close sequence without
 * selection ownership, widget mutation or a module connection. Finalization
 * is a distinct control, never evidence that Script already performs it. */
int main(int argc, char **argv)
{
#ifndef HAVE_XFT
	(void)argc;
	(void)argv;
	return 77;
#else
	Display *display;
	FlocaleFont *font;
	char name[] = "xft:DejaVu Sans:size=10";
	char module[] = "FvwmScript";
	char text[] = "owner-one";
	int width, height;
	Bool finalize = False;

	if (argc == 2 && !strcmp(argv[1], "--finalize"))
		finalize = True;
	else if (argc != 1)
		return 2;
	FlocaleInit(LC_CTYPE, "", "", module);
	display = XOpenDisplay(NULL);
	if (!display)
		return 77;
	flib_init_graphics(display);
	font = FlocaleLoadFont(display, name, module);
	if (!font || !font->fftf.fftfont[0])
	{
		if (font)
			FlocaleUnloadFont(display, font);
		XCloseDisplay(display);
		return 77;
	}
	width = FlocaleTextWidth(font, text, (int)strlen(text));
	height = font->height;
	FlocaleUnloadFont(display, font);
	XCloseDisplay(display);
	if (finalize)
		FftFini();
	if (printf("finalize=%d xft=1 width=%d height=%d\n",
		finalize, width, height) < 0 || fflush(stdout) != 0)
		return 1;
	return width > 0 && height > 0 ? 0 : 1;
#endif
}
