/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

/* ---------------------------- included header files ----------------------- */

#include "config.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#include "libs/defaults.h"
#include "libs/fvwm_x11.h"
#include "libs/Module.h"
#include "libs/Strings.h"
#include "libs/Parse.h"
#include "FvwmButtons.h"
#include "button.h"
#include "draw.h"
#include "icons.h"
#include "keyboard.h"
#include "buttons_updates.h"

/* ---------------------------- local definitions --------------------------- */

/* ---------------------------- local macros -------------------------------- */

/* ---------------------------- imports ------------------------------------- */

extern int Width, Height;
extern int screen;

/* ---------------------------- included code files ------------------------- */

/* ---------------------------- local types --------------------------------- */

/* ---------------------------- forward declarations ------------------------ */

/* ---------------------------- local variables ----------------------------- */

static Bool silent = False;

/* ---------------------------- exported variables (globals) ---------------- */

/* ---------------------------- local functions ----------------------------- */

static void show_error(const char *msg, ...)
{
	va_list args;

	if (silent == True)
	{
		return;
	}
	va_start(args, msg);
	if (msg != NULL) {
		vfprintf(stderr, msg, args);
		fprintf(stderr, ": ");
	}
	va_end(args);
}



#if 0
/* to be used in module_expand_action */
static module_expand_vars mev =
{
	/* number of vars */
	4,
	/* array of var names */
	{ "title", "icon", }
	/* array of values, void *, will be filled later */
	{ NULL, NULL },
	/* array of booleans, 0 if string, 1 if int */
	{ 0, 0, },
};
#endif

static char *expand_button_vars(const button_info *b, const char *line)
{
	/* not fully implemented yet, should expand $title, $icon and so on */
	rectangle r;
	char *expanded_line;

	/* there should be a function evaluating fore/back from colorset */
	char *fore = (b->flags.b_Fore) ? b->fore : "black";
	char *back = (b->flags.b_Back) ? b->back : "gray";

	get_button_root_geometry(&r, b);
	expanded_line = module_expand_action(
		Dpy, screen, (char *)line, &r, fore, back);

	return expanded_line;
}

static button_info *find_button(const char *s)
{
	button_info *b = NULL, *ub = UberButton;
	int mask;
	int x, y;
	int count, i;

	if (!s)
	{
		show_error("No button id specified\n");
		return NULL;
	}

	if (*s == '+')
	{
		unsigned int JunkWidth;
		unsigned int JunkHeight;

		mask = XParseGeometry(s, &x, &y, &JunkWidth, &JunkHeight);
		if (!(mask & XValue) || (mask & XNegative) ||
		    !(mask & YValue) || (mask & YNegative))
		{
			show_error("Illegal button position '%s'\n", s);
			return NULL;
		}
		if (x < 0 || y < 0)
		{
			show_error("Button column/row must not be negative\n");
			return NULL;
		}
		/* Refuse outside-grid coordinates before the legacy cell-index
		 * multiplication, including values that still fit an int. */
		if (!ub->flags.b_Container || ub->c->num_columns <= 0 ||
			x >= ub->c->num_columns || y >= ub->c->num_rows ||
			y > (INT_MAX - x) / ub->c->num_columns)
		{
			show_error("Button position is outside the grid\n");
			return NULL;
		}
		b = get_xy_button(ub, y, x);
		if (b == NULL)
		{
			show_error(
				"Button at column %d row %d not found\n", x, y);
			return NULL;
		}
	}
	else if (isdigit((unsigned char)*s))
	{
		x = atoi(s);
		i = count = -1;
		/* find the button */
		while (NextButton(&ub, &b, &i, 0))
		{
			if (++count == x)
				break;
		}
		if (count != x || b == NULL)
		{
			show_error("Button number %d not found\n", x);
			return NULL;
		}
	}
	else if (isalpha((unsigned char)*s))
	{
		Bool found = False;
		i = -1;
		/* find the button */
		while (NextButton(&ub, &b, &i, 0))
		{
			if (b->flags.b_Id && StrEquals(b->id, s))
			{
				found = True;
				break;
			}
		}
		if (found == False)
		{
			show_error("Button id '%s' does not exist\n", s);
			return NULL;
		}
	}
	else
	{
		show_error("Invalid button id '%s' specified\n", s);
		return NULL;
	}

	return b;
}

static button_info *parse_button_id(char **line)
{
	char *rest;
	char *selector = PeekToken(*line, &rest);
	button_info *b = find_button(selector);

	*line = b ? rest : NULL;
	return b;
}

/* Prepared values belong to the request until commit transfers them to a
 * button. Pictures hold cache references even when another button shares one. */
typedef struct
{
	char *text;
	FvwmPicture *picture;
	int number;
} prepared_update;

