#ifndef FVWM_CAPTURE_POLICY_H
#define FVWM_CAPTURE_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FVWM_CAPTURE_MAX_PIXELS (16U * 1024U * 1024U)

typedef struct
{
	uint32_t mask;
	unsigned shift;
	uint32_t maximum;
} capture_channel;

bool capture_geometry(int width, int height, int xoffset, int stride,
	int bits_per_pixel, size_t *row_bytes);
bool capture_channels(const unsigned long masks[3], int bits_per_pixel,
	capture_channel channels[3]);
void capture_rgb(unsigned long pixel, const capture_channel channels[3],
	unsigned char rgb[3]);

#endif
