/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include "FvwmButtons.h"
#include "buttons_navigation.h"
#include "draw.h"
#include "keyboard.h"

static Bool navigation_enabled;
static Bool window_focused;
static button_info *focus_button;
static button_info *pressed_button;
static KeyCode pressed_key;
static Bool activated_on_press;

/* Button records live for the module lifetime. Dynamic changes update their
 * state in place; no saved action string or pending pointer is freed here. */
static Bool eligible(const button_info *b)
{
	return b && !b->flags.b_Container && !b->flags.b_Disabled &&
		!b->flags.b_Swallow && (b->flags.b_Panel ||
		 (b->flags.b_Action && b->action &&
		  (b->action[0] || b->action[1])));
}

Bool buttons_keyboard_enabled(void)
{
	return navigation_enabled;
}

void buttons_set_keyboard_navigation(Bool enabled)
{
	navigation_enabled = enabled;
	if (enabled)
	{
		Bool supported = False;

		/* This affects only this X connection, never the global repeat
		 * settings or another application. Keep the traditional fallback. */
		XkbSetDetectableAutoRepeat(Dpy, True, &supported);
	}
}

Bool buttons_is_focused(const button_info *b)
{
	return navigation_enabled && window_focused && b == focus_button;
}

Bool buttons_is_key_pressed(const button_info *b)
{
	return pressed_button == b;
}

static void clear_press(void)
{
	pressed_button = NULL;
	pressed_key = 0;
	activated_on_press = False;
}

static void cancel_press(void)
{
	button_info *old = pressed_button;

	clear_press();
	if (old)
	{
		RedrawButton(old, DRAW_FORCE, NULL);
	}
}

static button_info *tab_target(button_info *current, Bool reverse)
{
	button_info *ub = UberButton, *b;
	button_info *first = NULL, *last = NULL, *previous = NULL;
	Bool found = False;
	int i = -1;

	while (NextButton(&ub, &b, &i, 0))
	{
		if (!eligible(b))
		{
			continue;
		}
		if (!first)
		{
			first = b;
		}
		last = b;
		if (!reverse && found)
		{
			return b;
		}
		if (b == current)
		{
			if (reverse && previous)
			{
				return previous;
			}
			found = True;
		}
		previous = b;
	}
	return reverse ? last : first;
}

static void set_focus_button(button_info *b)
{
	button_info *old = focus_button;

	if (b == old)
	{
		return;
	}
	cancel_press();
	focus_button = b;
	if (old)
	{
		RedrawButton(old, DRAW_FORCE, NULL);
	}
	if (b)
	{
		RedrawButton(b, DRAW_FORCE, NULL);
	}
}

static buttons_rect button_rect(button_info *b)
{
	buttons_rect r = {buttonXPos(b, b->n), buttonYPos(b, b->n),
		buttonWidth(b), buttonHeight(b)};

	return r;
}

static button_info *arrow_target(buttons_direction direction)
{
	button_info *ub = UberButton, *b, *best = focus_button;
	buttons_direction_score score = {0};
	int i = -1;

	if (!focus_button)
	{
		return tab_target(NULL, False);
	}
	while (NextButton(&ub, &b, &i, 0))
	{
		buttons_direction_score candidate;

		if (!eligible(b) || b == focus_button)
		{
			continue;
		}
		candidate = buttons_score_direction(
			button_rect(focus_button), button_rect(b), direction);
		if (buttons_score_better(candidate, score))
		{
			score = candidate;
			best = b;
		}
	}
	return best;
}

void buttons_state_changed(button_info *b)
{
	if (b->flags.b_Disabled && CurrentButton == b)
	{
		CurrentButton = NULL;
	}
	if (b->flags.b_Disabled && ActiveButton == b)
	{
		ActiveButton = NULL;
	}
	if (!eligible(pressed_button))
	{
		cancel_press();
	}
	if (!eligible(focus_button))
	{
		set_focus_button(tab_target(focus_button, False));
	}
}

/* Called only after a complete group commits. Do not draw intermediate
 * focus/press state; the caller owns the following repaint boundary. */
