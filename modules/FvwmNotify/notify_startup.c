/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "notify_startup.h"
#include "libs/module_startup.h"

bool notify_startup_begin(unsigned int seconds)
{
	return module_startup_begin(seconds);
}

void notify_startup_finish(void)
{
	module_startup_finish();
}
