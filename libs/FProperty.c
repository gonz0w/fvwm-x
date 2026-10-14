/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "FProperty.h"
#include "safemalloc.h"
#include "log.h"
#include <X11/Xatom.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool
FPropertySizeIsSafe(unsigned long items, int format, unsigned long bytes_after,
    FPropertyLimits limits, size_t *bytes)
{
	*bytes = 0;
	if (format != limits.format ||
	    (format != 8 && format != 16 && format != 32) ||
	    limits.min_bytes > limits.max_bytes ||
	    limits.max_bytes > INT_MAX || bytes_after != 0 ||
	    items > limits.max_bytes / (format / 8))
	{
		return false;
	}
	size_t length = (size_t)items * (format / 8);
	if (length < limits.min_bytes)
	{
		return false;
	}
	*bytes = length;
	return true;
}

FPropertyResult
FPropertyRead(Display *display, Window window, Atom property, Atom type,
    FPropertyLimits limits, unsigned char **data, size_t *bytes)
{
	unsigned char *reply = NULL;
	Atom actual_type = None;
	int actual_format = 0;
	unsigned long items = 0, bytes_after = 0;
	FPropertyResult result = FPROPERTY_INVALID;

	*data = NULL;
	*bytes = 0;
	if (limits.max_bytes > INT_MAX || limits.min_bytes > limits.max_bytes ||
	    (limits.format != 8 && limits.format != 16 && limits.format != 32))
	{
		return result;
	}
	/* Xlib's request length is in 32-bit units; round up without overflow. */
	long units = (long)(limits.max_bytes / 4 +
	    (limits.max_bytes % 4 != 0));
	int status = XGetWindowProperty(display, window, property, 0, units,
	    False, type, &actual_type, &actual_format, &items, &bytes_after, &reply);
	if (status != Success)
	{
		result = FPROPERTY_X_ERROR;
		goto out;
	}
	if (actual_type == None)
	{
		result = FPROPERTY_MISSING;
		goto out;
	}
	if ((type != AnyPropertyType && actual_type != type) ||
	    actual_format != limits.format)
	{
		goto out;
	}
	if (bytes_after != 0)
	{
		result = FPROPERTY_TOO_LARGE;
		goto out;
	}
	if (reply == NULL ||
	    !FPropertySizeIsSafe(items, actual_format, 0, limits, bytes))
	{
		goto out;
	}
	*data = fxmalloc(*bytes + 1);
	if (actual_format == 32)
	{
		/* Xlib expands format-32 items to native longs on LP64. */
		const unsigned long *values = (const unsigned long *)reply;
		for (size_t i = 0; i < items; i++)
		{
			((uint32_t *)*data)[i] = (uint32_t)values[i];
		}
	}
	else
	{
		memcpy(*data, reply, *bytes);
	}
	(*data)[*bytes] = '\0';
	result = FPROPERTY_OK;
out:
	if (reply != NULL)
	{
		XFree(reply);
	}
	return result;
}

Status
FGetTextProperty(Display *display, Window window, XTextProperty *text,
    Atom property)
{
	unsigned char *reply = NULL;
	Atom type = None;
	int format = 0;
	unsigned long items = 0, after = 0;
	size_t bytes;
	FPropertyLimits limits = {0, FPROPERTY_TEXT_MAX_BYTES, 8};

	memset(text, 0, sizeof(*text));
	int status = XGetWindowProperty(display, window, property, 0,
	    FPROPERTY_TEXT_MAX_BYTES / 4, False, AnyPropertyType,
	    &type, &format, &items, &after, &reply);
	if (status == Success && type != None && reply != NULL &&
	    FPropertySizeIsSafe(items, format, after, limits, &bytes))
	{
		text->value = reply;
		text->encoding = type;
		text->format = format;
		text->nitems = items;
		return True;
	}
	if (reply != NULL)
	{
		XFree(reply);
	}
	if (type != None)
	{
		fvwm_debug(__func__, "Ignoring invalid text property %lu", property);
	}
	return False;
}

