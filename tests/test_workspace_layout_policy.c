/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"
#include "fvwm/workspace_layout_policy.h"
#include <assert.h>
#include <limits.h>

static void expect_cell(rectangle cell, rectangle expected)
{
	assert(cell.x == expected.x && cell.y == expected.y);
	assert(cell.width == expected.width && cell.height == expected.height);
}

static void test_columns(void)
{
	struct workspace_layout_plan plan;
	rectangle area = {-101, 30, 101, 60};

	assert(workspace_layout_plan(WORKSPACE_LAYOUT_COLUMNS, area, 3, &plan));
	assert(plan.count == 3);
	expect_cell(plan.cells[0], (rectangle){-101, 30, 34, 60});
	expect_cell(plan.cells[1], (rectangle){-67, 30, 34, 60});
	expect_cell(plan.cells[2], (rectangle){-33, 30, 33, 60});

	/* The smallest cells at the window cap still cover the area. */
	area = (rectangle){INT_MIN, INT_MAX - 1, WORKSPACE_LAYOUT_MAX, 1};
	assert(workspace_layout_plan(WORKSPACE_LAYOUT_COLUMNS, area,
		WORKSPACE_LAYOUT_MAX, &plan));
	assert(plan.count == WORKSPACE_LAYOUT_MAX);
	for (int i = 0; i < WORKSPACE_LAYOUT_MAX; i++)
		expect_cell(plan.cells[i],
			(rectangle){INT_MIN + i, INT_MAX - 1, 1, 1});
}

static void test_master_stack(void)
{
	struct workspace_layout_plan plan;
	rectangle area = {240, -70, 101, 7};

	assert(workspace_layout_plan(WORKSPACE_LAYOUT_MASTER_STACK, area,
		4, &plan));
	assert(plan.count == 4);
	expect_cell(plan.cells[0], (rectangle){240, -70, 51, 7});
	expect_cell(plan.cells[1], (rectangle){291, -70, 50, 3});
	expect_cell(plan.cells[2], (rectangle){291, -67, 50, 2});
	expect_cell(plan.cells[3], (rectangle){291, -65, 50, 2});

	/* A sole window gets the complete area even at the minimum size. */
	area = (rectangle){-20, -10, 1, 1};
	assert(workspace_layout_plan(WORKSPACE_LAYOUT_MASTER_STACK, area,
		1, &plan));
	assert(plan.count == 1);
	expect_cell(plan.cells[0], area);
}

static void test_refusals(void)
{
	static const struct {
		enum workspace_layout_kind kind;
		rectangle area;
		size_t count;
	} cases[] = {
		{WORKSPACE_LAYOUT_COLUMNS, {0, 0, 10, 10}, 0},
		{WORKSPACE_LAYOUT_COLUMNS, {0, 0, 100, 10},
			WORKSPACE_LAYOUT_MAX + 1},
		{WORKSPACE_LAYOUT_COLUMNS, {0, 0, 2, 10}, 3},
		{WORKSPACE_LAYOUT_COLUMNS, {0, 0, 0, 10}, 1},
		{WORKSPACE_LAYOUT_COLUMNS, {0, 0, 10, -1}, 1},
		{WORKSPACE_LAYOUT_COLUMNS, {INT_MAX, 0, 1, 10}, 1},
		{WORKSPACE_LAYOUT_COLUMNS, {0, INT_MAX - 1, 10, 2}, 1},
		{WORKSPACE_LAYOUT_MASTER_STACK, {0, 0, 1, 10}, 2},
		{WORKSPACE_LAYOUT_MASTER_STACK, {0, 0, 10, 1}, 3},
		{(enum workspace_layout_kind)99, {0, 0, 10, 10}, 1},
	};
	struct workspace_layout_plan plan;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		plan.count = 1;
		plan.cells[0] = (rectangle){10, 20, 30, 40};
		assert(!workspace_layout_plan(cases[i].kind, cases[i].area,
			cases[i].count, &plan));
		assert(plan.count == 0);
		expect_cell(plan.cells[0], (rectangle){0});
	}
}

int main(void)
{
	test_columns();
	test_master_stack();
	test_refusals();
	return 0;
}
