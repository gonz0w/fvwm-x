/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "pager_packet.h"
#include "libs/Module.h"
#include "fvwm/fvwm.h"
#include "libs/vpacket.h"
#include "libs/module_packet_reader.h"

bool pager_packet_is_valid(unsigned long type, const unsigned long *body,
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
	case M_VISIBLE_NAME:
	case M_RES_NAME:
	case M_RES_CLASS:
	case MX_VISIBLE_ICON_NAME:
	case M_CONFIG_INFO:
	case M_STRING:
	case MX_REPLY:
		return module_packet_body_has_text(body, body_words, 3);
	case M_DESTROY_WINDOW:
	case M_RAISE_WINDOW:
	case M_LOWER_WINDOW:
	case M_DEICONIFY:
		return body_words >= 1;
	case M_NEW_DESK:
		return body_words >= 2;
	case MX_PROPERTY_CHANGE:
		return body_words >= 3;
	case M_FOCUS_CHANGE:
		return body_words >= 5;
	case M_ICONIFY:
	case M_ICON_LOCATION:
		return body_words >= 7;
	case M_NEW_PAGE:
		return body_words >= 8;
	case M_RESTACK:
		return body_words % 3 == 0;
	default:
		return true;
	}
}
