/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/module_packet_reader.h"
#include "libs/fvwmsignal.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void open_reader(module_packet_reader *reader, int descriptors[2])
{
	assert(pipe(descriptors) == 0);
	int flags = fcntl(descriptors[0], F_GETFL);
	assert(flags >= 0);
	assert(module_packet_reader_init(reader, descriptors[0]));
	assert(fcntl(descriptors[0], F_GETFL) == (flags | O_NONBLOCK));
}

static void put_bytes(int descriptor, const void *bytes, size_t length)
{
	assert(write(descriptor, bytes, length) == (ssize_t)length);
}

static void test_fragmented_packet(void)
{
	module_packet_reader reader;
	int descriptors[2];
	unsigned long words[] = {START_FLAG, M_STRING, 7, 123, 11, 22, 33};
	FvwmPacket *packet = (FvwmPacket *)1;

	open_reader(&reader, descriptors);
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_MORE);
	assert(packet == NULL && reader.used == 0);
	for (size_t i = 0; i < sizeof(words); i++)
	{
		put_bytes(descriptors[1], (char *)words + i, 1);
		module_packet_read_status status =
			module_packet_reader_read(&reader, &packet);
		assert(status == (i + 1 == sizeof(words) ?
			MODULE_PACKET_READ_READY : MODULE_PACKET_READ_MORE));
		if (status == MODULE_PACKET_READ_MORE)
		{
			assert(packet == NULL && reader.used == i + 1);
		}
	}
	assert(packet->size == 7 && packet->type == M_STRING);
	assert(memcmp(packet, words, sizeof(words)) == 0);
	assert(reader.used == 0 && reader.wanted == sizeof(unsigned long));
	close(descriptors[0]);
	close(descriptors[1]);
}

static void test_packet_limits_and_order(void)
{
	module_packet_reader reader;
	int descriptors[2];
	unsigned long words[FvwmPacketMaxSize];
	FvwmPacket *packet;

	open_reader(&reader, descriptors);
	for (size_t i = 0; i < FvwmPacketMaxSize; i++)
	{
		words[i] = i;
	}
	words[0] = START_FLAG;
	words[1] = M_STRING;
	words[2] = FvwmPacketMaxSize;
	for (size_t offset = 0; offset < sizeof(words);)
	{
		size_t bytes = sizeof(words) - offset;
		if (bytes > 128)
		{
			bytes = 128;
		}
		put_bytes(descriptors[1], (char *)words + offset, bytes);
		offset += bytes;
		assert(module_packet_reader_read(&reader, &packet) ==
			(offset == sizeof(words) ? MODULE_PACKET_READ_READY :
			MODULE_PACKET_READ_MORE));
	}
	assert(memcmp(packet, words, sizeof(words)) == 0);
	unsigned long pair[] = {START_FLAG, M_STRING, 4, 7,
		START_FLAG, M_ERROR, 4, 8};
	put_bytes(descriptors[1], pair, sizeof(pair));
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_READY);
	assert(packet->type == M_STRING && packet->size == 4 &&
		packet->timestamp == 7);
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_READY);
	assert(packet->type == M_ERROR && packet->timestamp == 8);
	close(descriptors[1]);
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ENDED);
	assert(packet == NULL);
	close(descriptors[0]);
}

static int interrupted_fd = -1;
static unsigned int interruptions;
ssize_t __real_read(int, void *, size_t);

ssize_t __wrap_read(int fd, void *buffer, size_t size)
{
	if (fd == interrupted_fd && interruptions != 0)
	{
		interruptions--;
		errno = EINTR;
		return -1;
	}
	return __real_read(fd, buffer, size);
}

static void test_sync_budget(void)
{
	module_packet_reader reader;
	int descriptors[2];
	unsigned long words[] = {START_FLAG, M_STRING, 5, 10, 42};
	FvwmPacket *packet;

	open_reader(&reader, descriptors);
	put_bytes(descriptors[1], words, sizeof(words));
	interrupted_fd = descriptors[0];
	interruptions = MODULE_PACKET_READ_ATTEMPTS;
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_MORE);
	assert(packet == NULL && reader.used == 0 && interruptions == 0);
	/* Header and body complete on the final permitted read. */
	interruptions = MODULE_PACKET_READ_ATTEMPTS - 3;
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_READY);
	assert(packet->timestamp == 10 && packet->body[0] == 42);
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_MORE);
	interrupted_fd = -1;
	close(descriptors[0]);
	close(descriptors[1]);
}

