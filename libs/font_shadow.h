#ifndef FVWMLIB_FONT_SHADOW_H
#define FVWMLIB_FONT_SHADOW_H

#include <stdbool.h>
#include <stdint.h>

/* Counts actual positions, including overlaps between selected directions. */
bool font_shadow_count(int size, int offset, unsigned directions, bool center,
    uint32_t limit, uint32_t *positions);
/* Center's row-major border, without scanning the excluded interior. */
bool font_shadow_center_position(int size, int offset, uint32_t index,
    int *x, int *y);

#endif
