/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Flocale.h"
#include "libs/font_shadow.h"
#include "libs/font_text.h"
#include "libs/FlocaleCharset.h"
#include "libs/FftInterface.h"
#include "libs/PictureBase.h"

#include <assert.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int advance;
void __real_XftTextExtents32(Display *, XftFont *, const FcChar32 *, int,
	XGlyphInfo *);

void __wrap_XftTextExtents32(Display *display, XftFont *font,
	const FcChar32 *text, int count, XGlyphInfo *info)
{
	if (advance != 0)
	{
		assert(count == 1);
		memset(info, 0, sizeof(*info));
		info->xOff = (short)advance;
		return;
	}
	__real_XftTextExtents32(display, font, text, count, info);
}

static int fontset_advance;
int __real_XwcTextEscapement(XFontSet, const wchar_t *, int);
int __wrap_XwcTextEscapement(XFontSet fontset, const wchar_t *text, int count)
{
	assert(count > 0 && count <= 1024);
	if (fontset_advance != 0)
		return count * fontset_advance;
	return __real_XwcTextEscapement(fontset, text, count);
}

static void test_utf8_boundaries(void)
{
	FlocaleFont font = {0};
	const unsigned char text[] = "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
	uint32_t scalar;

	font.fc = FlocaleCharsetGetUtf8Charset();
	assert(font_text_utf8(text, 1, &scalar) == 1 && scalar == 'a');
	assert(font_text_utf8(text + 1, 2, &scalar) == 2 && scalar == 0xe9);
	assert(font_text_utf8(text + 3, 3, &scalar) == 3 && scalar == 0x20ac);
	assert(font_text_utf8(text + 6, 4, &scalar) == 4 && scalar == 0x1f600);
	assert(font_text_utf8(text + 6, 3, &scalar) == 1 && scalar == 0xfffd);
	assert(font_text_utf8((const unsigned char *)"\xed\xa0\x80", 3,
	    &scalar) == 1 && scalar == 0xfffd);
	assert(font_text_utf8((const unsigned char *)"\xc0\x80", 2,
	    &scalar) == 1 && scalar == 0xfffd);
	assert(font_text_utf8(text, 0, &scalar) == 0);
	assert(FlocaleStringCharLength(&font, (const char *)text) == 4);
	assert(FlocaleStringCharToByteOffset(&font, (const char *)text, 4) == 10);
	assert(FlocaleStringCharToByteOffset(&font, (const char *)text, 100) == 10);
	assert(FlocaleStringByteToCharOffset(&font, (const char *)text, 10) == 4);
	assert(FlocaleStringCharLength(&font, "\xe2\x82") == 2);
	assert(FlocaleStringCharToByteOffset(&font, "\xe2\x82", 100) == 2);
}

static void test_fontset_width(Display *display, char *text)
{
	FlocaleFont *font = FlocaleLoadFont(display, "fixed", "fontset-test");
	char **missing = NULL, *fallback;
	int missing_count;
	XFontSet original, set;

	assert(font);
	set = XCreateFontSet(display, "fixed", &missing, &missing_count, &fallback);
	assert(set);
	if (missing != NULL)
		XFreeStringList(missing);
	original = font->fontset;
	font->fontset = set;
	fontset_advance = 32767;
	assert(FlocaleTextWidth(font, text, 65539) == INT_MAX);
	fontset_advance = -32768;
	assert(FlocaleTextWidth(font, text, 65539) == INT_MIN);
	fontset_advance = 0;
	/* Long valid strings must agree with independently measured short runs. */
	assert(FlocaleTextWidth(font, text, 65539) ==
	    65539 * FlocaleTextWidth(font, "a", 1));
	font->fontset = original;
	XFreeFontSet(display, set);
	FlocaleUnloadFont(display, font);
}

