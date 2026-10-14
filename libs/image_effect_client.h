#ifndef FVWMLIB_IMAGE_EFFECT_CLIENT_H
#define FVWMLIB_IMAGE_EFFECT_CLIENT_H

#include "image_effect_protocol.h"
#include "image_process.h"

/* Caller owns the fixed request input and result through actual process reap.
 * Pixmap/palette leases are separate owned scheduler state, never temporary
 * argv or a resource pointer held by an asynchronous signal callback. */
image_process *image_effect_start(const char *display,
	const image_effect_request *request, image_input *input,
	image_effect_result *result);

#endif
