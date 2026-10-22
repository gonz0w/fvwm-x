/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "buttons_updates.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>

static const char *const option_names[] =
{
	"Title", "Icon", "ActiveTitle", "ActiveIcon", "PressTitle", "PressIcon",
	"Colorset", "Enabled", "Selected", "ActiveColorset", "PressColorset",
	"FocusColorset", "DisabledColorset"
};

typedef struct
{
	const char *read;
	char *write;
} update_parser;

static void skip_space(update_parser *parser)
{
	while (isspace((unsigned char)*parser->read))
	{
		parser->read++;
	}
}

static bool delimiter(char c)
{
	return !c || isspace((unsigned char)c) || c == ',' ||
		c == '(' || c == ')';
}

/* The bounded input can only shrink: escapes and quotes are removed, and
 * each token's terminator replaces a separator or the input terminator. */
static char *read_token(update_parser *parser)
{
	char *start = parser->write;
	char quote = 0;

	skip_space(parser);
	if (delimiter(*parser->read))
	{
		return NULL;
	}
	if (*parser->read == '\'' || *parser->read == '"')
	{
		quote = *parser->read++;
	}
	while (*parser->read)
	{
		char c = *parser->read++;

		if (quote && c == quote)
		{
			if (!delimiter(*parser->read))
			{
				return NULL;
			}
			*parser->write++ = '\0';
			return start;
		}
		if (!quote && delimiter(c))
		{
			parser->read--;
			break;
		}
		if (c == '\\')
		{
			if (!*parser->read)
			{
				return NULL;
			}
			c = *parser->read++;
		}
		else if (!quote && (c == '\'' || c == '"'))
		{
			return NULL;
		}
		*parser->write++ = c;
	}
	if (quote)
	{
		return NULL;
	}
	*parser->write++ = '\0';
	return start;
}

static bool same_option(const char *left, const char *right)
{
	while (*left && *right &&
		tolower((unsigned char)*left) == tolower((unsigned char)*right))
	{
		left++;
		right++;
	}
	return !*left && !*right;
}

static bool decimal(const char **text)
{
	unsigned int number = 0;
	const char *s = *text;

	if (*s < '0' || *s > '9')
	{
		return false;
	}
	do
	{
		unsigned int digit = (unsigned int)(*s++ - '0');

		if (number > ((unsigned int)INT_MAX - digit) / 10)
		{
			return false;
		}
		number = number * 10 + digit;
	} while (*s >= '0' && *s <= '9');
	*text = s;
	return true;
}

static bool valid_selector(const char *s)
{
	if (isalpha((unsigned char)*s))
	{
		return true;
	}
	if (*s == '+')
	{
		s++;
		return decimal(&s) && *s++ == '+' && decimal(&s) && !*s;
	}
	return decimal(&s) && !*s;
}

bool buttons_parse_updates(const char *line, buttons_update_plan *plan,
	const char **error)
{
	update_parser parser;
	size_t length;

	memset(plan, 0, sizeof(*plan));
	*error = "invalid group syntax";
	if (!line)
	{
		return false;
	}
	for (length = 0; length <= BUTTONS_UPDATE_BYTES && line[length];
		length++)
	{
		/* Bound the whole request before copying or decoding tokens. */
	}
	if (length > BUTTONS_UPDATE_BYTES)
	{
		*error = "request exceeds 8192 bytes";
		return false;
	}
	parser = (update_parser){line, plan->storage};
	skip_space(&parser);
	while (*parser.read)
	{
		buttons_update_group *group;
		unsigned int seen = 0;

		if (plan->group_count == BUTTONS_UPDATE_GROUPS)
		{
			*error = "request exceeds 32 buttons";
			return false;
		}
		if (*parser.read++ != '(')
		{
			return false;
		}
		group = &plan->groups[plan->group_count++];
		group->selector = read_token(&parser);
		if (!group->selector || !valid_selector(group->selector))
		{
			*error = "invalid button selector";
			return false;
		}
		group->first = plan->change_count;
		for (;;)
		{
			char *name = read_token(&parser), *value;
			buttons_update_option option;

			if (!name)
			{
				return false;
			}
			for (option = 0; option < BUTTONS_UPDATE_OPTIONS; option++)
			{
				if (same_option(name, option_names[option]))
				{
					break;
				}
			}
			if (option == BUTTONS_UPDATE_OPTIONS ||
				(seen & (1U << option)))
			{
				*error = "unknown or repeated button option";
				return false;
			}
			seen |= 1U << option;
			value = read_token(&parser);
			if (!value)
			{
				return false;
			}
			if ((option == BUTTONS_ENABLED ||
				option == BUTTONS_SELECTED) &&
				strcmp(value, "0") && strcmp(value, "1"))
			{
				*error = "state requires 0 or 1";
				return false;
			}
			if (plan->change_count == BUTTONS_UPDATE_CHANGES)
			{
				*error = "request exceeds 128 changes";
				return false;
			}
			plan->changes[plan->change_count++] =
				(buttons_update_change){option, value};
			group->count++;
			skip_space(&parser);
			if (*parser.read == ')')
			{
				parser.read++;
				break;
			}
			if (*parser.read++ != ',')
			{
				return false;
			}
		}
		skip_space(&parser);
	}
	if (!plan->group_count)
	{
		return false;
	}
	*error = NULL;
	return true;
}
