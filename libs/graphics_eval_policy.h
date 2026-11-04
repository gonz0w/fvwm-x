/* Pure limits/timeline for non-installed drawing comparisons. */
#ifndef FVWM_GRAPHICS_EVAL_POLICY_H
#define FVWM_GRAPHICS_EVAL_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#define GRAPHICS_EVAL_MAX_SIDE 2048u
#define GRAPHICS_EVAL_MAX_PIXELS 262144u
#define GRAPHICS_EVAL_DURATION_MS 120u
#define GRAPHICS_EVAL_MAX_PAINTS 8u
#define GRAPHICS_EVAL_FRAME_MS 15u

typedef struct
{
	int32_t x, y;
	uint32_t width, height;
} graphics_eval_rect;

enum graphics_eval_preparation
{
	GRAPHICS_EVAL_REFUSED = -1,
	GRAPHICS_EVAL_EMPTY,
	GRAPHICS_EVAL_READY
};

/* Initialize out, validate budgets before multiplication, then intersect the
 * operation, clip and drawable bounds. No allocation, X work or retained
 * state. REFUSED leaves out empty and requires the caller's unchanged native
 * fallback. */
enum graphics_eval_preparation graphics_eval_prepare(
	graphics_eval_rect operation, graphics_eval_rect clip,
	uint32_t window_width, uint32_t window_height, graphics_eval_rect *out);

typedef struct
{
	uint64_t token, identity, generation;
	uint64_t started_ms, observed_ms, deadline_ms, next_ms;
	uint32_t from_rgb, to_rgb, last_rgb;
	unsigned int paints;
	bool active;
} graphics_eval_transition;

enum graphics_eval_start
{
	GRAPHICS_EVAL_START_REFUSED = -1,
	GRAPHICS_EVAL_START_IMMEDIATE,
	GRAPHICS_EVAL_START_ACTIVE
};

enum graphics_eval_tick
{
	GRAPHICS_EVAL_TICK_REFUSED = -1,
	GRAPHICS_EVAL_TICK_IDLE,
	GRAPHICS_EVAL_TICK_WAIT,
	GRAPHICS_EVAL_TICK_FRAME,
	GRAPHICS_EVAL_TICK_FINISH,
	GRAPHICS_EVAL_TICK_CANCELLED,
	GRAPHICS_EVAL_TICK_STALE
};

/* A caller-zeroed owner retains values only, never a UI/font/picture pointer.
 * Begin replaces prior work without reusing tokens. Repeated identical targets
 * do not postpone a deadline. A changed target on the same live identity
 * starts from the last delivered color. Refusal cancels prior work; native
 * paint stays with the caller. Invisible/reduced-motion/equal-color changes
 * schedule no tick. */
enum graphics_eval_start graphics_eval_transition_begin(
	graphics_eval_transition *state, uint64_t now_ms, uint64_t identity,
	uint64_t generation, uint32_t from_rgb, uint32_t to_rgb,
	bool visible, bool reduced_motion);

/* At most one paint per service call; missed frames are skipped. Stale tokens
 * cannot cancel newer work. Current identity/generation/visibility or
 * monotonic time failure cancels without drawable access. Only FRAME/FINISH
 * permits paint. rgb is initialized even on refusal. Cancellation itself
 * performs no UI work. */
enum graphics_eval_tick graphics_eval_transition_sample(
	graphics_eval_transition *state, uint64_t token, uint64_t identity,
	uint64_t generation, uint64_t now_ms, bool visible, uint32_t *rgb);
void graphics_eval_transition_cancel(graphics_eval_transition *state);
/* Pure calculation after the final FPending check: -1 means no timer. */
int graphics_eval_transition_timeout(
	const graphics_eval_transition *state, uint64_t now_ms);

#endif