static void commit_update(button_info *b, buttons_update_option option,
	prepared_update *value)
{
	char **text = NULL;
	FvwmPicture **picture = NULL;
	Bool owned = False;

	switch (option)
	{
	case BUTTONS_TITLE:
		text = &b->title;
		owned = b->flags.b_Title;
		b->flags.b_Title = 1;
		break;
	case BUTTONS_ACTIVE_TITLE:
		text = &b->activeTitle;
		owned = b->flags.b_ActiveTitle;
		b->flags.b_ActiveTitle = 1;
		break;
	case BUTTONS_PRESS_TITLE:
		text = &b->pressTitle;
		owned = b->flags.b_PressTitle;
		b->flags.b_PressTitle = 1;
		break;
	case BUTTONS_ICON:
		text = &b->icon_file;
		picture = &b->icon;
		owned = b->flags.b_Icon;
		b->flags.b_Icon = 1;
		break;
	case BUTTONS_ACTIVE_ICON:
		text = &b->active_icon_file;
		picture = &b->activeicon;
		owned = b->flags.b_ActiveIcon;
		b->flags.b_ActiveIcon = 1;
		break;
	case BUTTONS_PRESS_ICON:
		text = &b->press_icon_file;
		picture = &b->pressicon;
		owned = b->flags.b_PressIcon;
		b->flags.b_PressIcon = 1;
		break;
	case BUTTONS_COLORSET:
		b->colorset = value->number;
		b->flags.b_Colorset = 1;
		break;
	case BUTTONS_ENABLED:
		b->flags.b_Disabled = !value->number;
		break;
	case BUTTONS_SELECTED:
		b->flags.b_Selected = value->number;
		break;
	case BUTTONS_ACTIVE_COLORSET:
		b->activeColorset = value->number;
		b->flags.b_ActiveColorset = 1;
		break;
	case BUTTONS_PRESS_COLORSET:
		b->pressColorset = value->number;
		b->flags.b_PressColorset = 1;
		break;
	case BUTTONS_FOCUS_COLORSET:
		b->focusColorset = value->number;
		b->flags.b_FocusColorset = 1;
		break;
	case BUTTONS_DISABLED_COLORSET:
		b->disabledColorset = value->number;
		b->flags.b_DisabledColorset = 1;
		break;
	case BUTTONS_UPDATE_OPTIONS:
		break;
	}
	if (text)
	{
		/* Failed startup loads clear drawable flags, but the icon
		 * filename is still owned. Picture ownership follows its flag. */
		if (owned || picture)
		{
			free(*text);
		}
		if (owned && picture && *picture)
		{
			PDestroyFvwmPicture(Dpy, *picture);
		}
		*text = value->text;
		value->text = NULL;
		if (picture)
		{
			*picture = value->picture;
			value->picture = NULL;
		}
	}
}

