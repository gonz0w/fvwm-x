/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/WinMagic.h"

#include <X11/Xlib.h>
#include <assert.h>
#include <stdlib.h>

static Visual visual = { .visualid = 3 };
static Bool fail_attributes, fail_tree, fail_child, empty_tree;
static unsigned query_count, allocations;

Status __wrap_XGetWindowAttributes(Display *display, Window window,
	XWindowAttributes *attributes)
{
	if (fail_attributes || (fail_child && window == 11))
	{
		return 0;
	}
	*attributes = (XWindowAttributes) {
		.width = 100, .height = 100, .depth = 24, .visual = &visual,
		.colormap = 4, .class = InputOutput
	};
	if (window == 12)
	{
		attributes->x = 1;
	}
	if (window == 13)
	{
		attributes->class = InputOnly;
	}
	return 1;
}

Status __wrap_XQueryTree(Display *display, Window window, Window *root,
	Window *parent, Window **children, unsigned *count)
{
	query_count++;
	if (fail_tree)
	{
		return 0;
	}
	if (empty_tree)
	{
		*children = NULL;
		*count = 0;
		return 1;
	}
	*children = malloc(3 * sizeof(**children));
	assert(*children != NULL);
	(*children)[0] = 11;
	(*children)[1] = 12;
	(*children)[2] = 13;
	*count = 3;
	allocations++;
	return 1;
}

int __wrap_XFree(void *data)
{
	if (data != NULL)
	{
		assert(allocations > 0);
		allocations--;
		free(data);
	}
	return 0;
}

static void test_failed_outputs(void)
{
	Window *children = (Window *)&visual;
	fail_attributes = True;
	assert(GetEqualSizeChildren(NULL, 1, 24, 3, 4, &children) == -1);
	assert(children == NULL && allocations == 0 && query_count == 0);
	fail_attributes = False;
	fail_tree = True;
	children = (Window *)&visual;
	assert(GetEqualSizeChildren(NULL, 1, 24, 3, 4, &children) == -1);
	assert(children == NULL && allocations == 0);
	fail_tree = False;
}

static void test_parent_mismatch(void)
{
	int depths[] = { 8, 24, 24 };
	VisualID visuals[] = { 3, 9, 3 };
	Colormap colormaps[] = { 4, 4, 9 };
	for (unsigned i = 0; i < 3; i++)
	{
		Window *children = (Window *)&visual;
		assert(GetEqualSizeChildren(NULL, 1, depths[i], visuals[i],
			colormaps[i], &children) == -1);
		assert(children == NULL && allocations == 0);
	}
}

static void test_matching_children(void)
{
	Window *children = NULL;
	assert(GetEqualSizeChildren(NULL, 1, 24, 3, 4, &children) == 1);
	assert(children != NULL && children[0] == 11 && allocations == 1);
	XFree(children);
	assert(allocations == 0);
	/* Zero selectors retain their documented wildcard behavior. */
	assert(GetEqualSizeChildren(NULL, 1, 0, 0, 0, &children) == 1);
	assert(children[0] == 11 && allocations == 1);
	XFree(children);
	fail_child = True;
	assert(GetEqualSizeChildren(NULL, 1, 24, 3, 4, &children) == 0);
	assert(children == NULL && allocations == 0);
	fail_child = False;
	empty_tree = True;
	assert(GetEqualSizeChildren(NULL, 1, 24, 3, 4, &children) == 0);
	assert(children == NULL && allocations == 0);
	empty_tree = False;
}

int main(void)
{
	test_failed_outputs();
	test_parent_mismatch();
	test_matching_children();
	return 0;
}
