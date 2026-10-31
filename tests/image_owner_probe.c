/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Module.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* This optional mode is a fresh core dump channel, separate from the
 * continuous publication observer. Only one request can own a dump interval.
 * The startup END acknowledges mask installation before READY is published. */
static Bool snapshot_request(const char *text, size_t length,
	unsigned long *serial)
{
	*serial = 0;
	if (length < 10 || length > 29 ||
	    memcmp(text, "snapshot ", 9) != 0 || text[9] == '0')
	{
		return False;
	}
	for (size_t i = 9; i < length; i++)
	{
		unsigned digit = (unsigned char)text[i] - '0';

		if (digit > 9 || *serial > (ULONG_MAX - digit) / 10)
		{
			return False;
		}
		*serial = *serial * 10 + digit;
	}
	return True;
}

static Bool snapshot_colorset_id(const char *text, size_t length,
	unsigned *identifier)
{
	size_t i = sizeof("Colorset ") - 1;

	*identifier = 0;
	if (length <= i || text[i] == ' ')
		return False;
	for (; i < length && text[i] != ' '; i++)
	{
		unsigned digit;

		if (text[i] >= '0' && text[i] <= '9')
			digit = text[i] - '0';
		else if (text[i] >= 'a' && text[i] <= 'f')
			digit = text[i] - 'a' + 10;
		else
			return False;
		if (*identifier > (UINT_MAX - digit) / 16)
			return False;
		*identifier = *identifier * 16 + digit;
	}
	return i < length;
}

static Bool snapshot_write(FILE *report, const char *text, size_t length,
	size_t *written)
{
	const size_t limit = 16 * 1024 * 1024;

	if (length > limit - *written ||
	    fwrite(text, 1, length, report) != length)
	{
		return False;
	}
	*written += length;
	return True;
}

static int snapshot_colorsets(ModuleArgs *args)
{
	enum { BOOTSTRAP, IDLE, ACTIVE } state = BOOTSTRAP;
	FILE *report = fopen(args->user_argv[1], "w");
	int descriptors[2] = {args->to_fvwm, args->from_fvwm};
	unsigned long serial = 0;
	unsigned rows = 0, last_identifier = 0;
	Bool collecting = False;
	size_t written = 0;
	int result = 0;

	if (report == NULL)
	{
		return 3;
	}
	SetMessageMask(descriptors,
	    M_CONFIG_INFO | M_END_CONFIG_INFO | M_STRING);
	SendText(descriptors, "Send_ConfigInfo", None);
	for (;;)
	{
		FvwmPacket *packet = ReadFvwmPacket(descriptors[1]);

		if (packet == NULL)
		{
			if (state != IDLE)
				result = 10;
			break;
		}
		if (packet->type == M_STRING || packet->type == M_CONFIG_INFO)
		{
			size_t words = FvwmPacketBodySize(*packet);

			if (words <= 3)
			{
				result = 6;
				break;
			}
			const char *text = (const char *)&packet->body[3];
			size_t bytes = (words - 3) * sizeof(unsigned long);
			const char *end = bytes ?
			    memchr(text, '\0', bytes) : NULL;

			if (end == NULL)
			{
				result = 6;
				break;
			}
			if (packet->type == M_STRING)
			{
				if (state != IDLE ||
				    bytes > 29 + sizeof(unsigned long) ||
				    !snapshot_request(text, end - text,
				    &serial))
				{
					result = 8;
					break;
				}
				state = ACTIVE;
				rows = 0;
				collecting = False;
				SendText(descriptors, "Send_ConfigInfo", None);
			}
			else if (state == ACTIVE &&
			    strncmp(text, "Colorset ",
			    sizeof("Colorset ") - 1) == 0)
			{
				unsigned identifier;

				if (!snapshot_colorset_id(text, end - text,
				    &identifier))
				{
					result = 11;
					break;
				}
				if (identifier == 0)
				{
					char marker[64];
					int length = snprintf(marker,
					    sizeof(marker),
					    "BEGIN %lu\n", serial);

					/* Broadcasts are not a response start.
					 * Reset at each actual Colorset0; the
					 * core dump is ascending before END. */
					if (length <= 0 ||
					    (size_t)length >= sizeof(marker) ||
					    !snapshot_write(report, marker,
					    length, &written))
					{
						result = 7;
						break;
					}
					rows = 0;
					collecting = True;
				}
				if (!collecting || (rows > 0 &&
				    identifier <= last_identifier))
				{
					collecting = False;
					continue;
				}
				last_identifier = identifier;
				if (++rows > 4096 ||
				    !snapshot_write(report, text,
				    end - text, &written) ||
				    !snapshot_write(report, "\n", 1, &written))
				{
					result = 7;
					break;
				}
			}
		}
		else if (packet->type == M_END_CONFIG_INFO)
		{
			char marker[64];
			int length;

			if (state == BOOTSTRAP)
				length = snprintf(marker, sizeof(marker),
				    "READY\n");
			else if (state == ACTIVE && collecting && rows > 0)
				length = snprintf(marker, sizeof(marker),
				    "END %lu\n", serial);
			else
			{
				result = 9;
				break;
			}
			if (length <= 0 || (size_t)length >= sizeof(marker) ||
			    !snapshot_write(report, marker, length, &written) ||
			    fflush(report) != 0)
			{
				result = 7;
				break;
			}
			state = IDLE;
		}
	}
	if (fclose(report) != 0 && result == 0)
		result = 4;
	if (result != 0)
		fprintf(stderr,
		    "snapshot-colorsets protocol/report failure: %d\n",
		    result);
	return result;
}

