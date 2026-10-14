/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/FProperty.h"
#include <X11/Xatom.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	Display *display = XOpenDisplay(NULL);
	assert(display != NULL);
	Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
	    0, 0, 10, 10, 0, 0, 0);
	Atom property = XInternAtom(display, "FVWM_PROPERTY_TEST", False);
	FPropertyLimits limits = {.min_bytes = 4, .max_bytes = 16, .format = 32};
	unsigned char *data = (void *)1;
	size_t bytes = 99;
	assert(FPropertyRead(display, window, property, XA_CARDINAL, limits,
	    &data, &bytes) == FPROPERTY_MISSING);
	assert(data == NULL && bytes == 0);
	limits = (FPropertyLimits){.min_bytes = 0, .max_bytes = 5, .format = 8};
	XChangeProperty(display, window, property, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)"", 0);
	assert(FPropertyRead(display, window, property, XA_STRING, limits,
	    &data, &bytes) == FPROPERTY_OK);
	assert(bytes == 0 && data[0] == 0);
	free(data);
	limits = (FPropertyLimits){.min_bytes = 4, .max_bytes = 16, .format = 32};
	unsigned long values[] = {0, 1, 0x80000000UL, 0xffffffffUL, 5};
	XChangeProperty(display, window, property, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)values, 4);
	assert(FPropertyRead(display, window, property, XA_CARDINAL, limits,
	    &data, &bytes) == FPROPERTY_OK);
	assert(bytes == 16 && data[16] == 0);
	for (size_t i = 0; i < 4; i++)
	{
		assert(((uint32_t *)data)[i] == values[i]);
	}
	free(data);
	XChangeProperty(display, window, property, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)values, 5);
	assert(FPropertyRead(display, window, property, XA_CARDINAL, limits,
	    &data, &bytes) == FPROPERTY_TOO_LARGE);
	assert(data == NULL && bytes == 0);
	assert(FPropertyRead(display, window, property, XA_ATOM, limits,
	    &data, &bytes) == FPROPERTY_INVALID);
	XChangeProperty(display, window, property, XA_CARDINAL, 8,
	    PropModeReplace, (unsigned char *)"abcd", 4);
	assert(FPropertyRead(display, window, property, XA_CARDINAL, limits,
	    &data, &bytes) == FPROPERTY_INVALID);
	limits = (FPropertyLimits){.min_bytes = 1, .max_bytes = 5, .format = 8};
	XChangeProperty(display, window, property, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)"abcde", 5);
	assert(FPropertyRead(display, window, property, XA_STRING, limits,
	    &data, &bytes) == FPROPERTY_OK);
	assert(bytes == 5 && strcmp((char *)data, "abcde") == 0);
	free(data);
	XChangeProperty(display, window, property, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)"abcdef", 6);
	assert(FPropertyRead(display, window, property, XA_STRING, limits,
	    &data, &bytes) == FPROPERTY_INVALID);
	assert(data == NULL && bytes == 0);
	/* Preserve text encodings and reset rejected outputs. */
	XTextProperty text;
	const char *encodings[] = {"STRING", "COMPOUND_TEXT", "UTF8_STRING"};
	for (size_t i = 0; i < 3; i++)
	{
		Atom encoding = XInternAtom(display, encodings[i], False);
		XChangeProperty(display, window, XA_WM_NAME, encoding, 8,
		    PropModeReplace, (unsigned char *)"name", 4);
		assert(FGetWMName(display, window, &text));
		assert(text.encoding == encoding && text.format == 8);
		assert(text.nitems == 4 && strcmp((char *)text.value, "name") == 0);
		XFree(text.value);
	}
	unsigned char *large = calloc(FPROPERTY_TEXT_MAX_BYTES + 1, 1);
	assert(large != NULL);
	memset(large, 'x', FPROPERTY_TEXT_MAX_BYTES + 1);
	XChangeProperty(display, window, XA_WM_NAME, XA_STRING, 8,
	    PropModeReplace, large, FPROPERTY_TEXT_MAX_BYTES);
	assert(FGetWMName(display, window, &text));
	assert(text.nitems == FPROPERTY_TEXT_MAX_BYTES);
	XFree(text.value);
	XChangeProperty(display, window, XA_WM_NAME, XA_STRING, 8,
	    PropModeReplace, large, FPROPERTY_TEXT_MAX_BYTES + 1);
	assert(!FGetWMName(display, window, &text));
	assert(text.value == NULL && text.nitems == 0 && text.encoding == None);
	XChangeProperty(display, window, XA_WM_ICON_NAME, XA_STRING, 32,
	    PropModeReplace, (unsigned char *)values, 1);
	assert(!FGetWMIconName(display, window, &text));
	free(large);

	XClassHint hint;
	const unsigned char class_data[] = "instance\0Class";
	/* Both standard termination and Xlib's legacy missing final NUL work. */
	for (int trim = 0; trim <= 1; trim++)
	{
		XChangeProperty(display, window, XA_WM_CLASS, XA_STRING, 8,
		    PropModeReplace, class_data, sizeof(class_data) - trim);
		assert(FGetClassHint(display, window, &hint));
		assert(strcmp(hint.res_name, "instance") == 0);
		assert(strcmp(hint.res_class, "Class") == 0);
		XFree(hint.res_name);
		XFree(hint.res_class);
	}
	XChangeProperty(display, window, XA_WM_CLASS, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)"broken", 6);
	assert(!FGetClassHint(display, window, &hint));
	assert(hint.res_name == NULL && hint.res_class == NULL);
	XChangeProperty(display, window, XA_WM_CLASS, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)values, 1);
	assert(!FGetClassHint(display, window, &hint));

	const char *command[] = {"application", "", "two words", ""};
	XSetCommand(display, window, (char **)command, 4);
	char **argv = NULL;
	int argc = 0;
	assert(FGetCommand(display, window, &argv, &argc));
	assert(argc == 4);
	for (int i = 0; i < argc; i++)
	{
		assert(strcmp(argv[i], command[i]) == 0);
	}
	XFreeStringList(argv);
	XChangeProperty(display, window, XA_WM_COMMAND, XA_ATOM, 8,
	    PropModeReplace, (unsigned char *)"wrong", 5);
	assert(!FGetCommand(display, window, &argv, &argc));
	assert(argv == NULL && argc == 0);

	Atom leader_property = XInternAtom(display, "WM_CLIENT_LEADER", False);
	unsigned long leader = window;
	XChangeProperty(display, window, leader_property, XA_WINDOW, 32,
	    PropModeReplace, (unsigned char *)&leader, 1);
	assert(FGetClientLeader(display, window, leader_property) == window);
	XChangeProperty(display, window, leader_property, XA_WINDOW, 8,
	    PropModeReplace, (unsigned char *)"bad", 3);
	assert(FGetClientLeader(display, window, leader_property) == None);
	XChangeProperty(display, window, leader_property, XA_WINDOW, 32,
	    PropModeReplace, (unsigned char *)values, 2);
	assert(FGetClientLeader(display, window, leader_property) == None);
	Atom id = XInternAtom(display, "SM_CLIENT_ID", False);
	XChangeProperty(display, window, id, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)"id", 2);
	char *session_id = FGetSessionString(display, window, id);
	assert(session_id != NULL && strcmp(session_id, "id") == 0);
	XFree(session_id);
	XChangeProperty(display, window, id, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)"", 0);
	assert(FGetSessionString(display, window, id) == NULL);
	XChangeProperty(display, window, id, XA_ATOM, 8,
	    PropModeReplace, (unsigned char *)"id", 2);
	assert(FGetSessionString(display, window, id) == NULL);

	Atom colormaps = XInternAtom(display, "WM_COLORMAP_WINDOWS", False);
	Window *windows = NULL;
	int count = 0;
	XChangeProperty(display, window, colormaps, XA_WINDOW, 32,
	    PropModeReplace, (unsigned char *)values, 4);
	assert(FGetWMColormapWindows(display, window, &windows, &count));
	assert(count == 4);
	for (int i = 0; i < count; i++)
	{
		assert(windows[i] == values[i]);
	}
	XFree(windows);
	XChangeProperty(display, window, colormaps, XA_ATOM, 32,
	    PropModeReplace, (unsigned char *)values, 4);
	assert(!FGetWMColormapWindows(display, window, &windows, &count));
	assert(windows == NULL && count == 0);

	XDestroyWindow(display, window);
	XCloseDisplay(display);
	puts("property types, caps and packed LP64 conversion passed");
	return 0;
}
