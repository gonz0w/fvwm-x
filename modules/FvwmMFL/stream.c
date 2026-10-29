/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "stream.h"
#include <string.h>

enum mfl_stream_result mfl_read_command(struct evbuffer *input,
    char command[MFL_CLIENT_LINE_MAX + 1])
{
	struct evbuffer_ptr end = evbuffer_search(input, "\n", 1, NULL);
	size_t length = evbuffer_get_length(input);

	command[0] = '\0';
	if (end.pos < 0)
	{
		return length > MFL_CLIENT_LINE_MAX ? MFL_STREAM_INVALID :
		    MFL_STREAM_INCOMPLETE;
	}
	if ((size_t)end.pos > MFL_CLIENT_LINE_MAX)
	{
		return MFL_STREAM_INVALID;
	}
	length = (size_t)end.pos;
	if (evbuffer_remove(input, command, length) != (int)length ||
	    memchr(command, '\0', length) != NULL)
	{
		command[0] = '\0';
		return MFL_STREAM_INVALID;
	}
	command[length] = '\0';
	evbuffer_drain(input, 1);
	return MFL_STREAM_READY;
}

enum mfl_stream_result mfl_read_packet(struct evbuffer *input,
    unsigned long packet[FvwmPacketMaxSize])
{
	unsigned long header[FvwmPacketHeaderSize];
	size_t length;

	if (evbuffer_get_length(input) < sizeof(header))
	{
		return MFL_STREAM_INCOMPLETE;
	}
	if (evbuffer_copyout(input, header, sizeof(header)) != sizeof(header) ||
	    header[0] != START_FLAG || header[2] < FvwmPacketHeaderSize ||
	    header[2] > FvwmPacketMaxSize)
	{
		return MFL_STREAM_INVALID;
	}
	length = header[2] * sizeof(unsigned long);
	if (evbuffer_get_length(input) < length)
	{
		return MFL_STREAM_INCOMPLETE;
	}
	return evbuffer_remove(input, packet, length) == (int)length ?
	    MFL_STREAM_READY : MFL_STREAM_INVALID;
}

static bool mfl_queue_has_room(struct evbuffer *output, size_t length)
{
	size_t queued = evbuffer_get_length(output);

	return queued <= MFL_QUEUE_LIMIT && length <= MFL_QUEUE_LIMIT - queued;
}

bool mfl_queue_command(struct evbuffer *output, const char *command)
{
	unsigned long window = 0, continuation = 1;
	unsigned long length = strlen(command);
	unsigned char frame[
	    MAX_MODULE_INPUT_TEXT_LEN + 3 * sizeof(unsigned long)];
	size_t frame_length = length + 3 * sizeof(unsigned long);

	if (length > MAX_MODULE_INPUT_TEXT_LEN ||
	    !mfl_queue_has_room(output, frame_length))
	{
		return false;
	}
	memcpy(frame, &window, sizeof(window));
	memcpy(frame + sizeof(window), &length, sizeof(length));
	memcpy(frame + 2 * sizeof(window), command, length);
	memcpy(frame + 2 * sizeof(window) + length, &continuation,
	    sizeof(continuation));
	return evbuffer_add(output, frame, frame_length) == 0;
}

bool mfl_queue_line(struct evbuffer *output, const char *line)
{
	size_t length = strlen(line);

	if (length >= MFL_QUEUE_LIMIT ||
	    !mfl_queue_has_room(output, length + 1))
	{
		return false;
	}
	return evbuffer_add(output, line, length) == 0 &&
	    evbuffer_add(output, "\n", 1) == 0;
}
