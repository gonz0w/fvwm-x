/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/capture_policy.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
	size_t bytes = 99;
	capture_channel channels[3];
	unsigned char rgb[3];
	unsigned long rgb565[3] = {0xf800, 0x7e0, 0x1f};
	unsigned long rgb888[3] = {0xff0000, 0xff00, 0xff};
	unsigned long rgb101010[3] = {0x3ff00000, 0xffc00, 0x3ff};
	unsigned long invalid[3] = {0xf800, 0x7e0, 0x20};

	assert(capture_geometry(64, 48, 0, 128, 16, &bytes));
	assert(bytes == 192);
	assert(capture_geometry(4096, 4096, 0, 16384, 32, &bytes));
	assert(!capture_geometry(4096, 4097, 0, 16384, 32, &bytes));
	assert(bytes == 0);
	assert(!capture_geometry(0, 1, 0, 4, 32, &bytes));
	assert(!capture_geometry(1, -1, 0, 4, 32, &bytes));
	assert(!capture_geometry(1, 1, -1, 4, 32, &bytes));
	assert(!capture_geometry(64, 48, 0, 127, 16, &bytes));
	assert(!capture_geometry(1, 1, INT_MAX, 4, 32, &bytes));
	assert(!capture_geometry(1, 2, 0, INT_MAX, 32, &bytes));
	assert(!capture_geometry(1, 1, 0, 8, 64, &bytes));
	assert(!capture_geometry(1, 1, 0, 4, 32, NULL));
	assert(capture_channels(rgb565, 16, channels));
	capture_rgb(0xf800, channels, rgb);
	assert(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 0);
	capture_rgb(0x7e0, channels, rgb);
	assert(rgb[0] == 0 && rgb[1] == 255 && rgb[2] == 0);
	assert(capture_channels(rgb888, 32, channels));
	capture_rgb(0x224466, channels, rgb);
	assert(rgb[0] == 0x22 && rgb[1] == 0x44 && rgb[2] == 0x66);
	assert(capture_channels(rgb101010, 32, channels));
	capture_rgb(0x3ff80000, channels, rgb);
	assert(rgb[0] == 255 && rgb[1] == 127 && rgb[2] == 0);
	assert(!capture_channels(invalid, 16, channels)); /* overlap */
	invalid[2] = 0x15;
	assert(!capture_channels(invalid, 16, channels)); /* sparse */
	invalid[2] = 0;
	assert(!capture_channels(invalid, 16, channels));
	assert(!capture_channels(rgb888, 16, channels)); /* too wide */
	assert(!capture_channels(NULL, 32, channels));
	assert(!capture_channels(rgb888, 32, NULL));
	puts("Capture geometry, masks and RGB policy passed.");
	return 0;
}
