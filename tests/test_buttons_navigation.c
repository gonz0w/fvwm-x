/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "modules/FvwmButtons/buttons_navigation.h"
#include <assert.h>
#include <limits.h>

int main(void)
{
	buttons_rect origin = {0, 0, 100, 50};
	buttons_rect right = {100, 0, 100, 50};
	buttons_rect below = {0, 50, 100, 50};
	buttons_rect diagonal = {100, 60, 100, 50};
	buttons_direction_score none = {0};
	buttons_direction_score best = buttons_score_direction(
		origin, right, BUTTONS_RIGHT);

	assert(best.valid && best.aligned && best.forward == 200);
	assert(buttons_score_better(best, none));
	assert(!buttons_score_better(none, best));
	assert(!buttons_score_better(best, best));
	assert(!buttons_score_direction(origin, right, BUTTONS_LEFT).valid);
	assert(!buttons_score_direction(origin, below, BUTTONS_RIGHT).valid);
	assert(buttons_score_direction(origin, below, BUTTONS_DOWN).valid);
	assert(buttons_score_direction(below, origin, BUTTONS_UP).valid);
	assert(buttons_score_direction(right, origin, BUTTONS_LEFT).valid);
	assert(!buttons_score_better(buttons_score_direction(
		origin, diagonal, BUTTONS_RIGHT), best));
	right.x = 200;
	assert(buttons_score_better(best, buttons_score_direction(
		origin, right, BUTTONS_RIGHT)));
	right = (buttons_rect){100, 10, 100, 50};
	assert(buttons_score_better(best, buttons_score_direction(
		origin, right, BUTTONS_RIGHT)));
	right.width = 0;
	assert(!buttons_score_direction(origin, right, BUTTONS_RIGHT).valid);
	assert(!buttons_score_direction(origin, origin, BUTTONS_DOWN).valid);
	assert(!buttons_score_direction(origin, origin,
		(buttons_direction)99).valid);
	origin = (buttons_rect){INT_MIN, INT_MIN, 1, 1};
	right = (buttons_rect){INT_MAX, INT_MAX, INT_MAX, INT_MAX};
	best = buttons_score_direction(origin, right, BUTTONS_RIGHT);
	assert(best.valid && best.forward > INT_MAX && !best.aligned);
	assert(buttons_score_direction(right, origin, BUTTONS_LEFT).valid);
	return 0;
}
