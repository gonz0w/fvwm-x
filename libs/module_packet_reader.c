/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "module_packet_reader.h"
#include "fvwmsignal.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>

bool module_packet_body_has_text(const unsigned long *body, size_t body_words,
	size_t text_offset)
{
	return body != NULL && body_words <= FvwmPacketBodyMaxSize &&
		text_offset < body_words && memchr(body + text_offset, '\0',
			(body_words - text_offset) * sizeof(*body)) != NULL;
}

bool module_packet_reader_init(module_packet_reader *reader, int descriptor)
{
	int flags;

	*reader = (module_packet_reader){.descriptor = -1, .failed = true};
	flags = fcntl(descriptor, F_GETFL);
	if (flags < 0 ||
		(!(flags & O_NONBLOCK) &&
		fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0))
	{
		return false;
	}
	reader->descriptor = descriptor;
	reader->wanted = sizeof(unsigned long);
	reader->failed = false;
	return true;
}

module_packet_read_status module_packet_reader_read(
	module_packet_reader *reader, FvwmPacket **packet)
{
	unsigned int attempts = 0;

	*packet = NULL;
	if (reader->failed || reader->descriptor < 0)
	{
		errno = EINVAL;
		return MODULE_PACKET_READ_ERROR;
	}
	while (!isTerminated)
	{
		if (reader->used == reader->wanted)
		{
			if (reader->wanted == sizeof(unsigned long))
			{
				if (reader->words[0] != START_FLAG)
				{
					reader->failed = true;
					errno = EPROTO;
					return MODULE_PACKET_READ_ERROR;
				}
				else
				{
					reader->wanted =
						FvwmPacketHeaderSize_byte;
				}
			}
			else if (reader->wanted == FvwmPacketHeaderSize_byte)
			{
				unsigned long words = reader->words[2];

				if (words < FvwmPacketHeaderSize ||
					words > FvwmPacketMaxSize)
				{
					reader->failed = true;
					errno = EPROTO;
					return MODULE_PACKET_READ_ERROR;
				}
				reader->wanted = words * sizeof(unsigned long);
				if (reader->used < reader->wanted)
				{
					continue;
				}
			}
			if (reader->used == reader->wanted &&
				reader->wanted >= FvwmPacketHeaderSize_byte)
			{
				*packet = (FvwmPacket *)reader->words;
				reader->used = 0;
				reader->wanted = sizeof(unsigned long);
				return MODULE_PACKET_READ_READY;
			}
		}
		/* Complete before yielding: the last permitted read may finish
		 * a packet with no further fd readiness. */
		if (attempts++ >= MODULE_PACKET_READ_ATTEMPTS)
		{
			return MODULE_PACKET_READ_MORE;
		}
		ssize_t count = fvwmRead(reader->descriptor,
			(char *)reader->words + reader->used,
			reader->wanted - reader->used);
		if (count > 0)
		{
			reader->used += (size_t)count;
			continue;
		}
		if (count == 0)
		{
			reader->failed = true;
			if (reader->used != 0)
			{
				errno = EPROTO;
				return MODULE_PACKET_READ_ERROR;
			}
			errno = 0;
			return MODULE_PACKET_READ_ENDED;
		}
		if (isTerminated)
		{
			return MODULE_PACKET_READ_ENDED;
		}
		if (errno == EINTR)
		{
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return MODULE_PACKET_READ_MORE;
		}
		reader->failed = true;
		return MODULE_PACKET_READ_ERROR;
	}
	return MODULE_PACKET_READ_ENDED;
}
