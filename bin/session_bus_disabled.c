/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <stdio.h>

int main(void)
{
	return puts("{\"version\":1,\"available\":false,\"state\":\"unavailable\","
		"\"bus_id\":\"\",\"server_owner\":\"\",\"capabilities\":[],"
		"\"notification_id\":0,\"detail\":\"optional_bus_support_disabled\"}")
		< 0;
}