static void test_shadow_positions(Display *display)
{
	char description[128];
	uint32_t count;
	int maximum = 0;
	for (int size = 1; size <= 128; size++)
	{
		if (font_shadow_count(size, 0, 0, true,
		    FVWM_FONT_SHADOW_LIMIT, &count))
		{
			maximum = size;
		}
	}
	if (maximum > 0)
	{
		FlocaleFont *font;
		FlocaleFont *cached;
		snprintf(description, sizeof(description), "Shadow=%d 0 C:fixed",
		    maximum);
		font = FlocaleLoadFont(display, description, "font-shadow-test");
		assert(font);
		cached = FlocaleLoadFont(display, description, "font-shadow-test");
		assert(cached == font);
		FlocaleUnloadFont(display, cached);
		FlocaleUnloadFont(display, font);
	}
	snprintf(description, sizeof(description), "Shadow=%d 0 C:fixed",
	    maximum + 1);
	assert(FlocaleLoadFont(display, description, "font-shadow-test") == NULL);
	assert(FlocaleLoadFont(display, "Shadow=32768 0 C:fixed",
	    "font-shadow-test") == NULL);
	assert(FlocaleLoadFont(display, "Shadow=1 2147483646 C:fixed",
	    "font-shadow-test") == NULL);

	/* Compare production iteration with the old row-major border and
	 * directional ordering, including rotations and overlapping directions. */
	for (unsigned mask = 1; mask <= (MULTI_DIR_ALL | MULTI_DIR_C); mask++)
	{
		unsigned directions = 0;
		for (unsigned bit = MULTI_DIR_FIRST; bit < MULTI_DIR_C; bit <<= 1)
		{
			directions += (mask & bit) != 0;
		}
		if (!font_shadow_count(2, 1, directions, mask & MULTI_DIR_C,
		    FVWM_FONT_SHADOW_LIMIT, &count))
		{
			continue;
		}
		for (rotation_t rotation = ROTATION_0; rotation <= ROTATION_270;
		     rotation++)
		{
			FlocaleFont font = {0};
			FlocaleWinString string = {0};
			flocale_gstp_args args;
			multi_direction_t direction = MULTI_DIR_NONE;
			uint32_t observed = 0;
			int x = 0, y = 0;
			font.shadow_size = 2;
			font.shadow_offset = 1;
			font.flags.shadow_dir = mask;
			string.flags.text_rotation = rotation;
			FlocaleInitGstpArgs(&args, &font, &string, 100, 100);
			while (gravity_get_next_multi_dir(mask, &direction),
			    direction != MULTI_DIR_NONE)
			{
				if (direction == MULTI_DIR_C)
				{
					for (int ty = -3; ty <= 3; ty++)
					{
						for (int tx = -3; tx <= 3; tx++)
						{
							if (tx > -2 && tx < 2 &&
							    ty > -2 && ty < 2)
							{
								continue;
							}
							assert(FlocaleGetShadowTextPosition(
							    &x, &y, &args));
							assert(x == args.orig_x + tx &&
							    y == args.orig_y + ty);
							observed++;
						}
					}
				}
				else
				{
					int dx, dy;
					direction_t dir_x, dir_y;
					gravity_split_xy_dir(&dir_x, &dir_y,
					    gravity_multi_dir_to_dir(direction));
					dx = gravity_dir_to_sign_one_axis(dir_x);
					dy = gravity_dir_to_sign_one_axis(dir_y);
					gravity_rotate_xy(rotation, dx, dy, &dx, &dy);
					for (int step = 2; step <= 3; step++)
					{
						assert(FlocaleGetShadowTextPosition(&x, &y,
						    &args));
						assert(x == args.orig_x + dx * step &&
						    y == args.orig_y + dy * step);
						observed++;
					}
				}
			}
			assert(observed == count);
			assert(!FlocaleGetShadowTextPosition(&x, &y, &args));
			assert(x == args.orig_x && y == args.orig_y);
		}
	}
	{
		FlocaleFont font = {0};
		FlocaleWinString string = {0};
		flocale_gstp_args args;
		int x = 0, y = 0;
		font.shadow_size = 1;
		font.flags.shadow_dir = MULTI_DIR_N;
		FlocaleInitGstpArgs(&args, &font, &string, INT_MAX, INT_MIN);
		assert(FlocaleGetShadowTextPosition(&x, &y, &args));
		assert(x == INT_MAX && y == INT_MIN);
		assert(!FlocaleGetShadowTextPosition(&x, &y, &args));
	}
}