static void change_buttons(const char *line)
{
	buttons_update_plan plan;
	prepared_update prepared[BUTTONS_UPDATE_CHANGES] = {{0}};
	button_info *targets[BUTTONS_UPDATE_GROUPS];
	const char *error;
	size_t g, j, k;
	button_info *focus;
	Bool layout_changed = False, reflowed;

	if (!buttons_parse_updates(line, &plan, &error))
	{
		show_error("ChangeButtons refused: %s\n", error);
		return;
	}
	/* Resolve every target and palette without allocating sparse colorsets.
	 * A later refusal must not consume capacity or alter button state. */
	for (g = 0; g < plan.group_count; g++)
	{
		buttons_update_group *group = &plan.groups[g];

		targets[g] = find_button(group->selector);
		if (!targets[g] || targets[g]->flags.b_Container)
		{
			error = "selector does not name a leaf button";
			goto done;
		}
		for (j = 0; j < g; j++)
		{
			if (targets[g] == targets[j])
			{
				error = "button appears more than once";
				goto done;
			}
		}
		for (j = group->first; j < group->first + group->count; j++)
		{
			buttons_update_change *change = &plan.changes[j];
			char *end;

			if (change->option == BUTTONS_ENABLED ||
				change->option == BUTTONS_SELECTED)
			{
				prepared[j].number = change->value[0] == '1';
			}
			else if (change->option >= BUTTONS_COLORSET)
			{
				if (!ParseColorsetId(change->value, &end,
					&prepared[j].number) || *end ||
					!FindColorset(prepared[j].number))
				{
					error = "colorset must already exist";
					goto done;
				}
			}
		}
	}
	/* Load icons with the group's final normal palette, independent of
	 * option order. No rendering, event dispatch or button mutation here. */
	for (g = 0; g < plan.group_count; g++)
	{
		buttons_update_group *group = &plan.groups[g];
		int colorset = targets[g]->colorset;

		for (j = group->first; j < group->first + group->count; j++)
		{
			if (plan.changes[j].option == BUTTONS_COLORSET)
			{
				colorset = prepared[j].number;
			}
		}
		for (j = group->first; j < group->first + group->count; j++)
		{
			buttons_update_change *change = &plan.changes[j];

			if (change->option >= BUTTONS_COLORSET)
			{
				continue;
			}
			prepared[j].text = fxstrdup(change->value);
			if ((change->option == BUTTONS_ICON ||
				change->option == BUTTONS_ACTIVE_ICON ||
				change->option == BUTTONS_PRESS_ICON) &&
				!LoadIconFile(change->value, &prepared[j].picture,
					colorset))
			{
				error = "cannot load icon";
				goto done;
			}
		}
	}
	/* All fallible work is complete. Commit without servicing another
	 * event, then repair focus against the final state and repaint once.
	 * This is logical atomicity, not a compositor/frame presentation API. */
	for (g = 0; g < plan.group_count; g++)
	{
		buttons_update_group *group = &plan.groups[g];

		for (j = group->first; j < group->first + group->count; j++)
		{
			commit_update(targets[g], plan.changes[j].option,
				&prepared[j]);
			if (plan.changes[j].option < BUTTONS_COLORSET)
				layout_changed = True;
		}
	}
	reflowed = layout_changed && buttons_layout_reflow(UberButton);
	focus = buttons_group_state_changed();
	if (reflowed)
		RedrawWindow();
	for (g = 0; !reflowed && g < plan.group_count; g++)
	{
		RedrawButton(targets[g], DRAW_FORCE, NULL);
		if (targets[g] == focus)
		{
			focus = NULL;
		}
	}
	if (focus && !reflowed)
	{
		RedrawButton(focus, DRAW_FORCE, NULL);
	}
	if (UberButton->c->flags.b_TransBack)
	{
		SetTransparentBackground(UberButton, Width, Height);
	}
	error = NULL;
done:
	for (k = 0; k < plan.change_count; k++)
	{
		free(prepared[k].text);
		if (prepared[k].picture)
		{
			PDestroyFvwmPicture(Dpy, prepared[k].picture);
		}
	}
	if (error)
	{
		show_error("ChangeButtons refused: %s\n", error);
	}
}

/* ---------------------------- interface functions ------------------------- */

static char *actions[] =
{
	"Silent", "ChangeButtons", "ChangeButton", "ExpandButtonVars",
	"PressButton", NULL
};

static char *button_options[] =
{
	"Title", "Icon", "ActiveTitle", "ActiveIcon", "PressTitle", "PressIcon",
	"Colorset", "Enabled", "Selected", "ActiveColorset", "PressColorset",
	"FocusColorset", "DisabledColorset", NULL
};

