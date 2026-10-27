/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	const unsigned long events[] = {
		MX_VISIBLE_ICON_NAME, MX_ENTER_WINDOW, MX_LEAVE_WINDOW,
		MX_PROPERTY_CHANGE, MX_MONITOR_ENABLED, MX_MONITOR_DISABLED,
		MX_MONITOR_CHANGED, MX_MONITOR_FOCUS, MX_ECHO, MX_REPLY,
		MX_CONFIG_SOURCE
	};
	unsigned long packet[16];
	unsigned long mask = 0;
	size_t i;

	for (i = 0; i < sizeof(events) / sizeof(events[0]); i++)
	{
		memset(packet, 0, sizeof(packet));
		packet[0] = START_FLAG;
		packet[1] = events[i];
		packet[2] = FvwmPacketHeaderSize + 3;
		packet[3] = 1234;
		packet[4] = 11;
		packet[5] = 22;
		packet[6] = 33;
		if (events[i] == MX_CONFIG_SOURCE)
			packet[5] = packet[6] = 0;
		if (events[i] != MX_ENTER_WINDOW &&
		    events[i] != MX_LEAVE_WINDOW)
		{
			memcpy(packet + 7, "native text", sizeof("native text"));
			packet[2] += (sizeof("native text") +
				sizeof(unsigned long) - 1) / sizeof(unsigned long);
		}
		mask |= events[i] & ~M_EXTENDED_MSG;
		if (fwrite(packet, sizeof(unsigned long), packet[2], stdout) !=
		    packet[2])
		{
			return 1;
		}
	}
	return mask != MAX_XMSG_MASK || fflush(stdout) != 0;
}
