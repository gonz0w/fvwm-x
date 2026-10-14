/* -*-c-*- */
/* Copyright (C) 2002  Olivier Chapuis */

#ifndef FVWMLIB_PICTURE_IMAGE_LOADER_H
#define FVWMLIB_PICTURE_IMAGE_LOADER_H

/* ---------------------------- included header files ---------------------- */

#include <stddef.h>

#include "fvwm_x11.h"
#include "PictureBase.h"
#include "libs/monitor_scale.h"
#include "image_input.h"

/* ---------------------------- global macros ------------------------------ */

/* ---------------------------- type definitions --------------------------- */

/* ---------------------------- interface functions ------------------------ */

/*
 * <pubfunc>PImageCreatePixmapFromArgbData
 * <description>
 * Create a pixmap with its mask and alpha channel from ARGB data.
 * </description>
 */
Bool PImageCreatePixmapFromArgbData(
	Display *dpy, Window win, CARD32 *data, size_t data_len, int start,
	int width, int height, Pixmap *pixmap, Pixmap *mask, Pixmap *alpha,
	int *nalloc_pixels, Pixel **alloc_pixels, int *no_limit,
	FvwmPictureAttributes fpa);
/*
 * <pubfunc>PImageLoadPixmapFromFile
 * <description>
 * Create a pixmap with its mask and alpha channel from a file.
 * </description>
 */
Bool PImageLoadPixmapFromFile(
	Display *dpy, Window win, char *file, Pixmap *pixmap, Pixmap *mask,
	Pixmap *alpha, int *width, int *height, int *depth,
	int *nalloc_pixels, Pixel **alloc_pixels, int *no_limit,
	FvwmPictureAttributes fpa);

/*
 * <pubfunc>PImageLoadPixmapFromFile
 * <description>
 * Create a FvwmPicture from a file.
 * </description>
 */
FvwmPicture *PImageLoadFvwmPictureFromFile(
	Display *dpy, Window win, char *path, FvwmPictureAttributes fpa,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height);

/* Same file/ownership contract, with the SVG attempt reported for deferred
 * service instead of waiting. Raster and bitmap success takes precedence. */
FvwmPicture *picture_load_non_svg(Display *dpy, Window win, char *path,
	FvwmPictureAttributes fpa, monitor_scale_factor factor,
	unsigned target_width, unsigned target_height, Bool *svg_deferred);

/* Preserve non-SVG format fallbacks for a deferred request using its already
 * opened snapshot. Borrows input/metadata; takes path only on success. SVG is
 * never retried here. Other formats keep their existing synchronous decoder. */
FvwmPicture *picture_load_non_svg_input(Display *dpy, Window win, char *path,
	const image_input *input, const struct stat *metadata,
	const image_input *xpm_input, FvwmPictureAttributes fpa,
	monitor_scale_factor factor,
	unsigned target_width, unsigned target_height);

/* XPM-only decode before an SVG attempt, preserving the existing order.
 * Borrows both inputs; a NULL xpm_input records refused expansion. */
Bool picture_decode_xpm_input(Display *dpy, char *path,
	const image_input *input, const image_input *xpm_input,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height, CARD32 **pixels, int *width, int *height);

/* Same decode, with source XPM hotspot metadata for cursor preparation. */
Bool picture_decode_xpm_cursor_input(Display *dpy, char *path,
	const image_input *input, const image_input *xpm_input,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height, CARD32 **pixels, int *width, int *height,
	int hotspot[2]);

/*
 * <pubfunc>PImageLoadPixmapFromFile
 * <description>
 * Create a cursor from a file.
 * </description>
 */
Cursor PImageLoadCursorFromFile(
	Display *dpy, Window win, char *path, int x_hot, int y_hot,
	monitor_scale_factor factor);

/*
 * <pubfunc>PImageLoadPixmapFromFile
 * <description>
 * Create a pixmap with its mask from xpm data.
 * </description>
 */
Bool PImageLoadPixmapFromXpmData(
	Display *dpy, Window win, int color_limit, char **data,
	Pixmap *pixmap, Pixmap *mask, int *width,
	int *height, int *depth);

#endif /* FVWMLIB_PICTURE_IMAGE_LOADER_H  */
