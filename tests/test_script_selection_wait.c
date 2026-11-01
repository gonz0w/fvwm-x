/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/Widgets/Tools.h"
#include "libs/FEvent.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int fd[2];
static X11base base;
static struct XObj widget;
static XEvent events[48];
static size_t queued, draws;
static int x_pipe[2], module_pipe[2];
static enum {
	NORMAL, NESTED, MUTATED, PIPE_LOST, EXACT, BAD_EXACT,
	MISSING_NUL, EMPTY, NO_SERVICE_HUP
} mode;

static void draw(struct XObj *object, XEvent *event)
{
	assert(object == &widget && event->type == Expose);
	draws++;
	if (mode == MUTATED)
		object->resource_revision++;
}

Bool __wrap_FCheckIfEvent(Display *display, XEvent *event,
	Bool (*predicate)(Display *, XEvent *, XPointer), XPointer context)
{
	for (size_t i = 0; i < queued; i++)
		if (predicate(display, &events[i], context))
		{
			*event = events[i];
			memmove(&events[i], &events[i + 1],
				(--queued - i) * sizeof(events[0]));
			return True;
		}
	return False;
}

Bool __wrap_FCheckTypedWindowEvent(Display *display, Window window,
	int type, XEvent *event)
{
	(void)display; (void)window; (void)type; (void)event;
	assert(!"selection servicing must not coalesce additional events");
	return False;
}

Bool __wrap_FCheckTypedEvent(Display *display, int type, XEvent *event)
{
	(void)display; (void)type; (void)event;
	assert(!"selection servicing must not coalesce additional events");
	return False;
}

static FSelectionStatus transfer(FSelectionService service, void *context,
	FSelectionResult *result)
{
	if (mode == NESTED)
	{
		FSelectionResult nested = {.data = (unsigned char *)1,
			.length = 99, .type = XA_ATOM};

		assert(FvwmScriptSelectionExact(100, 101, CurrentTime,
			&nested) == FSELECTION_CANCELLED);
		assert(nested.data == NULL && nested.length == 0 &&
			nested.type == None);
	}
	if (mode == NO_SERVICE_HUP)
	{
		close(module_pipe[1]);
		module_pipe[1] = -1;
	}
	bool serviced = mode == NO_SERVICE_HUP || service(context);

	assert(serviced == (mode != MUTATED && mode != PIPE_LOST));
	/* Deliberately return a stale successful buffer after cancellation too:
	 * the module's final operation-initial fence must retire it. */
	const unsigned char text[] = {'o', 'k', 0};
	const unsigned char invalid[] = {'a', 0, 'b', 0};
	const unsigned char empty[] = {0};
	const unsigned char *bytes = mode == BAD_EXACT ? invalid :
		(mode == EMPTY ? empty : text);
	size_t length = mode == BAD_EXACT ? sizeof(invalid) :
		(mode == EMPTY ? 1 : (mode == EXACT ? sizeof(text) : 2));

	result->data = malloc(length + 1);
	assert(result->data != NULL);
	memcpy(result->data, bytes, length);
	result->data[length] = 0;
	result->length = length;
	result->type = XA_STRING;
	return FSELECTION_OK;
}

FSelectionStatus __wrap_FSelectionRequestTextNegotiated(Display *display,
	Atom selection, Atom property, Time time, int timeout,
	FSelectionService service, void *context, FSelectionResult *result)
{
	(void)time;
	assert(display == dpy && selection == XA_PRIMARY);
	assert(property == propriete && timeout == FSELECTION_TIMEOUT_MS);
	return transfer(service, context, result);
}

FSelectionStatus __wrap_FSelectionRequestExact(Display *display,
	Atom selection, Atom target, Atom property, Time time, int timeout,
	FSelectionService service, void *context, FSelectionResult *result)
{
	(void)time;
	assert(display == dpy && selection == 100 && target == 101);
	assert(property == propriete && timeout == FSELECTION_TIMEOUT_MS);
	return transfer(service, context, result);
}

static void setup(void)
{
	assert(pipe(x_pipe) == 0 && pipe(module_pipe) == 0);
	x_fd = x_pipe[0];
	fd[1] = module_pipe[0];
	/* No Xlib operation may dereference this fixture token. */
	dpy = (Display *)1;
	Root = 42;
	base = (X11base){.win = 43};
	x11base = &base;
	widget = (struct XObj){.win = 44, .DrawObj = draw,
		.resource_revision = 1};
	tabxobj[0] = &widget;
	nbobj = 1;
	propriete = 102;
	isTerminated = 0;
	queued = draws = 0;
}

