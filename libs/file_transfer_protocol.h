#ifndef FVWM_FILE_TRANSFER_PROTOCOL_H
#define FVWM_FILE_TRANSFER_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FILE_TRANSFER_MAGIC 0x46565431u
#define FILE_TRANSFER_WIRE_BYTES 608
#define FILE_TRANSFER_SELECTION_BYTES 512
#define FILE_TRANSFER_MAX_ROWS 4096
#define FILE_TRANSFER_MAX_SELECTED 256
#define FILE_TRANSFER_MAX_URI_BYTES (256 * 1024)
#define FILE_TRANSFER_MAX_GROUPS 8
#define FILE_TRANSFER_FRAME_BYTES (512 * 1024)

enum file_transfer_message {
	FILE_TRANSFER_ATTACH = 1,
	FILE_TRANSFER_GENERATION,
	FILE_TRANSFER_BEGIN,
	FILE_TRANSFER_MOTION,
	FILE_TRANSFER_RELEASE,
	FILE_TRANSFER_CANCEL,
	FILE_TRANSFER_CLIENT_MESSAGE,
	FILE_TRANSFER_FEEDBACK,
	FILE_TRANSFER_DETACH,
	FILE_TRANSFER_ACTION
};

enum file_transfer_action {
	FILE_TRANSFER_ACTION_COPY = 1,
	FILE_TRANSFER_ACTION_CUT,
	FILE_TRANSFER_ACTION_PASTE
};

enum file_transfer_feedback {
	FILE_TRANSFER_PENDING = 0,
	FILE_TRANSFER_REJECTED,
	FILE_TRANSFER_COPY,
	FILE_TRANSFER_MOVE,
	FILE_TRANSFER_DONE,
	FILE_TRANSFER_IDLE
};

/* Private Form/helper metadata only: no paths, labels or clipboard payloads.
 * Codec uses 24 big-endian 32-bit words followed by 512 selection-bit bytes.
 * Words: magic, kind, epoch hi/lo, generation, frame/list, time, root x/y,
 * state, detail, five ClientMessage data words, format, byte count, row count,
 * then the 16-byte nonce. Unused fields/selection padding must be zero. */
struct file_transfer_record {
	enum file_transfer_message kind;
	uint64_t epoch;
	uint32_t generation;
	uint32_t frame;
	uint32_t list;
	uint32_t timestamp;
	int32_t root_x;
	int32_t root_y;
	uint32_t state;
	uint32_t detail;
	uint32_t data[5];
	uint32_t format;
	uint32_t selection_bytes;
	uint32_t rows;
	unsigned char nonce[16];
	unsigned char selection[FILE_TRANSFER_SELECTION_BYTES];
};

bool file_transfer_encode(const struct file_transfer_record *record,
	unsigned char output[FILE_TRANSFER_WIRE_BYTES]);
bool file_transfer_decode(const unsigned char *input, size_t length,
	struct file_transfer_record *record);

#endif