int main(void)
{
	Display *display = XOpenDisplay(NULL);
	FlocaleFont *font;
	char *text = malloc(65540);
	char ordinary[] = "xft:Sans-12";
	char overflow[] = "Shadow=2147483647:fixed";
	char sum_overflow[] = "Shadow=2147483647 2147483647:fixed";
	char quote[] = "'";
	char empty[] = "";

	assert(display && text);
	PictureInitCMap(display);
	FlocaleInit(LC_CTYPE, "", NULL, "font-geometry-test");
	test_shadow_positions(display);
	test_utf8_boundaries();
	font = FlocaleLoadFont(display, ordinary, "font-geometry-test");
	assert(font && font->fftf.fftfont[0] != NULL);
	memset(text, 'a', 65539);
	text[65539] = '\0';
	test_fontset_width(display, text);
	advance = 32767;
	assert(FftTextWidth(display, font, text, 1) == 32767);
	assert(FftTextWidth(display, font, text, 65539) == INT_MAX);
	advance = -32768;
	assert(FftTextWidth(display, font, text, 65539) == INT_MIN);
	advance = 0;
#ifdef HAVE_ICONV
	{
		FlocaleFont *latin = FlocaleLoadFont(display,
		    "StringEncoding=ISO8859-1:xft:Sans-12", "encoding-test");
		assert(latin);
		assert(FlocaleTextWidth(latin, "x\xe9", 2) ==
		    FlocaleTextWidth(font, "x\xc3\xa9", 3));
		assert(FlocaleTextWidth(latin, "\xe9", 1) ==
		    FlocaleTextWidth(font, "\xc3\xa9", 2));
		FlocaleUnloadFont(display, latin);
	}
#endif
	assert(FlocaleTextWidth(font, "a", INT_MIN) == 0);
	FlocaleUnloadFont(display, font);
	assert(FlocaleLoadFont(display, overflow, "font-geometry-test") == NULL);
	assert(FlocaleLoadFont(display, sum_overflow, "font-geometry-test") == NULL);
	font = FlocaleLoadFont(display, "fixed", "font-geometry-test");
	assert(font && FlocaleTextWidth(font, "Geometry", 8) > 0);
	{
		XFontStruct synthetic = {0};
		XFontStruct *original = font->font;
		XFontSet original_set = font->fontset;
		synthetic.max_char_or_byte2 = 255;
		synthetic.all_chars_exist = True;
		synthetic.min_bounds.width = 32767;
		synthetic.max_bounds.width = 32767;
		font->font = &synthetic;
		font->fontset = None;
		assert(FlocaleTextWidth(font, text, 65539) == INT_MAX);
		synthetic.min_bounds.width = -32768;
		synthetic.max_bounds.width = -32768;
		assert(FlocaleTextWidth(font, text, 65539) == INT_MIN);
		font->font = original;
		font->fontset = original_set;
	}
	{
		FlocaleCharset big5 = *font->fc;
		FlocaleCharset *original = font->fc;
		unsigned original_mb = font->flags.is_mb;
		big5.x = "big5-0";
		big5.encoding_type = FLC_ENCODING_TYPE_FONT;
		font->fc = &big5;
		font->flags.is_mb = True;
		/* One byte bypasses iconv: the core Big5 fallback must advance
		 * past its ASCII placeholder instead of filling without bound. */
		assert(FlocaleTextWidth(font, "a", 1) >= 0);
		font->fc = original;
		font->flags.is_mb = original_mb;
	}
	{
		FlocaleCharset *original = font->fc;
		FlocaleCharset *original_string = font->str_fc;
		XFontSet original_set = font->fontset;
		Pixmap pixmap = XCreatePixmap(display, DefaultRootWindow(display),
		    64, 32, DefaultDepth(display, DefaultScreen(display)));
		GC gc = XCreateGC(display, pixmap, 0, NULL);
		FlocaleWinString string = {0};
		font->fc = font->str_fc = FlocaleCharsetGetUtf8Charset();
		font->fontset = None;
		string.win = pixmap;
		string.gc = gc;
		string.x = 2;
		string.y = 14;
		string.str = "a\xcc\x81\xf0\x9f\x98\x80";
		FlocaleDrawString(display, font, &string, 0);
		XSync(display, False);
		font->fc = original;
		font->str_fc = original_string;
		font->fontset = original_set;
		XFreeGC(display, gc);
		XFreePixmap(display, pixmap);
	}
	FlocaleUnloadFont(display, font);
	font = FlocaleLoadFont(display, empty, "font-geometry-test");
	assert(font);
	FlocaleUnloadFont(display, font);
	font = FlocaleLoadFont(display, quote, "font-geometry-test");
	assert(font);
	FlocaleUnloadFont(display, font);
	free(text);
	XCloseDisplay(display);
	FcFini(); /* Release Fontconfig's process cache for leak checking. */
	return 0;
}
