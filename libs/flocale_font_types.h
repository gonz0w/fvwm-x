/* Complete font record for font and drawing owners. */
#ifndef FVWMLIB_FLOCALE_FONT_TYPES_H
#define FVWMLIB_FLOCALE_FONT_TYPES_H

#include "Flocale.h"

struct _FlocaleFont
{
	struct _FlocaleFont *next;
	char *name;
	int count;
	unsigned int scale_quarters;
	unsigned int generation;
	XFontStruct *font;      /* font structure */
	XFontSet fontset;       /* font set */
	FftFontType fftf;       /* fvwm xft font */
	FlocaleCharset *fc;     /* fvwm charset of the font */
	FlocaleCharset *str_fc; /* fvwm charset of the strings to be displayed */
	int height;             /* height of the font: ascent + descent */
	int ascent;
	int descent;
	int shadow_size;
	int shadow_offset;
	struct
	{
		unsigned shadow_dir : (DIR_ALL_MASK + 1);
		unsigned must_free_fc : 1;
		/* is_mb are used only with a XFontStruct font, for XFontSet
		 * everything is done in the good way automatically and this
		 * parameters is not needed */
		unsigned is_mb  : 1; /* if true the font is a 2 bytes font */
	} flags;
};

#endif
