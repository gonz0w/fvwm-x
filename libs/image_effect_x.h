#ifndef FVWMLIB_IMAGE_EFFECT_X_H
#define FVWMLIB_IMAGE_EFFECT_X_H

#include "image_effect_protocol.h"

typedef bool (*image_effect_write_fn)(const void *bytes, size_t length);

/* Worker only: a separate X connection reads pinned, acknowledged resources.
 * Uses fixed-size tiles; creates no pixmap/color or authoritative X state. */
bool image_effect_render(const char *display,
	const image_effect_request *request, image_effect_write_fn write_result);

#endif