/* Observe real mini-icon packets without querying X or inspecting core
 * memory. The harness owns the private report and this module's lifetime. */
int main(int argc, char **argv)
{
	ModuleArgs *args = ParseModuleArgs(argc, argv, 0);

	if (args != NULL && args->user_argc == 2 &&
	    strcmp(args->user_argv[0], "snapshot-colorsets") == 0)
	{
		return snapshot_colorsets(args);
	}
	if (args == NULL || (args->user_argc != 1 &&
	    (args->user_argc != 2 ||
	     strcmp(args->user_argv[0], "colorsets") != 0)))
	{
		return 2;
	}
	Bool colorsets = args->user_argc == 2;
	FILE *report = fopen(args->user_argv[colorsets ? 1 : 0], "w");

	if (report == NULL)
	{
		return 3;
	}
	int descriptors[2] = {args->to_fvwm, args->from_fvwm};

	SetMessageMask(descriptors, colorsets ? M_CONFIG_INFO : M_MINI_ICON);
	SendText(descriptors, colorsets ? "Send_ConfigInfo" : "Send_WindowList",
	    None);
	for (;;)
	{
		FvwmPacket *packet = ReadFvwmPacket(descriptors[1]);

		if (packet == NULL)
		{
			return fclose(report) == 0 ? 0 : 4;
		}
		if (packet->type == M_MINI_ICON)
		{
			if (FvwmPacketBodySize(*packet) < 9 ||
			    fprintf(report, "%lu %lu %lu %lu\n",
			    packet->body[0], packet->body[6],
			    packet->body[3], packet->body[4]) < 0 ||
			    fflush(report) != 0)
			{
				fclose(report);
				return 5;
			}
		}
		else if (colorsets && packet->type == M_CONFIG_INFO &&
		    FvwmPacketBodySize(*packet) > 3)
		{
			const char *text = (const char *)&packet->body[3];
			size_t bytes = (FvwmPacketBodySize(*packet) - 3) *
			    sizeof(unsigned long);

			if (memchr(text, '\0', bytes) == NULL)
			{
				fclose(report);
				return 6;
			}
			if (strncmp(text, "Colorset ", 8) == 0 &&
			    (fprintf(report, "%s\n", text) < 0 ||
			     fflush(report) != 0))
			{
				fclose(report);
				return 7;
			}
		}
	}
}
