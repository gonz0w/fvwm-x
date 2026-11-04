#ifndef FVWM_CLAY_LAYOUT_EVAL_H
#define FVWM_CLAY_LAYOUT_EVAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLAY_LAYOUT_EVAL_MAX_ITEMS 256
#define CLAY_LAYOUT_EVAL_MAX_EXTENT 32767
#define CLAY_LAYOUT_EVAL_ARENA_LIMIT (2u * 1024u * 1024u)

/* Private owner: zero initialize, never copy/move while initialized, destroy
 * once after use. No UI/X/font/picture or arena-backed output is retained. */
typedef struct
{
	void *context;
	void *arena;
	size_t arena_bytes;
	bool failed;
} clay_layout_eval;

typedef struct
{
	uint32_t id;
	bool found;
	float x, y, width, height;
} clay_layout_eval_box;

enum clay_layout_eval_result
{
	CLAY_LAYOUT_EVAL_ERROR = -1,
	CLAY_LAYOUT_EVAL_REFUSED,
	CLAY_LAYOUT_EVAL_READY
};

/* Preflight the whole bounded arena before entering upstream initialization.
 * Fallible allocation preserves the caller's native fallback on exhaustion. */
bool clay_layout_eval_init(clay_layout_eval *state, size_t arena_limit);
void clay_layout_eval_destroy(clay_layout_eval *state);

/* Only equal positive weights and noncompressed rows enter Clay. Its GROW
 * policy equalizes small controls; it is not native minimum-plus-surplus.
 * Every failure leaves widths untouched and requires the native fallback. */
enum clay_layout_eval_result clay_layout_eval_row(clay_layout_eval *state,
	int available, int height, const int *minimum, const unsigned *weights,
	size_t count, int *widths);

/* Pure output boundary: unique IDs, finite adjacent bounds and exact coverage.
 * Round shared cumulative boundaries once, then publish a complete copy. */
bool clay_layout_eval_widths(int available, int height,
	const clay_layout_eval_box *boxes, size_t count, int *widths);

#endif