Status
FGetWMName(Display *display, Window window, XTextProperty *text)
{
	return FGetTextProperty(display, window, text, XA_WM_NAME);
}

Status
FGetWMIconName(Display *display, Window window, XTextProperty *text)
{
	return FGetTextProperty(display, window, text, XA_WM_ICON_NAME);
}

Status
FGetClassHint(Display *display, Window window, XClassHint *hint)
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	FPropertyLimits limits = {1, FPROPERTY_TEXT_MAX_BYTES, 8};

	hint->res_name = NULL;
	hint->res_class = NULL;
	FPropertyResult result = FPropertyRead(display, window, XA_WM_CLASS,
	    XA_STRING, limits, &data, &bytes);
	if (result == FPROPERTY_OK)
	{
		/* The instance must end inside the reply. Like XGetClassHint,
		 * accept a class terminated by Xlib's extra trailing zero. */
		unsigned char *separator = memchr(data, '\0', bytes);
		if (separator != NULL)
		{
			hint->res_name = fxstrdup((char *)data);
			hint->res_class = fxstrdup((char *)separator + 1);
			free(data);
			return True;
		}
	}
	free(data);
	if (result != FPROPERTY_MISSING)
	{
		fvwm_debug(__func__, "Ignoring invalid WM_CLASS property");
	}
	return False;
}

Status
FGetCommand(Display *display, Window window, char ***argv, int *argc)
{
	XTextProperty text;
	Status status = False;

	*argv = NULL;
	*argc = 0;
	if (!FGetTextProperty(display, window, &text, XA_WM_COMMAND))
	{
		return False;
	}
	if (text.encoding == XA_STRING)
	{
		/* XGetCommand ignores exactly one final property terminator;
		 * empty arguments within the list remain significant. */
		if (text.nitems != 0 && text.value[text.nitems - 1] == '\0')
		{
			text.nitems--;
		}
		status = XTextPropertyToStringList(&text, argv, argc);
	}
	else
	{
		fvwm_debug(__func__, "Ignoring invalid WM_COMMAND encoding");
	}
	XFree(text.value);
	return status;
}

char *
FGetSessionString(Display *display, Window window, Atom property)
{
	XTextProperty text;

	if (!FGetTextProperty(display, window, &text, property))
	{
		return NULL;
	}
	if (text.encoding == XA_STRING && text.nitems != 0)
	{
		return (char *)text.value;
	}
	fvwm_debug(__func__, "Ignoring invalid session property %lu", property);
	XFree(text.value);
	return NULL;
}

Window
FGetClientLeader(Display *display, Window window, Atom property)
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	FPropertyLimits limits = {4, 4, 32};
	Window leader = None;
	FPropertyResult result = FPropertyRead(display, window, property,
	    XA_WINDOW, limits, &data, &bytes);

	if (result == FPROPERTY_OK)
	{
		leader = ((uint32_t *)data)[0];
	}
	else if (result != FPROPERTY_MISSING)
	{
		fvwm_debug(__func__, "Ignoring invalid client leader property");
	}
	free(data);
	return leader;
}

Status
FGetWMColormapWindows(Display *display, Window window, Window **windows,
    int *count)
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	FPropertyLimits limits = {0, 16U * 1024U, 32};
	Atom property = XInternAtom(display, "WM_COLORMAP_WINDOWS", False);
	FPropertyResult result = FPropertyRead(display, window, property,
	    XA_WINDOW, limits, &data, &bytes);

	*windows = NULL;
	*count = 0;
	if (result == FPROPERTY_OK)
	{
		*count = bytes / sizeof(uint32_t);
		*windows = fxmalloc((*count + 1) * sizeof(Window));
		for (int i = 0; i < *count; i++)
		{
			(*windows)[i] = ((uint32_t *)data)[i];
		}
	}
	else if (result != FPROPERTY_MISSING)
	{
		fvwm_debug(__func__, "Ignoring invalid colormap-window property");
	}
	free(data);
	return result == FPROPERTY_OK;
}
