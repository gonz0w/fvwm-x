#ifndef FVWMLIB_MODULE_PACKET_READER_H
#define FVWMLIB_MODULE_PACKET_READER_H

#include "Module.h"
#include <stdbool.h>
#include <stddef.h>

#define MODULE_PACKET_READ_ATTEMPTS 64U

typedef struct
{
	unsigned long words[FvwmPacketMaxSize];
	size_t used;
	size_t wanted;
	int descriptor;
	bool failed;
} module_packet_reader;

typedef enum
{
	MODULE_PACKET_READ_MORE,
	MODULE_PACKET_READ_READY,
	MODULE_PACKET_READ_ENDED,
	MODULE_PACKET_READ_ERROR
} module_packet_read_status;

/* Caller owns the descriptor. Initialize after blocking startup consumers:
 * preserve descriptor flags and add O_NONBLOCK for runtime use. */
bool module_packet_reader_init(module_packet_reader *reader, int descriptor);
/* No read-ahead past one packet. A returned packet belongs to reader and stays
 * valid until its next read. MORE preserves progress without waiting. */
module_packet_read_status module_packet_reader_read(
	module_packet_reader *reader, FvwmPacket **packet);

/* Consumers must bound strings by the declared body, not reader capacity. */
bool module_packet_body_has_text(const unsigned long *body, size_t body_words,
	size_t text_offset);

#endif
