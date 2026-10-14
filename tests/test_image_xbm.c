/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_xbm.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
compare_xlib(const char *path, const char *text)
{
	FILE *file = fopen(path, "wb");

	assert(file != NULL);
	assert(fwrite(text, 1, strlen(text), file) == strlen(text));
	assert(fclose(file) == 0);
	unsigned char *baseline = NULL, *checked = NULL;
	unsigned bw = 0, bh = 0, w = 0, h = 0;
	int bx = -1, by = -1, x = -1, y = -1;
	image_input source = {(unsigned char *)text, strlen(text)};

	assert(XReadBitmapFileData(path, &bw, &bh, &baseline, &bx, &by)
	    == BitmapSuccess);
	assert(image_xbm_read(&source, INT_MAX, SIZE_MAX,
	    &checked, &w, &h, &x, &y));
	assert(w == bw && h == bh && x == bx && y == by);
	assert(memcmp(baseline, checked, ((size_t)w + 7) / 8 * h) == 0);
	XFree(baseline);
	free(checked);
}

static void
refuse(const char *text, unsigned max_dimension, size_t max_pixels)
{
	image_input source = {(unsigned char *)text, strlen(text)};
	unsigned char *bits = (unsigned char *)1;
	unsigned width = 123, height = 123;
	int hx = 123, hy = 123;

	assert(!image_xbm_read(&source, max_dimension, max_pixels,
	    &bits, &width, &height, &hx, &hy));
	assert(bits == NULL && width == 0 && height == 0);
	assert(hx == -1 && hy == -1);
}

static void
test_formats_and_padding(const char *path)
{
	const char *types[] = {"unsigned char", "char", "short"};

	for (unsigned type = 0; type < 3; type++)
	{
		for (unsigned width = 1; width <= 33; width++)
		{
			char text[4096];
			size_t used = (size_t)snprintf(text, sizeof(text),
			    "/* bitmap */\n#define unused abc\n"
			    "#define icon_width %u\n#define icon_height 3\n"
			    "#define icon_x_hot 3\n#define icon_y_hot 2\n"
			    "static %s icon_bits[] = {\n", width, types[type]);
			unsigned columns = type == 2 ? (width + 15) / 16 :
			    (width + 7) / 8;

			for (unsigned i = 0; i < columns * 3; i++)
			{
				used += (size_t)snprintf(text + used,
				    sizeof(text) - used, "0x%x, ",
				    type == 2 ? (i + 1) * 258 : i + 1);
			}
			assert(used + 4 < sizeof(text));
			memcpy(text + used, "\n};\n", 5);
			compare_xlib(path, text);
		}
	}
	compare_xlib(path, "#define width 8\r\n#define height 1\r\n"
	    "static char bits[] = {\r\n0x55 };\r\n");
	compare_xlib(path, "#define bitmap_width 8\n#define bitmap_height 1\n"
	    "static char bitmap_bits[] = {\n0xff };\n"
	    "static char bitmap_bits[] = {\n0x00 };\n");
	/* Preserve Xlib's wider file geometry. The server's pixmap limit is
	 * independent and can be lower than its file parser's accepted width. */
	char *wide = malloc(31000);
	const char *header = "#define wide_width 40000\n"
	    "#define wide_height 1\nstatic char wide_bits[] = {\n";
	size_t used = strlen(header);

	assert(wide);
	memcpy(wide, header, used);
	for (int index = 0; index < 5000; index++)
	{
		memcpy(wide + used, "0xff,\n", 6);
		used += 6;
	}
	memcpy(wide + used, "};\n", 4);
	compare_xlib(path, wide);
	free(wide);
}

static void
test_refusal_outputs(void)
{
	const char *invalid[] = {
	    "", "nothing", "#define width -1\n#define height 1\n"
	    "static char bits[] = {\n0xff };\n",
	    "#define width 0\n#define height 1\nstatic char bits[] = {\n0 };\n",
	    "#define width 9999999999999999999999999999999999999\n",
	    "#define width 8\n#define height 2\nstatic char bits[] = {\n0 };\n",
	    "#define width 8\n#define height 1\nstatic char bits[] = {\n"
	    "0xffffffffffffffffffffffffffffffffffffffff };\n",
	    "#define width 32769\n#define height 1\n"
	    "static char bits[] = {\n0 };\n",
	    "#define width 32768\n#define height 32768\n"
	    "static char bits[] = {\n0 };\n"};

	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		refuse(invalid[i], 32768, 64U * 1024U * 1024U);
	}
	const char *valid = "#define width 8\n#define height 2\n"
	    "static char bits[] = {\n0, 1 };\n";

	refuse(valid, 7, 64);
	refuse(valid, 8, 15);
	char long_line[257];

	memset(long_line, ' ', sizeof(long_line));
	long_line[255] = '\n';
	long_line[256] = '\0';
	refuse(long_line, 32768, 64U * 1024U * 1024U);
}

int
main(void)
{
	char path[] = "/tmp/fvwm-image-xbm-XXXXXX";
	int fd = mkstemp(path);

	assert(fd >= 0 && close(fd) == 0);
	test_formats_and_padding(path);
	test_refusal_outputs();
	assert(unlink(path) == 0);
	puts("X10/X11 bitmap buffer parsing matches Xlib and bounds allocation");
	return 0;
}
