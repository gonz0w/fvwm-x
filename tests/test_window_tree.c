/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/WinMagic.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

enum { CHAIN, CYCLE, SELF, ROOT_CHANGE, FAILED };
static unsigned mode, calls, owned, attribute_calls;
static unsigned length = 3;
static int delta_x = 2, delta_y = -3, border = 1;
static Bool fail_attributes;

Status __wrap_XQueryTree(Display *display, Window window, Window *root,
	Window *parent, Window **children, unsigned *count)
{
	(void)display;
	calls++;
	*children = malloc(sizeof(**children));
	assert(*children != NULL);
	owned++;
	*count = 1;
	*root = mode == ROOT_CHANGE && calls > 1 ? 9 : 1;
	*parent = window < length + 1 ? window + 1 : 1;
	if (mode == CYCLE)
		*parent = window == 2 ? 3 : 2;
	if (mode == SELF)
		*parent = window;
	return mode != FAILED;
}

Status __wrap_XGetWindowAttributes(Display *display, Window window,
	XWindowAttributes *attributes)
{
	(void)display;
	(void)window;
	attribute_calls++;
	*attributes = (XWindowAttributes) {
		.x = delta_x, .y = delta_y, .border_width = border
	};
	return !fail_attributes;
}

int __wrap_XFree(void *data)
{
	if (data != NULL)
	{
		assert(owned != 0);
		owned--;
		free(data);
	}
	return 0;
}

static void reset(unsigned selected, unsigned steps)
{
	assert(owned == 0);
	mode = selected;
	length = steps;
	calls = attribute_calls = 0;
	fail_attributes = False;
	delta_x = 2;
	delta_y = -3;
	border = 1;
}

static void ancestor_tests(void)
{
	reset(CHAIN, 3);
	assert(GetTopAncestorWindow(NULL, 2) == 4 && calls == 3);
	reset(CHAIN, 1);
	assert(GetTopAncestorWindow(NULL, 2) == None && calls == 1);
	assert(GetTopAncestorWindow(NULL, None) == None && calls == 1);
	reset(CHAIN, F_WINDOW_TREE_MAX_QUERIES);
	assert(GetTopAncestorWindow(NULL, 2) == length + 1);
	assert(calls == F_WINDOW_TREE_MAX_QUERIES);
	reset(CHAIN, F_WINDOW_TREE_MAX_QUERIES + 1);
	assert(GetTopAncestorWindow(NULL, 2) == None);
	assert(calls == F_WINDOW_TREE_MAX_QUERIES);
	for (unsigned selected = CYCLE; selected <= FAILED; selected++)
	{
		reset(selected, 3);
		assert(GetTopAncestorWindow(NULL, 2) == None);
		assert(calls == (selected == CYCLE ?
			F_WINDOW_TREE_MAX_QUERIES : selected == ROOT_CHANGE ? 2 : 1));
		assert(owned == 0);
	}
}

static void parent_tests(void)
{
	FWindowTreeWalk walk = {0};
	Window parent = 99;
	reset(SELF, 3);
	assert(!FQueryWindowParent(NULL, 2, &walk, &parent));
	assert(parent == None && walk.queries == 1 && walk.root == None);
	walk.queries = F_WINDOW_TREE_MAX_QUERIES;
	parent = 99;
	assert(!FQueryWindowParent(NULL, 2, &walk, &parent));
	assert(parent == None && calls == 1 && owned == 0);
}

static void frame_tests(void)
{
	int x = 10, y = 20;
	reset(CHAIN, 3);
	assert(FGetFrameWindow(NULL, 2, &x, &y) == 4);
	assert(x == 16 && y == 16 && attribute_calls == 2);
	reset(CHAIN, 1);
	x = INT_MAX;
	y = INT_MIN;
	assert(FGetFrameWindow(NULL, 2, &x, &y) == 2);
	assert(x == INT_MAX && y == INT_MIN && attribute_calls == 0);
	for (unsigned selected = CYCLE; selected <= FAILED; selected++)
	{
		reset(selected, 3);
		x = 10;
		y = 20;
		assert(FGetFrameWindow(NULL, 2, &x, &y) == None);
		assert(x == 10 && y == 20 && owned == 0);
	}
	reset(CHAIN, 3);
	fail_attributes = True;
	assert(FGetFrameWindow(NULL, 2, &x, &y) == None);
	assert(x == 10 && y == 20);
	for (unsigned axis = 0; axis < 2; axis++)
	{
		reset(CHAIN, 2);
		x = axis == 0 ? INT_MAX : 0;
		y = axis == 1 ? INT_MIN : 0;
		int original_x = x, original_y = y;
		assert(FGetFrameWindow(NULL, 2, &x, &y) == None);
		assert(x == original_x && y == original_y);
	}
	reset(CHAIN, 2);
	delta_x = INT_MAX;
	border = INT_MAX;
	x = y = 0;
	assert(FGetFrameWindow(NULL, 2, &x, &y) == None);
	assert(x == 0 && y == 0 && owned == 0);
}

int main(void)
{
	ancestor_tests();
	parent_tests();
	frame_tests();
	assert(owned == 0);
	puts("window tree boundaries: passed");
	return 0;
}
