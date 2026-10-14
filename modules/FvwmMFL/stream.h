#ifndef FVWM_MFL_STREAM_H
#define FVWM_MFL_STREAM_H

#include "libs/defaults.h"
#include "libs/Module.h"
#include <event2/buffer.h>
#include <stdbool.h>

#define MFL_QUEUE_LIMIT (256U * 1024U)
#define MFL_CLIENT_LIMIT 32U
#define MFL_COMMAND_BUDGET 32U
#define MFL_PACKET_BUDGET 64U
#define MFL_INPUT_LIMIT (16U * 1024U)
#define MFL_PROGRESS_TIMEOUT 30

enum mfl_stream_result {
	MFL_STREAM_INVALID = -1,
	MFL_STREAM_INCOMPLETE = 0,
	MFL_STREAM_READY = 1
};

/* Incomplete frames remain in input. Invalid input is never forwarded. */
enum mfl_stream_result mfl_read_command(struct evbuffer *input,
    char command[MAX_MODULE_INPUT_TEXT_LEN + 1]);
enum mfl_stream_result mfl_read_packet(struct evbuffer *input,
    unsigned long packet[FvwmPacketMaxSize]);

/* Append the existing native module wire format atomically within the cap. */
bool mfl_queue_command(struct evbuffer *output, const char *command);
bool mfl_queue_line(struct evbuffer *output, const char *line);

#endif
