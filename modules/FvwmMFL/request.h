#ifndef FVWM_MFL_REQUEST_H
#define FVWM_MFL_REQUEST_H

#include "libs/cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MFL_REQUEST_ID_MAX 64
#define MFL_REQUEST_LIMIT 8
#define MFL_REQUEST_TIMEOUT_MS 5000
#define MFL_SETTINGS_INPUT_MAX 384
#define MFL_SETTINGS_RESULT_MAX 8192
#define MFL_SETTINGS_CHUNK 256

enum mfl_request_kind {
	MFL_REQUEST_INVALID,
	MFL_REQUEST_CAPABILITIES,
	MFL_REQUEST_CONNECTION,
	MFL_REQUEST_READ_ONLY,
	MFL_REQUEST_DESKTOP,
	MFL_REQUEST_BARRIER,
	MFL_REQUEST_SETTINGS_QUERY,
	MFL_REQUEST_SETTINGS_SET,
	MFL_REQUEST_SETTINGS_RESET,
	MFL_REQUEST_SETTINGS_RELOAD
};

struct mfl_request {
	enum mfl_request_kind kind;
	char id[MFL_REQUEST_ID_MAX + 1];
	const char *error;
	char settings_payload[MFL_SETTINGS_INPUT_MAX + 1];
};

struct mfl_pending_request {
	enum mfl_request_kind kind;
	char id[MFL_REQUEST_ID_MAX + 1];
	uint64_t serial;
	uint64_t deadline_ms;
	char *settings_result;
	size_t settings_length;
	unsigned int settings_parts;
};

enum mfl_settings_part_result {
	MFL_SETTINGS_INVALID,
	MFL_SETTINGS_MORE,
	MFL_SETTINGS_DONE
};

bool mfl_request_is_settings(enum mfl_request_kind kind);
bool mfl_request_is_action(enum mfl_request_kind kind);
void mfl_request_clear(struct mfl_pending_request *pending);
bool mfl_request_settings_command(char *output, size_t capacity,
	const struct mfl_request *request, uint64_t serial, const char *cookie);
const char *mfl_request_settings_payload(const char *message,
	const struct mfl_pending_request *pending, const char *cookie);
enum mfl_settings_part_result mfl_request_settings_part(
	struct mfl_pending_request *pending, const char *payload);
/* A provider result/error envelope, owned by the caller; never core state. */
cJSON *mfl_request_settings_result(const struct mfl_pending_request *pending);

/* Flat schema; initialized outputs retain an unambiguous recognized ID. */
bool mfl_request_parse(const char *json, struct mfl_request *request);
const char *mfl_request_reserve(struct mfl_pending_request *pending,
	const struct mfl_request *request, uint64_t serial, uint64_t now_ms,
	struct mfl_pending_request **slot);
bool mfl_request_command(char *command, size_t capacity,
	const struct mfl_pending_request *pending);
/* NULL means this packet belongs to another request (or no request). */
const char *mfl_request_payload(const char *message,
	const struct mfl_pending_request *pending);
/* A new owned result, or NULL for malformed core data/allocation failure. */
cJSON *mfl_request_core_result(enum mfl_request_kind kind, const char *payload);
/* Always consumes result, including on failure. NULL ID means uncorrelated. */
cJSON *mfl_request_reply(const char *id, const char *error, cJSON *result);

#endif
