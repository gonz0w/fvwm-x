/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "iconman_packet.h"
#include "libs/module_packet_reader.h"
#include "fvwm/fvwm.h"
#include "libs/vpacket.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

bool iconman_parse_window_id(const char *text, unsigned long *result)
{
	char *end;
	unsigned long value;

	if (result == NULL)
	{
		return false;
	}
	*result = 0;
	if (text == NULL)
	{
		return false;
	}
	while (isspace((unsigned char)*text))
	{
		text++;
	}
	if (*text == '-')
	{
		return false;
	}
	errno = 0;
	value = strtoul(text, &end, 10);
	if (end == text || errno == ERANGE || value > UINT32_MAX)
	{
		return false;
	}
	*result = value;
	return true;
}

bool iconman_packet_is_valid(unsigned long type, const unsigned long *body,
	size_t body_words)
{
	if (body_words > FvwmPacketBodyMaxSize ||
		(body_words != 0 && body == NULL))
	{
		return false;
	}
	switch (type)
	{
	case M_ADD_WINDOW:
	case M_CONFIGURE_WINDOW:
		return body_words * sizeof(unsigned long) >= sizeof(ConfigWinPacket);
	case M_MINI_ICON:
		return body_words * sizeof(unsigned long) >=
			offsetof(MiniIconPacket, name);
	case M_FOCUS_CHANGE:
	case M_DESTROY_WINDOW:
	case M_WINDOWSHADE:
	case M_DEWINDOWSHADE:
	case M_NEW_DESK:
		return body_words >= 1;
	case M_NEW_PAGE:
		return body_words >= 3;
	case M_ICONIFY:
	case M_DEICONIFY:
	case M_ICON_LOCATION:
		return body_words >= 7;
	case M_ICON_NAME:
	case M_VISIBLE_NAME:
	case MX_VISIBLE_ICON_NAME:
	case M_RES_NAME:
	case M_RES_CLASS:
	case M_WINDOW_NAME:
	case M_CONFIG_INFO:
	case M_STRING:
		return module_packet_body_has_text(body, body_words, 3);
	case MX_PROPERTY_CHANGE:
		return body_words >= 3 &&
			(body[0] != MX_PROPERTY_CHANGE_SWALLOW || body[1] == 0 ||
			module_packet_body_has_text(body, body_words, 3));
	default:
		return true;
	}
}
