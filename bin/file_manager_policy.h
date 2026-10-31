#ifndef FVWM_FILE_MANAGER_POLICY_H
#define FVWM_FILE_MANAGER_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FILE_MANAGER_URI_COUNT 8U
#define FILE_MANAGER_URI_BYTES 16384U
#define FILE_MANAGER_STARTUP_BYTES 512U
#define FILE_MANAGER_FRAME_BYTES 65536U
#define FILE_MANAGER_REPLY_BYTES 512U

enum file_manager_reply_status {
	FILE_MANAGER_ACCEPTED,
	FILE_MANAGER_INVALID,
	FILE_MANAGER_REFUSED,
	FILE_MANAGER_UNKNOWN
};

struct file_manager_reply {
	uint64_t id;
	enum file_manager_reply_status status;
	char detail[FILE_MANAGER_REPLY_BYTES + 1];
};

/* Validate UTF-8 envelope text, including its byte bound and no controls. */
bool file_manager_text_valid(const char *text, size_t maximum);
/* Exactly four primitive fields; no unknown/duplicate keys or NUL escapes.
 * The complete JSON frame, excluding its newline, is at most 512 bytes. */
bool file_manager_reply_parse(const char *line, size_t length,
	struct file_manager_reply *reply);

#endif