void parse_message_line(char *line)
{
	char *rest;
	int action = -1;
	button_info *b;
	char *act;
	char *buttonn;
	int mousebutton;
	Bool layout_changed = False;

	silent = False;
	do
	{
		action = GetTokenIndex(line, actions, -1, &rest);
		if (action == -1)
		{
			show_error("Message not understood: %s", line);
			return;
		}
		if (action == 0)
		{
			silent = True;
			break;
		}
	} while (action == 0);

	/* If silent was given (to surpress erros from parsing commands, then
	 * retokenise the rest of the line, so that token matching for the
	 * actual command is possible.
	 */
	if (silent == True) {
		line = PeekToken(line, &rest);
		action = GetTokenIndex(rest, actions, -1, &rest);
	}

	if (action == 1)
	{
		change_buttons(rest);
		return;
	}

	/* find out which button */
	b = parse_button_id(&rest);
	if (!b || !rest)
	{
		return;
	}

	switch (action)
	{
	case 2:
		/* ChangeButton */
		/* Grid geometry retains its initial dimensions. Content layout
		 * measures once after all successful changes in this request. */
		while (rest && rest[0] != '\0')
		{
			char *option_pair;
			int option, i;
			char *value0, *value;
			FvwmPicture *icon;

			/* parse option and value and give diagnostics */
			rest = GetQuotedString(
				rest, &option_pair, ",", NULL, NULL, NULL);
			while (isspace(*rest))
			{
				rest++;
			}
			if (!option_pair)
				continue;

			option = GetTokenIndex(
				option_pair, button_options, -1, &value0);
			if (option < 0)
			{
				show_error(
					"Unsupported button option line '%s'\n",
					option_pair);
				free(option_pair);
				continue;
			}

			GetNextToken(value0, &value);
			free(option_pair);

			if (value == NULL)
			{
				show_error(
					"No title/icon to change specified.\n");
				continue;
			}
			switch (option)
			{
			case 0:
				/* Title */
				if (b->flags.b_Title)
					free(b->title);
				b->flags.b_Title = 1;
				b->title = value;
				layout_changed = True;
				value = NULL;
				break;
			case 2:
				/* ActiveTitle */
				if (b->flags.b_ActiveTitle)
					free(b->activeTitle);
				b->flags.b_ActiveTitle = 1;
				b->activeTitle = value;
				layout_changed = True;
				value = NULL;
				break;
			case 4:
				/* PressTitle */
				if (b->flags.b_PressTitle)
					free(b->pressTitle);
				b->flags.b_PressTitle = 1;
				b->pressTitle = value;
				layout_changed = True;
				value = NULL;
				break;
			case 7: /* Enabled */
			case 8: /* Selected */
				if (strcmp(value, "0") != 0 && strcmp(value, "1") != 0)
				{
					show_error("State requires 0 or 1\n");
					break;
				}
				if (option == 7)
				{
					b->flags.b_Disabled = value[0] == '0';
				}
				else
				{
					b->flags.b_Selected = value[0] == '1';
				}
				buttons_state_changed(b);
				break;
			case 9: /* ActiveColorset */
			case 10: /* PressColorset */
			case 11: /* FocusColorset */
			case 12: /* DisabledColorset */
				if (!ParseColorsetId(value, NULL, &i) ||
					!AllocColorset(i))
				{
					show_error("Invalid state colorset\n");
					break;
				}
				if (option == 9)
				{
					b->activeColorset = i;
					b->flags.b_ActiveColorset = 1;
				}
				else if (option == 10)
				{
					b->pressColorset = i;
					b->flags.b_PressColorset = 1;
				}
				else if (option == 11)
				{
					b->focusColorset = i;
					b->flags.b_FocusColorset = 1;
				}
				else
				{
					b->disabledColorset = i;
					b->flags.b_DisabledColorset = 1;
				}
				break;
			case 6:
				/* Colorset */
				if (!ParseColorsetId(value, NULL, &i) ||
				    !AllocColorset(i))
				{
					break;
				}
				b->colorset = i;
				b->flags.b_Colorset = 1;
				break;
			default:
				if (
					LoadIconFile(
						value, &icon, b->colorset) == 0)
				{
					show_error(
						"Cannot load icon \"%s\"\n",
						value);
				}
				else
				{
				    switch (option)
				    {
					case 1: /* Icon */
						if (b->flags.b_Icon)
						{
							free(b->icon_file);
							PDestroyFvwmPicture(
								Dpy, b->icon);
						}
						b->flags.b_Icon = 1;
						b->icon_file = value;
						value = NULL;
						b->icon = icon;
						layout_changed = True;
						break;
					case 3: /* ActiveIcon */
						if (b->flags.b_ActiveIcon)
						{
							free(b->active_icon_file);
							PDestroyFvwmPicture(
								Dpy,
								b->activeicon);
						}
						b->flags.b_ActiveIcon = 1;
						b->active_icon_file = value;
						value = NULL;
						b->activeicon = icon;
						layout_changed = True;
						break;
					case 5: /* PressIcon */
						if (b->flags.b_PressIcon)
						{
							free(b->press_icon_file);
							PDestroyFvwmPicture(
								Dpy,
								b->pressicon);
						}
						b->flags.b_PressIcon = 1;
						b->press_icon_file = value;
						value = NULL;
						b->pressicon = icon;
						layout_changed = True;
						break;
					}
				}
				break;
			}

			if (value)
		    {
			    free(value);
		    }
		}

		if (layout_changed && buttons_layout_reflow(UberButton))
			RedrawWindow();
		else
			RedrawButton(b, DRAW_FORCE, NULL);
		if (UberButton->c->flags.b_TransBack)
		{
			SetTransparentBackground(UberButton, Width, Height);
		}
		break;

	case 3:
		/* ExpandButtonVars */
		line = expand_button_vars(b, rest);
		if (line)
		{
			SendText(fd, line, 0);
			free(line);
		}
		break;
	case 4:
		/* PressButton */
		rest = GetQuotedString(rest, &buttonn, "", NULL, NULL, NULL);
		if (buttonn)
		{
			mousebutton = atoi(buttonn);
			free(buttonn);
			if (
				mousebutton <= 0 ||
				mousebutton > NUMBER_OF_EXTENDED_MOUSE_BUTTONS)
			{
				mousebutton = 1;
			}
		}
		else
		{
			mousebutton = 1;
		}
		CurrentButton = b;
		act = GetButtonAction(b, mousebutton);
		ButtonPressProcess(b, &act);
		if (act)
		{
			free(act);
		}
		CurrentButton = NULL;

		break;
	}

	return;
}