static void test_bad_start_is_terminal(void)
{
	module_packet_reader reader;
	int descriptors[2];
	unsigned long words[] = {0, START_FLAG, M_ERROR, 4, 9};
	FvwmPacket *packet;

	open_reader(&reader, descriptors);
	put_bytes(descriptors[1], words, sizeof(words));
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ERROR);
	assert(packet == NULL && errno == EPROTO && reader.failed);
	assert(reader.used == sizeof(unsigned long));
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ERROR);
	assert(packet == NULL && reader.used == sizeof(unsigned long));
	close(descriptors[0]);
	close(descriptors[1]);
}

static void test_invalid_counts(void)
{
	const unsigned long invalid[] =
		{0, 3, FvwmPacketMaxSize + 1, ULONG_MAX};

	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		module_packet_reader reader;
		int descriptors[2];
		FvwmPacket *packet;
		unsigned long words[] = {START_FLAG, M_STRING, invalid[i], 0};

		open_reader(&reader, descriptors);
		put_bytes(descriptors[1], words, sizeof(words));
		errno = 0;
		assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ERROR);
		assert(packet == NULL && errno == EPROTO && reader.failed);
		assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ERROR);
		close(descriptors[0]);
		close(descriptors[1]);
	}
}

static void test_eof_error_and_cancellation(void)
{
	module_packet_reader reader;
	int descriptors[2];
	FvwmPacket *packet;
	unsigned long words[] = {START_FLAG, M_STRING, 5, 0, 42};

	for (size_t bytes = 1; bytes < sizeof(words); bytes++)
	{
		open_reader(&reader, descriptors);
		put_bytes(descriptors[1], words, bytes);
		close(descriptors[1]);
		assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ERROR);
		assert(packet == NULL && errno == EPROTO && reader.failed);
		assert(module_packet_reader_read(&reader, &packet) ==
			MODULE_PACKET_READ_ERROR);
		close(descriptors[0]);
	}
	open_reader(&reader, descriptors);
	put_bytes(descriptors[1], words, sizeof(words));
	isTerminated = 1;
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ENDED);
	assert(packet == NULL && reader.used == 0);
	isTerminated = 0;
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_READY);
	assert(packet->body[0] == 42);
	close(descriptors[0]);
	errno = 0;
	assert(module_packet_reader_read(&reader, &packet) ==
		MODULE_PACKET_READ_ERROR);
	assert(packet == NULL && errno == EBADF);
	close(descriptors[1]);
	assert(!module_packet_reader_init(&reader, -1));
	assert(reader.descriptor == -1 && reader.failed);
}

static void test_declared_text(void)
{
	for (size_t words = 0; words <= FvwmPacketBodyMaxSize; words++)
	{
		unsigned long *body = words ? malloc(words * sizeof(*body)) : NULL;

		assert(words == 0 || body != NULL);
		if (body != NULL)
		{
			memset(body, 'x', words * sizeof(*body));
			((char *)body)[0] = '\0';
		}
		assert(!module_packet_body_has_text(body, words, 3));
		assert(!module_packet_body_has_text(body, words, (size_t)-1));
		if (words > 3)
		{
			((char *)body)[words * sizeof(*body) - 1] = '\0';
			assert(module_packet_body_has_text(body, words, 3));
		}
		free(body);
	}
	unsigned long body[4] = {0};
	assert(!module_packet_body_has_text(NULL, 4, 3));
	assert(!module_packet_body_has_text(body, FvwmPacketBodyMaxSize + 1, 3));
}

int main(void)
{
	test_declared_text();
	test_fragmented_packet();
	test_packet_limits_and_order();
	test_sync_budget();
	test_bad_start_is_terminal();
	test_invalid_counts();
	test_eof_error_and_cancellation();
	return 0;
}
