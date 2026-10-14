/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <errno.h>
#include <poll.h>

#include "interaction.h"
#include "libs/FEvent.h"
#include "libs/fvwmsignal.h"
#include "libs/log.h"
#include "externs.h"

static Bool target_event(Display *display, XEvent *event, XPointer context)
{
	return fvwm_interaction_target_event(
		(const fvwm_interaction *)context, event);
}

Bool fvwm_interaction_cancelled(fvwm_interaction *interaction)
{
	XEvent event;

	if (interaction->result != FVWM_INTERACTION_RUNNING)
	{
		return True;
	}
	if (isTerminated)
	{
		interaction->result = FVWM_INTERACTION_TERMINATED;
	}
	else if (FCheckPeekIfEvent(
		dpy, &event, target_event, (XPointer)interaction))
	{
		interaction->result = FVWM_INTERACTION_TARGET_GONE;
	}

	return interaction->result != FVWM_INTERACTION_RUNNING;
}

Bool fvwm_interaction_wait(
	fvwm_interaction *interaction, long mask, XEvent *event)
{
	struct pollfd descriptor = { ConnectionNumber(dpy), POLLIN, 0 };

	while (!fvwm_interaction_cancelled(interaction))
	{
		if (FCheckMaskEvent(dpy, mask, event))
		{
			return True;
		}
		if (interaction->service != NULL)
		{
			if (!fvwm_interaction_service(interaction, 100))
			{
				return False;
			}
			continue;
		}
		/* The finite wait closes the signal-before-poll race without
		 * installing another signal handler or reentering commands. */
		if (poll(&descriptor, 1, 100) < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			fvwm_debug(__func__, "X readiness wait failed: %d", errno);
			interaction->result = FVWM_INTERACTION_IO_ERROR;
			return False;
		}
		if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			fvwm_debug(__func__, "X descriptor lost during interaction");
			interaction->result = FVWM_INTERACTION_IO_ERROR;
			return False;
		}
	}

	return False;
}

Bool fvwm_interaction_service(fvwm_interaction *interaction, int timeout_ms)
{
	if (fvwm_interaction_cancelled(interaction))
	{
		return False;
	}
	if (interaction->service != NULL && !interaction->service(timeout_ms))
	{
		interaction->result = FVWM_INTERACTION_IO_ERROR;
		return False;
	}
	return !fvwm_interaction_cancelled(interaction);
}

void fvwm_interaction_coalesce(long mask, XEvent *event)
{
	unsigned int count;

	/* Never drain indefinitely under a continuous motion producer. Keep
	 * release/key barriers, including the final release coordinates. */
	for (count = 0; count < FVWM_INTERACTION_MOTION_BUDGET; count++)
	{
		if (!FCheckMaskEvent(dpy, mask, event) ||
		    event->type == ButtonPress || event->type == ButtonRelease ||
		    event->type == KeyPress)
		{
			break;
		}
	}
}