button_info *buttons_group_state_changed(void)
{
	if (CurrentButton && CurrentButton->flags.b_Disabled)
	{
		CurrentButton = NULL;
	}
	if (ActiveButton && ActiveButton->flags.b_Disabled)
	{
		ActiveButton = NULL;
	}
	if (!eligible(pressed_button))
	{
		clear_press();
	}
	if (!eligible(focus_button))
	{
		focus_button = tab_target(focus_button, False);
		clear_press();
	}
	/* Only Enabled changes eligibility. Any cleared old focus/press is
	 * already among the caller's targets; the new focus may need drawing. */
	return focus_button;
}

static void activate(button_info *b)
{
	button_info *saved = CurrentButton;
	char *act;

	if (!eligible(b))
	{
		return;
	}
	/* Resolve the current action at dispatch, using the same command engine
	 * as pointer and PressButton activation. This is UI state, not authority. */
	CurrentButton = b;
	act = GetButtonAction(b, 1);
	if (!act)
	{
		act = GetButtonAction(b, 0);
	}
	ButtonPressProcess(b, &act);
	free(act);
	CurrentButton = saved;
}

Bool buttons_keyboard_event(XEvent *event)
{
	KeySym key;
	char buffer[10];

	if (!navigation_enabled)
	{
		return False;
	}
	if (event->type == FocusIn || event->type == FocusOut)
	{
		if (event->xfocus.window != MyWindow)
		{
			return True;
		}
		window_focused = event->type == FocusIn &&
			event->xfocus.detail != NotifyVirtual &&
			event->xfocus.detail != NotifyNonlinearVirtual;
		cancel_press();
		if (!eligible(focus_button))
		{
			focus_button = tab_target(NULL, False);
		}
		if (focus_button)
		{
			RedrawButton(focus_button, DRAW_FORCE, NULL);
		}
		return True;
	}
	if (event->type == ButtonPress)
	{
		cancel_press();
		if (event->xbutton.window == MyWindow)
		{
			button_info *b = select_button(UberButton,
				event->xbutton.x, event->xbutton.y);

			if (eligible(b))
			{
				set_focus_button(b);
			}
		}
		return False;
	}
	if (event->type != KeyPress && event->type != KeyRelease)
	{
		return False;
	}
	/* Navigation is confined to the module's own window. No grabs, focus
	 * requests or typing interception are made for any other client. */
	if (!window_focused || event->xkey.window != MyWindow)
	{
		return True;
	}
	if (event->type == KeyRelease)
	{
		if (event->xkey.keycode == pressed_key)
		{
			XEvent next;

			/* Traditional X autorepeat sends a release/press pair with
			 * identical keycode/time. Peek only already queued events;
			 * leave dispatch and its service budget to the main loop. */
			if (XPending(Dpy))
			{
				XPeekEvent(Dpy, &next);
				if (next.type == KeyPress &&
					next.xkey.keycode == event->xkey.keycode &&
					next.xkey.time == event->xkey.time)
				{
					return True;
				}
			}
			if (!activated_on_press)
			{
				activate(pressed_button);
			}
			cancel_press();
		}
		return True;
	}
	XLookupString(&event->xkey, buffer, sizeof(buffer), &key, NULL);
	if (event->xkey.state & (ControlMask | Mod1Mask | Mod3Mask |
		Mod4Mask | Mod5Mask))
	{
		return True;
	}
	if (key == XK_Tab || key == XK_ISO_Left_Tab)
	{
		set_focus_button(tab_target(focus_button,
			(event->xkey.state & ShiftMask) != 0));
	}
	else if (key == XK_Left || key == XK_Right ||
		key == XK_Up || key == XK_Down)
	{
		buttons_direction direction = key == XK_Left ? BUTTONS_LEFT :
			key == XK_Right ? BUTTONS_RIGHT :
			key == XK_Up ? BUTTONS_UP : BUTTONS_DOWN;

		set_focus_button(arrow_target(direction));
	}
	else if ((key == XK_Return || key == XK_KP_Enter ||
		key == XK_Linefeed || key == XK_space) && !pressed_key &&
		!CurrentButton && eligible(focus_button))
	{
		char *act = GetButtonAction(focus_button, 1);

		if (!act)
		{
			act = GetButtonAction(focus_button, 0);
		}
		pressed_button = focus_button;
		pressed_key = event->xkey.keycode;
		activated_on_press = focus_button->flags.b_ActionOnPress ||
			(act && strncasecmp(act, "Popup", 5) == 0);
		free(act);
		RedrawButton(pressed_button, DRAW_FORCE, NULL);
		if (activated_on_press)
		{
			activate(pressed_button);
		}
	}
	return True;
}
