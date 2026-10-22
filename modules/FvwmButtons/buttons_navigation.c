/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "buttons_navigation.h"
#include <stdlib.h>

buttons_direction_score buttons_score_direction(
	buttons_rect from, buttons_rect to, buttons_direction direction)
{
	buttons_direction_score score = {0};
	int64_t dx = 2 * (int64_t)to.x + to.width -
		(2 * (int64_t)from.x + from.width);
	int64_t dy = 2 * (int64_t)to.y + to.height -
		(2 * (int64_t)from.y + from.height);

	if (from.width <= 0 || from.height <= 0 ||
		to.width <= 0 || to.height <= 0)
	{
		return score;
	}
	switch (direction)
	{
	case BUTTONS_LEFT:
	case BUTTONS_RIGHT:
		score.forward = direction == BUTTONS_LEFT ? -dx : dx;
		score.sideways = llabs(dy);
		score.aligned = (int64_t)to.y < (int64_t)from.y +
			from.height && (int64_t)from.y <
			(int64_t)to.y + to.height;
		break;
	case BUTTONS_UP:
	case BUTTONS_DOWN:
		score.forward = direction == BUTTONS_UP ? -dy : dy;
		score.sideways = llabs(dx);
		score.aligned = (int64_t)to.x < (int64_t)from.x +
			from.width && (int64_t)from.x <
			(int64_t)to.x + to.width;
		break;
	default:
		return score;
	}
	score.valid = score.forward > 0;
	return score;
}

bool buttons_score_better(
	buttons_direction_score candidate, buttons_direction_score best)
{
	if (!candidate.valid)
	{
		return false;
	}
	if (!best.valid || candidate.aligned != best.aligned)
	{
		return !best.valid || candidate.aligned;
	}
	/* Prefer the nearest column/row, then its nearest centre. Equal
	 * scores retain configuration traversal order, including containers. */
	return candidate.forward < best.forward ||
		(candidate.forward == best.forward &&
		 candidate.sideways < best.sideways);
}
