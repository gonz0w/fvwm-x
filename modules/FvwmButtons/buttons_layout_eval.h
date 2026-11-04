#ifndef FVWM_BUTTONS_LAYOUT_EVAL_H
#define FVWM_BUTTONS_LAYOUT_EVAL_H

#include "libs/ui_layout.h"

#ifdef FVWM_LAYOUT_EVAL
struct button_info_struct;
void buttons_layout_eval_metadata(struct button_info_struct *const *buttons,
	size_t count);
bool buttons_layout_eval_row(int available, int height, const int *minimum,
	const unsigned *weights, size_t count, int *widths, bool *clipped);
#else
/* Ordinary binaries retain the direct solver, including its input contract. */
#define buttons_layout_eval_metadata(buttons, count) ((void)0)
#define buttons_layout_eval_row(width, height, minimum, weights, count, \
	widths, clipped) \
	ui_layout_row(width, minimum, weights, count, widths, clipped)
#endif

#endif
