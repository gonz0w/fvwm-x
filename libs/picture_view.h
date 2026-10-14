#ifndef FVWMLIB_PICTURE_VIEW_H
#define FVWMLIB_PICTURE_VIEW_H

#include "Picture.h"

typedef void (*picture_view_release_fn)(Display *dpy, FvwmPicture *view,
	void *context);

/* One owner-specific lease, with absent-image fields until backed. Takes name
 * ownership. Its release callback removes scheduler interest after the last
 * reference; the view is still readable during that callback. */
FvwmPicture *picture_view_create(char *name, FvwmPictureAttributes attributes,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height,
	unsigned generation, picture_view_release_fn release, void *context);
/* Retain an immutable usable backing, unwrapping another view. A pending view
 * must never borrow fields from another mutable view. NULL restores absence.
 * Caller commits replacements only at the owner's safe layout boundary. */
void picture_view_set_backing(Display *dpy, FvwmPicture *view,
	FvwmPicture *picture);
Bool picture_is_usable(const FvwmPicture *picture);
/* Retain only the currently usable immutable backing. A mapped menu/temporary
 * drawing snapshot must not share the mutable lease that may later commit. */
FvwmPicture *picture_snapshot(FvwmPicture *picture);

#endif
