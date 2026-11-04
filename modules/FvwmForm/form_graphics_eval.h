#ifndef FORM_GRAPHICS_EVAL_H
#define FORM_GRAPHICS_EVAL_H

#ifdef FVWM_GRAPHICS_EVAL
#include "libs/graphics_eval.h"
#include "libs/graphics_eval_observer.h"
void form_eval_observer_init(void);
graphics_eval_draw_scope form_eval_draw_start(
	enum graphics_observer_operation operation);
void form_eval_draw_end(graphics_eval_draw_scope scope, Bool fallback);
#define form_eval_draw_begin(scope, operation) \
	graphics_eval_draw_scope scope = form_eval_draw_start(operation)
void form_eval_cancel(Bool repaint);
void form_eval_event(const XEvent *event);
void form_eval_choice_begin(Item *item);
void form_eval_service(void);
int form_eval_timeout(void);
Bool form_eval_choice_outline(Item *item);
Bool form_eval_rectangle(Item *item, int x, int y, int width, int height,
	unsigned long foreground, const XRectangle *clip);
#else
#define form_eval_observer_init() ((void)0)
#define form_eval_draw_begin(scope, operation) ((void)0)
#define form_eval_draw_end(scope, fallback) ((void)0)
#define form_eval_cancel(repaint) ((void)0)
#define form_eval_event(event) ((void)0)
#define form_eval_choice_begin(item) ((void)0)
#define form_eval_service() ((void)0)
#define form_eval_timeout() (-1)
#define form_eval_choice_outline(item) False
#define form_eval_rectangle(item, x, y, width, height, foreground, clip) False
#endif
#endif
