#ifndef FVWM_FILE_TRANSFER_POLICY_H
#define FVWM_FILE_TRANSFER_POLICY_H
#include "libs/file_transfer_protocol.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum ft_command_kind
{
	FT_EXPECT, FT_CONTEXT, FT_OFFER, FT_READ, FT_DRAG_DATA,
	FT_ADMIT, FT_COMPLETE, FT_CLEAR, FT_CANCEL, FT_SHUTDOWN
};
struct ft_command
{
	enum ft_command_kind kind;
	uint64_t id, epoch, transfer, lease;
	uint32_t generation, time;
	bool enabled, success, has_time, move;
	char uris[FILE_TRANSFER_MAX_URI_BYTES + 1];
};
bool ft_decimal(const char *text, uint64_t *value);
bool ft_command_parse(const char *line, size_t length,
	struct ft_command *command);
/* File-only lexical validation; never observes the filesystem or decodes paths
 * for use. Go performs the single decode through filemodel.ParseTarget. */
bool ft_uris_valid(const char *text, const char *hostname);
bool ft_time_valid(uint32_t time, uint32_t since);
#endif
