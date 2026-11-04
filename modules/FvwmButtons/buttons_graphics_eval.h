#ifndef BUTTONS_GRAPHICS_EVAL_H
#define BUTTONS_GRAPHICS_EVAL_H

#ifdef FVWM_GRAPHICS_EVAL
#include "libs/graphics_eval.h"
#include "libs/graphics_eval_observer.h"
graphics_eval_draw_scope buttons_eval_draw_start(
	enum graphics_observer_operation operation);
void buttons_eval_draw_end(graphics_eval_draw_scope scope, Bool fallback);
#define buttons_eval_draw_begin(scope, operation) \
	graphics_eval_draw_scope scope = buttons_eval_draw_start(operation)
unsigned long buttons_eval_foreground(button_info *button);
void buttons_eval_focus_begin(button_info *button, unsigned long from);
void buttons_eval_cancel(Bool repaint);
void buttons_eval_event(const XEvent *event);
void buttons_eval_service(void);
int buttons_eval_timeout(void);
Bool buttons_eval_focus(button_info *button, int x, int y, int width,
	int height, unsigned long foreground);
Bool buttons_eval_indicator(int x, int y, unsigned long foreground);
#else
#define buttons_eval_draw_begin(scope, operation) ((void)0)
#define buttons_eval_draw_end(scope, fallback) ((void)0)
#define buttons_eval_focus_begin(button, from) ((void)0)
#define buttons_eval_cancel(repaint) ((void)0)
#define buttons_eval_event(event) ((void)0)
#define buttons_eval_service() ((void)0)
#define buttons_eval_timeout() (-1)
#define buttons_eval_focus(button, x, y, width, height, foreground) False
#define buttons_eval_indicator(x, y, foreground) False
#endif
#endif
