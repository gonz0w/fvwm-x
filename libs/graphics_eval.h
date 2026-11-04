/* Non-installed comparison painter; normal modules do not link this owner. */
#ifndef FVWM_GRAPHICS_EVAL_H
#define FVWM_GRAPHICS_EVAL_H

#include <stddef.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "graphics_eval_policy.h"

#define GRAPHICS_EVAL_MAX_OPERATIONS 16u
#define GRAPHICS_EVAL_REVERSE_LINE 1u
#define GRAPHICS_EVAL_HORIZONTAL_GRADIENT 2u

enum graphics_eval_kind
{
	GRAPHICS_EVAL_LINE,
	GRAPHICS_EVAL_RECTANGLE,
	GRAPHICS_EVAL_ELLIPSE,
	GRAPHICS_EVAL_GRADIENT
};

typedef struct
{
	enum graphics_eval_kind kind;
	graphics_eval_rect rectangle; /* full actual-pixel footprint */
	uint32_t foreground, background; /* explicit RGB8, no text/color
		lookup */
	unsigned int flags;
} graphics_eval_operation;

typedef struct
{
	uint64_t attempts, refused, empty, committed;
	uint64_t pixmap_requests, pixmap_retires, gc_requests, gc_retires;
	uint64_t surface_requests, surface_retires;
	uint64_t context_requests, context_retires;
	uint64_t pattern_requests, pattern_retires;
	uint64_t preserve_copies, commit_copies, pixels_copied;
	uint64_t scratch_pixels_requested, peak_scratch_pixels;
} graphics_eval_stats;

Bool graphics_eval_cairo_backend(void);
Bool graphics_eval_supported(
	Display *display, Visual *visual, unsigned int depth);
Bool graphics_eval_clock(uint64_t *milliseconds);

/* Borrow display/drawable/visual and operation values only for this call. The
 * caller owns mapping, lifetime, background regeneration and native fallback.
 * bounds encloses every operation; clip is prepared by the draw/hit owner.
 * Only the recorded default TrueColor depth24 RGB8 visual is admitted
 * initially.  Preflight and injected/client preparation refusal do not modify
 * destination pixels. One bounded scratch group is copied only after Cairo
 * flush/finish and client status checks. True means submission or an empty
 * group, not server acknowledgement; keep existing asynchronous-X
 * handlers/ordering barriers. No scratch/font/item/picture reference is
 * retained. Statistics count requested own resources; Cairo's internal caches
 * and server acceptance are separate. A false result requires the unchanged
 * native operation at the same draw site. */
Bool graphics_eval_paint(
	Display *display, Drawable destination, Visual *visual,
	unsigned int depth, uint32_t window_width, uint32_t window_height,
	graphics_eval_rect bounds,
	graphics_eval_rect clip, const graphics_eval_operation *operations,
	size_t count, Bool antialias, Bool refuse_preparation,
	graphics_eval_stats *stats);

#endif
