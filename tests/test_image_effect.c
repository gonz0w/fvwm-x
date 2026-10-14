/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_effect.h"

#include <assert.h>
#include <stddef.h>

static void test_legacy_rounding(void)
{
	uint16_t red[] = {65535, 0, 0};
	uint16_t green[] = {0, 65535, 0};
	uint16_t blue[] = {0, 0, 65535};
	image_effect_pixel result;

	assert(image_effect_blend(red, green, blue, 255, false, 50, 100,
	    &result));
	assert(result.kind == IMAGE_EFFECT_RGB && result.red == 32767 &&
	    result.green == 32767 && result.blue == 0);
	assert(image_effect_blend(red, green, blue, 255, false, 50, 50,
	    &result));
	assert(result.red == 16319 && result.green == 16319 &&
	    result.blue == 32896);
	/* The software path first reduces added alpha, then blends RGB16.
	 * Do not change its two integer divisions into a floating point weight. */
	for (unsigned alpha = 0; alpha <= 255; alpha++)
	{
		for (unsigned percent = 0; percent <= 100; percent++)
		{
			unsigned effective = alpha * percent / 100;

			assert(image_effect_blend(red, green, blue, alpha, false,
			    50, percent, &result));
			assert(result.kind == (effective ? IMAGE_EFFECT_RGB :
			    IMAGE_EFFECT_BACKGROUND));
			if (effective != 0)
			{
				assert(result.red == 32767U * effective / 255);
				assert(result.green == result.red);
				assert(result.blue == 65535U *
				    (255 - effective) / 255);
			}
			else
			{
				assert(result.red == 0 && result.green == 0 &&
				    result.blue == 0);
			}
		}
	}
}

static void test_palette_precision_and_refusal(void)
{
	uint16_t source[] = {21845, 43690, 65535};
	uint16_t tint[] = {65535, 0, 21845};
	uint16_t background[] = {49151, 32767, 16383};
	image_effect_pixel result;

	for (unsigned percent = 0; percent <= 100; percent++)
	{
		assert(image_effect_blend(source, tint, background, 255, false,
		    percent, 100, &result));
		assert(result.red == ((100 - percent) * 21845U +
		    percent * 65535U) / 100);
		assert(result.green == (100 - percent) * 43690U / 100);
		assert(result.blue == ((100 - percent) * 65535U +
		    percent * 21845U) / 100);
	}
	assert(image_effect_blend(source, tint, background, 255, true, 100,
	    100, &result));
	assert(result.kind == IMAGE_EFFECT_BACKGROUND && result.red == 0);
	const unsigned invalid[][3] = {{256, 0, 100}, {255, 101, 100},
	    {255, 0, 101}};

	for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		result = (image_effect_pixel){1, 2, 3, IMAGE_EFFECT_RGB};
		assert(!image_effect_blend(source, tint, background,
		    invalid[i][0], false, invalid[i][1], invalid[i][2], &result));
		assert(result.kind == IMAGE_EFFECT_BACKGROUND && result.red == 0);
	}
	assert(!image_effect_blend(NULL, tint, background, 1, false, 0, 100,
	    &result));
	assert(!image_effect_blend(source, tint, background, 1, false, 0, 100,
	    NULL));
}

int main(void)
{
	test_legacy_rounding();
	test_palette_precision_and_refusal();
	return 0;
}