static void finish(void)
{
	close(x_pipe[0]);
	close(x_pipe[1]);
	close(module_pipe[0]);
	if (module_pipe[1] >= 0)
		close(module_pipe[1]);
	x11base = NULL;
	tabxobj[0] = NULL;
	nbobj = 0;
}

int __wrap_main(int argc, char **argv)
{
	(void)argc; (void)argv;
	FSelectionResult result;

	setup();
	mode = NORMAL;
	events[queued++] = (XEvent){.xkey = {.type = KeyPress, .window = 44}};
	events[queued++] = (XEvent){.xselection = {.type = SelectionNotify,
		.requestor = 900}};
	events[queued++] = (XEvent){.xproperty = {.type = PropertyNotify,
		.window = 900, .atom = propriete}};
	events[queued++] = (XEvent){.xexpose = {.type = Expose, .window = 901}};
	for (unsigned i = 0; i < 40; i++)
		events[queued++] = (XEvent){.xexpose = {.type = Expose,
			.window = widget.win, .width = 1, .height = 1}};
	assert(write(module_pipe[1], "p", 1) == 1);
	assert(FvwmScriptSelectionText(&widget, XA_PRIMARY, CurrentTime,
		&result) == FSELECTION_OK);
	assert(draws == 32 && queued == 12);
	assert(events[0].type == KeyPress && events[1].type == SelectionNotify);
	assert(events[2].type == PropertyNotify &&
		events[3].xany.window == 901);
	char byte = 0;

	assert(read(module_pipe[0], &byte, 1) == 1 && byte == 'p');
	FSelectionResultFree(&result);
	finish();

	setup();
	mode = NESTED;
	assert(FvwmScriptSelectionText(&widget, XA_PRIMARY, CurrentTime,
		&result) == FSELECTION_OK);
	FSelectionResultFree(&result);
	finish();

	setup();
	mode = MUTATED;
	events[queued++] = (XEvent){.xexpose = {.type = Expose,
		.window = widget.win, .width = 1, .height = 1}};
	assert(FvwmScriptSelectionText(&widget, XA_PRIMARY, CurrentTime,
		&result) == FSELECTION_CANCELLED);
	assert(result.data == NULL && result.length == 0 &&
		result.type == None);
	finish();

	setup();
	mode = PIPE_LOST;
	close(module_pipe[1]);
	module_pipe[1] = -1;
	assert(FvwmScriptSelectionText(&widget, XA_PRIMARY, CurrentTime,
		&result) == FSELECTION_CANCELLED);
	assert(isTerminated && result.data == NULL);
	finish();

	setup();
	mode = NO_SERVICE_HUP;
	assert(FvwmScriptSelectionText(&widget, XA_PRIMARY, CurrentTime,
		&result) == FSELECTION_CANCELLED);
	assert(isTerminated && result.data == NULL && result.length == 0);
	finish();

	setup();
	mode = EXACT;
	assert(FvwmScriptSelectionExact(100, 101, CurrentTime,
		&result) == FSELECTION_OK);
	assert(result.length == 3 && memcmp(result.data, "ok", 3) == 0);
	FSelectionResultFree(&result);
	mode = BAD_EXACT;
	assert(FvwmScriptSelectionExact(100, 101, CurrentTime,
		&result) == FSELECTION_INVALID);
	assert(result.data == NULL && result.length == 0);
	mode = MISSING_NUL;
	assert(FvwmScriptSelectionExact(100, 101, CurrentTime,
		&result) == FSELECTION_INVALID);
	assert(result.data == NULL && result.length == 0);
	mode = EMPTY;
	assert(FvwmScriptSelectionExact(100, 101, CurrentTime,
		&result) == FSELECTION_OK);
	assert(result.length == 1 && result.data[0] == 0);
	FSelectionResultFree(&result);
	mode = EXACT;
	assert(FvwmScriptSelectionExact(100, 101, CurrentTime,
		&result) == FSELECTION_OK);
	FSelectionResultFree(&result);
	finish();
	return 0;
}
