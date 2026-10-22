#ifndef BUTTONS_NAVIGATION_H
#define BUTTONS_NAVIGATION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	BUTTONS_LEFT, BUTTONS_RIGHT, BUTTONS_UP, BUTTONS_DOWN
} buttons_direction;

typedef struct
{
	int x, y, width, height;
} buttons_rect;

typedef struct
{
	bool valid;
	bool aligned;
	int64_t forward, sideways;
} buttons_direction_score;

buttons_direction_score buttons_score_direction(
	buttons_rect from, buttons_rect to, buttons_direction direction);
bool buttons_score_better(
	buttons_direction_score candidate, buttons_direction_score best);

#endif
