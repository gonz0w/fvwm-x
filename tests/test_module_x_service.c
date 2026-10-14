/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/module_x_service.h"
#include <assert.h>

static void test_x_service_limits(void)
{
	struct timespec started = {10, 999000000L};
	struct timespec now = {11, 3999999L};

	assert(module_x_service_available(1, &started, &now));
	now.tv_nsec++;
	assert(!module_x_service_available(1, &started, &now));
	assert(module_x_service_available(0, &started, &now));
	assert(!module_x_service_available(32, &started, &started));
	assert(module_x_service_available(31, &started, &started));
	started = (struct timespec){10, 0};
	now = (struct timespec){10, MODULE_X_SERVICE_NS - 1};
	assert(module_x_service_available(1, &started, &now));
	now.tv_nsec++;
	assert(!module_x_service_available(1, &started, &now));
	now = (struct timespec){12, 0};
	assert(!module_x_service_available(1, &started, &now));
	assert(module_x_service_available(0, &started, &now));
	now = (struct timespec){9, 0};
	assert(!module_x_service_available(1, &started, &now));
	now = (struct timespec){10, 1000000000L};
	assert(!module_x_service_available(1, &started, &now));
	started = (struct timespec){10, 5};
	now = (struct timespec){10, 4};
	assert(!module_x_service_available(0, &started, &now));
}

int main(void)
{
	test_x_service_limits();
	return 0;
}
