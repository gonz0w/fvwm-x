#ifndef FVWMLIB_IMAGE_EFFECT_H
#define FVWMLIB_IMAGE_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

enum
{
	IMAGE_EFFECT_BACKGROUND,
	IMAGE_EFFECT_RGB
};

typedef struct
{
	uint16_t red;
	uint16_t green;
	uint16_t blue;
	uint16_t kind;
} image_effect_pixel;

/* Legacy software colorset arithmetic on queried server RGB16, before output
 * palette allocation. Masked/zero-alpha pixels reuse the exact background
 * pixel, rather than allocating its RGB again. Failure clears the result. */
bool image_effect_blend(const uint16_t source[3], const uint16_t tint[3],
	const uint16_t background[3], unsigned alpha, bool masked,
	unsigned tint_percent, unsigned alpha_percent, image_effect_pixel *result);

#endif
