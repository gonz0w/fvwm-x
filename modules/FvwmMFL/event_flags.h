#ifndef FVWMMFL_EVENT_FLAGS_H
#define FVWMMFL_EVENT_FLAGS_H

#include "libs/Module.h"
#include <stdbool.h>

/* Extended packet types share low bits with ordinary events. Keep the
 * unsigned wire width and match extended types exactly. */
static inline bool
fvwmmfl_event_flag_matches(unsigned long packet, unsigned long event)
{
	if (packet & M_EXTENDED_MSG)
	{
		return packet == event;
	}
	return !(event & M_EXTENDED_MSG) && (packet & event) != 0;
}

/* The extension marker belongs to the mask, not to an individual event. */
static inline unsigned long
fvwmmfl_event_flag_remove(unsigned long mask, unsigned long event)
{
	return mask & ~(event & ~M_EXTENDED_MSG);
}

#endif
