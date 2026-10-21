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

/*
** Strings.c: various routines for dealing with strings
*/
#include "config.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

#include "safemalloc.h"
#include "Strings.h"

/* Return the owned first-line length so quoted consumers need no rescan.
 * Keep indices within the string even when the trimmed result is empty. */
static size_t copy_string_line(char **dest, const char *source)
{
	size_t len;

	*dest = NULL;
	if (source == NULL)
	{
		return 0;
	}
	while (isspace((unsigned char)*source) && *source != '\n')
	{
		source++;
	}
	len = strcspn(source, "\n");
	while (len > 0 && isspace((unsigned char)source[len - 1]))
	{
		len--;
	}
	*dest = fxmalloc(len + 1);
	memcpy(*dest, source, len);
	(*dest)[len] = '\0';

	return len;
}

void CopyString(char **dest, const char *source)
{
	copy_string_line(dest, source);
}

void CopyStringWithQuotes(char **dest, const char *src)
{
	/* Only ASCII spaces admit the optional opening quote, as in existing
	 * font/configuration consumers. A preceding tab must retain quotes. */
	while (src && src[0] == ' ')
	{
		src++;
	}
	if (src && src[0] == '"')
	{
		size_t len = copy_string_line(dest, src + 1);

		if (len > 0 && (*dest)[len - 1] == '"')
		{
			(*dest)[len - 1] = '\0';
		}
	}
	else
	{
		copy_string_line(dest, src);
	}
}

/* Unlike CopyString, trim whitespace across the whole string, including
 * newlines. The caller owns a distinct allocation, also for an empty result. */
char *stripcpy(const char *source)
{
	char *result;
	size_t len;

	if (source == NULL)
	{
		return NULL;
	}
	while (isspace((unsigned char)*source))
	{
		source++;
	}
	len = strlen(source);
	while (len > 0 && isspace((unsigned char)source[len - 1]))
	{
		len--;
	}
	result = fxmalloc(len + 1);
	memcpy(result, source, len);
	result[len] = '\0';

	return result;
}

int StrEquals( const char *s1, const char *s2 )
{
	if (s1 == NULL && s2 == NULL)
	{
		return 1;
	}
	if (s1 == NULL || s2 == NULL)
	{
		return 0;
	}

	return strcasecmp(s1,s2) == 0;
}


int StrHasPrefix( const char* string, const char* prefix )
{
	if ( prefix == NULL )
	{
		return 1;
	}
	if ( string == NULL )
	{
		return 0;
	}

	return strncasecmp( string, prefix, strlen(prefix) ) == 0;
}


/*
 *
 * Adds single quotes arround the string and escapes single quotes with
 * backslashes.  The result is placed in the given dest, not allocated.
 * The end of destination, i.e. pointer to '\0' is returned.
 * You should allocate dest yourself, at least strlen(source) * 2 + 3.
 *
 */
char *QuoteString(char *dest, const char *source)
{
	int i = 0;
	*dest++ = '\'';
	for(i = 0; source[i]; i++)
	{
		if (source[i] == '\'')
		{
			*dest++ = '\\';
		}
		*dest++ = source[i];
	}
	*dest++ = '\'';
	*dest = '\0';

	return dest;
}

/*
 * Adds delim around the source and escapes all characters in escape with
 * the corresponding escaper. The dest string must be preallocated.
 * delim should be included in escape with a proper escaper.
 * Returns a pointer to the end of dest.
 */

char *QuoteEscapeString(char *dest, const char *source, char delim,
			const char *escape, const char *escaper)
{
	*dest++ = delim;
	while (*source)
	{
		char *esc;
		esc = strchr(escape, *source);
		if (esc != NULL)
		{
			*dest++ = escaper[(int)(esc-escape)];
		}
		*dest++ = *source++;
	}
	*dest++ = delim;
	*dest = '\0';

	return dest;
}

/*
 * Calculates the lenght needed by a escaped by QuoteEscapeString
 * the corresponding escaper.
 */

unsigned int QuoteEscapeStringLength(const char *source, const char *escape)
{
	unsigned int len = 2;

	while (*source)
	{
		if (strchr(escape, *source) != NULL)
		{
			len++;
		}
		len++;
		source++;
	}

	return len;
}
