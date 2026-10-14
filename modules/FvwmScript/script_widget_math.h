#ifndef FVWMSCRIPT_SCRIPT_WIDGET_MATH_H
#define FVWMSCRIPT_SCRIPT_WIDGET_MATH_H

#include <stdbool.h>
#include <stdint.h>

/* Retain legacy startup expansion, saturating only unrepresentable endpoints. */
int script_widget_initial_maximum(int minimum, int maximum, int increment,
	bool include_equal);
/* Drawing clamps a copy of the value; stored values are never changed here.
 * Empty/inverted ranges and tracks have position zero. */
int script_widget_position(int value, int minimum, int maximum, int extent,
	int inset, bool reverse);
int script_widget_pointer_position(int position, int origin, int extent);
/* Preserve the scrollbar's two integer divisions and resulting rounding. */
int script_widget_pointer_value(int position, int extent, int minimum,
	int maximum, bool reverse);
/* MiniScroll wraps without overflowing before testing the endpoint. */
int script_widget_step(int value, int minimum, int maximum, bool forward);

/* X rectangle extents are unsigned 16-bit fields. Do arithmetic before
 * narrowing, leaving declared runtime geometry unchanged. */
int script_widget_dimension(int64_t value);
int script_list_row_height(int font_height);
int script_list_first(int first, int count, int visible);
void script_list_thumb(int first, int count, int visible, int track,
	int *position, int *size);
int script_list_pointer_first(int pointer, int origin, int track, int count,
	int visible);
int script_list_move(int first, int delta, int count, int visible);
int script_list_cell(int first, int row, int count);

#endif
