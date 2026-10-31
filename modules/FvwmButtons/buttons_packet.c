/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "buttons_packet.h"
#include "libs/module_packet_reader.h"
#include "fvwm/fvwm.h"
#include "libs/vpacket.h"

bool buttons_packet_is_valid(unsigned long type, const unsigned long *body,
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
	case M_FOCUS_CHANGE:
		return body_words >= 2;
	case M_DESTROY_WINDOW:
	case M_NEW_DESK:
	case M_MAP:
		return body_words >= 1;
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
