/* -*-c-*- */

#ifndef FVWMLIB_FFT_INTERFACE_H
#define FVWMLIB_FFT_INTERFACE_H

#include "fvwm_x11.h"
#include "libs/monitor_scale.h"

#ifdef HAVE_XFT
#define FftSupport 1
#else
#define FftSupport 0
#endif

typedef struct _FlocaleFont FlocaleFont;
typedef struct _FlocaleWinString FlocaleWinString;

/*
 * Fvwm Xft font structure
 */

typedef struct
{
	XftFont *fftfont[4]; /* Four rotations */
	char *encoding;
} FftFontType;

void FftGetFontHeights(
	FftFontType *fftf, int *height, int *ascent, int *descent);

FftFontType *FftGetFont(Display *dpy, char *fontname, char *module);
FftFontType *FftGetFontAtScale(
	Display *dpy, char *fontname, char *module,
	monitor_scale_factor factor);
void FftCloseFont(Display *dpy, FftFontType *fftf);
/* Process-final only: unload all fonts and close all displays first. Releases
 * fallback patterns and Fontconfig state; no subsequent font use is allowed. */
void FftFini(void);

void FftDrawString(
	Display *dpy, FlocaleFont *flf, FlocaleWinString *fws,
	Pixel fg, Pixel fgsh, Bool has_fg_pixels, int len,
	unsigned long flags);

int FftTextWidth(Display *dpy, FlocaleFont *flf, char *str, int len);
void FftPrintPatternInfo(FftFontType *ft);

#endif
