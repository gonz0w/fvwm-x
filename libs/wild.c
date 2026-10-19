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

#include "config.h"

#include <stddef.h>
#include <string.h>

#include "wild.h"

/* Keep the original NULL and exhaustion rules. In particular, "**" does
 * not match empty text, and a final escape cannot consume a terminator. */
int matchWildcards(const char *pattern, const char *string)
{
	const char *retry_pattern = NULL, *retry_string = NULL;

	if (string == NULL)
		return pattern == NULL || strcmp(pattern, "*") == 0;
	if (pattern == NULL)
		return 1;

	/* A leading star followed by a nonempty literal must end at the
	 * subject's end. No escaping or empty-tail rule is involved here. */
	if (*pattern == '*' && pattern[1])
	{
		size_t width = strcspn(pattern + 1, "*?\\");

		if (!pattern[width + 1])
		{
			size_t length = strlen(string);

			return length >= width &&
				strcmp(string + length - width,
					pattern + 1) == 0;
		}
	}

	while (*string)
	{
		if (*pattern == '*')
		{
			pattern++;
			if (!*pattern)
				return 1;
			retry_pattern = pattern;
			retry_string = string;
		}
		else
		{
			const char *literal = pattern;
			int question = *literal == '?';

			if (*literal == '\\')
			{
				literal++;
				question = 0;
			}
			if (*literal && (question || *literal == *string))
			{
				pattern = literal + 1;
				string++;
			}
			else if (retry_pattern)
			{
				/* The latest star owns the retry. Its text
				 * stays at or before the current non-NUL
				 * byte. Advancing can reach the terminator
				 * but cannot pass it. No recursive frame
				 * or heap state is retained. */
				pattern = retry_pattern;
				string = ++retry_string;
			}
			else
				return 0;
		}
	}
	return !*pattern || (*pattern == '*' && !pattern[1]);
}
