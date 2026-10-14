#ifndef FVWMLIB_PICTURE_COLOR_H
#define FVWMLIB_PICTURE_COLOR_H

#include "PictureBase.h"

typedef struct picture_color picture_color;

/* Adopt one existing allocation; retaining its exact pixel does not re-query
 * or reallocate a potentially different palette cell. Release after all
 * published pictures and workers using the allocation retire. */
picture_color *picture_color_adopt(Colormap cmap, Pixel pixel, Bool no_limit);
/* Legacy sentinel pixels have no allocation to own or free. */
picture_color *picture_color_borrow(Colormap cmap, Pixel pixel);
picture_color *picture_color_retain(picture_color *color);
void picture_color_release(Display *dpy, picture_color *color);
Pixel picture_color_pixel(const picture_color *color);

#endif
