#ifndef FVWM_ICCCM_SIZE_HINTS_H
#define FVWM_ICCCM_SIZE_HINTS_H

#include "libs/fvwmrect.h"

#include <X11/Xutil.h>

typedef enum
{
	ICCCM_CONSTRAIN_OK,
	ICCCM_CONSTRAIN_OVERFLOW,
	ICCCM_CONSTRAIN_INVALID_ARGUMENT
} icccm_constrain_status;

typedef struct
{
	long flags;
	int  min_width;
	int  min_height;
	int  max_width;
	int  max_height;
	int  width_inc;
	int  height_inc;
	struct
	{
		int x;
		int y;
	} min_aspect, max_aspect;
	int base_width;
	int base_height;
	int win_gravity;
} icccm_size_hints;

icccm_constrain_status
icccm_constrain_size(const icccm_size_hints *hints, int *width, int *height,
    rectangle *frame);

#endif
