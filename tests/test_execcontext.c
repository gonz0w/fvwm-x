/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "fvwm/execcontext.h"
#include "fvwm/module_list.h"
#include "libs/FEvent.h"
#include "libs/wcontext.h"

#include <assert.h>
#include <string.h>

Display *dpy;

static void test_clone_owns_trigger(void)
{
	XEvent trigger = { 0 };
	FvwmWindow window = { 0 };
	fmodule module = { 0 };
	exec_context_changes_t changes = { 0 };
	const exec_context_t *parent;
	const exec_context_t *clone;
	const exec_context_t *nested;

	trigger.xbutton.type = ButtonPress;
	trigger.xbutton.window = 123;
	trigger.xbutton.time = 71;
	trigger.xbutton.x_root = 45;
	changes.type = EXCT_MODULE;
	changes.x.etrigger = &trigger;
	changes.w.fw = &window;
	changes.w.w = 123;
	changes.w.wcontext = C_WINDOW;
	changes.m.module = &module;
	parent = exc_create_context(&changes, ECC_TYPE | ECC_ETRIGGER |
		ECC_FW | ECC_W | ECC_WCONTEXT | ECC_MODULE);
	clone = exc_clone_context(parent, NULL, 0);
	assert(clone->x.etrigger != parent->x.etrigger);
	assert(memcmp(clone->x.etrigger, &trigger, sizeof(trigger)) == 0);
	assert(clone->w.fw == &window && clone->m.module == &module);
	assert(clone->w.w == 123 && clone->w.wcontext == C_WINDOW);
	assert(clone->type == EXCT_MODULE && clone->x.elast == parent->x.elast);
	trigger.xbutton.time = 72;
	parent->x.etrigger->xbutton.time = 73;
	assert(clone->x.etrigger->xbutton.time == 71);
	nested = exc_clone_context(clone, NULL, 0);
	assert(nested->x.etrigger != clone->x.etrigger);
	clone->x.etrigger->xbutton.x_root = 99;
	assert(nested->x.etrigger->xbutton.x_root == 45);
	exc_destroy_context(parent);
	exc_destroy_context(clone);
	assert(nested->x.etrigger->xbutton.time == 71);
	assert(nested->x.etrigger->xbutton.x_root == 45);
	exc_destroy_context(nested);
}

static void test_clone_trigger_override(void)
{
	const exec_context_t *parent = exc_create_null_context();
	XEvent trigger = { 0 };
	exec_context_changes_t changes = { 0 };
	const exec_context_t *clone;

	trigger.xkey.type = KeyPress;
	trigger.xkey.keycode = 44;
	changes.x.etrigger = &trigger;
	changes.type = EXCT_SCHEDULE;
	clone = exc_clone_context(parent, &changes, ECC_ETRIGGER | ECC_TYPE);
	assert(clone->type == EXCT_SCHEDULE && parent->type == EXCT_NULL);
	assert(clone->x.etrigger != &trigger);
	trigger.xkey.keycode = 55;
	assert(clone->x.etrigger->xkey.keycode == 44);
	exc_destroy_context(clone);
	fev_fake_event(&trigger);
	changes.x.etrigger = NULL;
	clone = exc_clone_context(parent, &changes, ECC_ETRIGGER);
	assert(clone->x.etrigger->xkey.keycode == 55);
	trigger.xkey.keycode = 66;
	fev_fake_event(&trigger);
	assert(clone->x.etrigger->xkey.keycode == 55);
	assert(clone->x.elast->xkey.keycode == 66);
	exc_destroy_context(parent);
	assert(clone->x.etrigger->xkey.keycode == 55);
	exc_destroy_context(clone);
}

int main(void)
{
	test_clone_owns_trigger();
	test_clone_trigger_override();

	return 0;
}
