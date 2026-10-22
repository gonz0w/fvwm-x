/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "fvwm/ewmh_property_policy.h"
#include "fvwm/property_policy.h"
#include <assert.h>
#include <limits.h>

static void
test_fixed_properties_require_complete_wire_values(void)
{
	FPropertyLimits limits = fvwm_ewmh_property_limits("_NET_WM_STRUT", 32);
	size_t bytes = 99;

	assert(!FPropertySizeIsSafe(1, 32, 0, limits, &bytes) && bytes == 0);
	assert(!FPropertySizeIsSafe(3, 32, 0, limits, &bytes));
	assert(FPropertySizeIsSafe(4, 32, 0, limits, &bytes) && bytes == 16);
	assert(!FPropertySizeIsSafe(5, 32, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(4, 16, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(4, 32, 4, limits, &bytes));
	limits = fvwm_ewmh_property_limits("_NET_WM_PID", 32);
	assert(FPropertySizeIsSafe(1, 32, 0, limits, &bytes) && bytes == 4);
	assert(!FPropertySizeIsSafe(0, 32, 0, limits, &bytes));
	/* Legacy WM_STATE permits one through three complete native values. */
	limits = (FPropertyLimits){4, 12, 32};
	for (unsigned long fields = 1; fields <= 3; fields++)
	{
		assert(FPropertySizeIsSafe(fields, 32, 0, limits, &bytes));
	}
	assert(!FPropertySizeIsSafe(0, 32, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(4, 32, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(4, 8, 0, limits, &bytes));
}

static void
test_caps_and_arithmetic(void)
{
	size_t bytes;
	FPropertyLimits limits = fvwm_ewmh_property_limits("_NET_WM_NAME", 8);

	assert(FPropertySizeIsSafe(0, 8, 0, limits, &bytes) && bytes == 0);
	assert(FPropertySizeIsSafe(limits.max_bytes, 8, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(limits.max_bytes + 1, 8, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(ULONG_MAX, 8, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(1, 0, 0, limits, &bytes));
	limits = fvwm_ewmh_property_limits("_NET_WM_ICON", 32);
	assert(FPropertySizeIsSafe(4096UL * 4096 + 2, 32, 0, limits, &bytes));
	assert(!FPropertySizeIsSafe(ULONG_MAX, 32, 0, limits, &bytes));
	limits.max_bytes = (size_t)INT_MAX + 1;
	assert(!FPropertySizeIsSafe(1, 32, 0, limits, &bytes));
	limits.min_bytes = 2;
	limits.max_bytes = 1;
	assert(!FPropertySizeIsSafe(1, 8, 0, limits, &bytes));
}

int
main(void)
{
	assert(fvwm_openlook_attr_length_is_valid(3));
	assert(fvwm_openlook_attr_length_is_valid(5));
	assert(!fvwm_openlook_attr_length_is_valid(1));
	assert(!fvwm_openlook_attr_length_is_valid(4));
	assert(fvwm_selection_pair_length_is_valid(0));
	assert(fvwm_selection_pair_length_is_valid(8));
	assert(fvwm_selection_pair_length_is_valid(1024));
	assert(!fvwm_selection_pair_length_is_valid(4));
	assert(!fvwm_selection_pair_length_is_valid(1028));
	/* Equality, CurrentTime, before/after and wrap are independent of Xlib. */
	assert(fvwm_selection_request_time_is_valid(0, 100));
	assert(fvwm_selection_request_time_is_valid(100, 100));
	assert(fvwm_selection_request_time_is_valid(101, 100));
	assert(!fvwm_selection_request_time_is_valid(99, 100));
	assert(fvwm_selection_request_time_is_valid(5, UINT32_MAX - 4));
	assert(!fvwm_selection_request_time_is_valid(UINT32_MAX - 4, 5));
	assert(!fvwm_selection_request_time_is_valid(0x80000001U, 1));
	assert(fvwm_selection_request_time_is_valid(0x80000000U, 1));

	test_fixed_properties_require_complete_wire_values();
	test_caps_and_arithmetic();
	return 0;
}
