#ifndef FVWMLIB_PICTURE_ARGB_H
#define FVWMLIB_PICTURE_ARGB_H

#include "PictureBase.h"
#include "image_effect.h"
#include <stddef.h>
#include <stdint.h>

typedef struct picture_argb_builder picture_argb_builder;

#define PICTURE_ARGB_STEP_PIXELS 16384U

/* Borrow pixels through completion. A step converts/uploads at most this
 * many pixels in source row order, and releases at most this many duplicate
 * color references. The caller keeps the picture visual active during service.
 * This uses small ordinary XImages, without shared-memory setup barriers. */
picture_argb_builder *picture_argb_start(Display *dpy, Window drawable,
	const uint32_t *pixels, size_t length, unsigned width, unsigned height,
	FvwmPictureAttributes attributes);
/* Cursor preparation: bounded direct ARGB32 upload, with RGB premultiplied
 * by alpha. The caller chooses this only with a server ARGB32 format. */
picture_argb_builder *picture_argb_start_argb32(Display *dpy,
	Window drawable, const uint32_t *pixels, size_t length,
	unsigned width, unsigned height);
/* Software-effect RGB16 records use the legacy non-dithered output palette.
 * The caller pins the exact background pixel through the output's lifetime. */
picture_argb_builder *picture_argb_start_effects(Display *dpy, Window drawable,
	const image_effect_pixel *pixels, size_t length, unsigned width,
	unsigned height, Pixel background);
Bool picture_argb_step(picture_argb_builder *builder);
void picture_argb_cancel(picture_argb_builder *builder);
/* Called after owned creation errors are received. Mark uncreated pixmaps
 * invalid before cleanup; failed GC handles still need client-side release. */
void picture_argb_creation_error(picture_argb_builder *builder,
	const XErrorEvent *error);
/* Transfers a count-one immutable picture after successful completion. */
FvwmPicture *picture_argb_take(picture_argb_builder *builder);
/* Refuses unfinished work, including pending cancellation cleanup. */
Bool picture_argb_destroy(picture_argb_builder *builder);

#endif
