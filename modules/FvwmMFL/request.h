#ifndef FVWM_MFL_REQUEST_H
#define FVWM_MFL_REQUEST_H

#include "libs/cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MFL_REQUEST_ID_MAX 64
#define MFL_REQUEST_LIMIT 8
#define MFL_REQUEST_TIMEOUT_MS 5000

enum mfl_request_kind {
	MFL_REQUEST_INVALID,
	MFL_REQUEST_CAPABILITIES,
	MFL_REQUEST_CONNECTION,
	MFL_REQUEST_READ_ONLY,
	MFL_REQUEST_DESKTOP,
	MFL_REQUEST_BARRIER
};

struct mfl_request {
	enum mfl_request_kind kind;
	char id[MFL_REQUEST_ID_MAX + 1];
	const char *error;
};

struct mfl_pending_request {
	enum mfl_request_kind kind;
	char id[MFL_REQUEST_ID_MAX + 1];
	uint64_t serial;
	uint64_t deadline_ms;
};

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
