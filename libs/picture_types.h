/* Complete picture record for cache, rendering and native module owners. */
#ifndef FVWMLIB_PICTURE_TYPES_H
#define FVWMLIB_PICTURE_TYPES_H

#include "PictureBase.h"

struct FvwmPictureThing
{
	struct FvwmPictureThing *next;
	char *name;
	unsigned long stamp;  /* should be FileStamp */
	unsigned long fpa_mask;
	Pixmap picture;
	Pixmap mask;
	Pixmap alpha;
	Cursor prepared_cursor;
	/* Exact background allocation retained by a derived effect picture. */
	struct picture_color *effect_background;
	uint16_t average_rgb[3];
	Bool average_valid;
	Bool average_has_pixels;
	unsigned int depth;
	unsigned int width;
	unsigned int height;
	unsigned int count;
	Pixel *alloc_pixels;
	int nalloc_pixels;
	Bool no_limit;
	Colormap color_cmap;
	unsigned int scale_quarters;
	unsigned int target_width;
	unsigned int target_height;
	unsigned int generation;
	/* Core deferred views borrow immutable backing resources. Ordinary
	 * pictures and module packet snapshots leave these fields zero. */
	struct FvwmPictureThing *backing;
	void (*view_release)(Display *, struct FvwmPictureThing *, void *);
	void *view_context;
	/* Optional owner binding is independent of the scheduler subscription.
	 * Its finalizer runs after removing interest, before backing release. */
	void *view_owner;
	void (*view_owner_release)(void *);
	Bool is_view;
	Bool pending;
};

#endif
