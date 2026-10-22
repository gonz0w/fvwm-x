#ifndef FVWM_MFL_REQUEST_H
#define FVWM_MFL_REQUEST_H

#include "libs/cJSON.h"
#include "libs/module_query.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MFL_REQUEST_ID_MAX 64
#define MFL_REQUEST_LIMIT 8
#define MFL_REQUEST_TIMEOUT_MS 5000
#define MFL_SETTINGS_INPUT_MAX 384
#define MFL_SETTINGS_RESULT_MAX 8192
#define MFL_SETTINGS_CHUNK MODULE_QUERY_CHUNK

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
	MFL_REQUEST_SETTINGS_RELOAD,
	MFL_REQUEST_SETTINGS_VALIDATE,
	MFL_REQUEST_PENDING,
	MFL_REQUEST_CANCEL,
	MFL_REQUEST_TOPOLOGY,
	MFL_REQUEST_WORKSPACES,
	MFL_REQUEST_BINDINGS
};

struct mfl_request {
	enum mfl_request_kind kind;
	char id[MFL_REQUEST_ID_MAX + 1];
	char target[MFL_REQUEST_ID_MAX + 1];
	unsigned int offset;
	unsigned int limit;
	char generation[MODULE_QUERY_GENERATION_LEN + 1];
	char binding_resource[16];
	const char *error;
	char settings_payload[MFL_SETTINGS_INPUT_MAX + 1];
};

struct mfl_pending_request {
	enum mfl_request_kind kind;
	char id[MFL_REQUEST_ID_MAX + 1];
	uint64_t serial;
	uint64_t deadline_ms;
	char *reply_data;
	size_t reply_length;
	unsigned int reply_parts;
};

enum mfl_part_result {
	MFL_PART_INVALID,
	MFL_PART_MORE,
	MFL_PART_DONE
};

/* Local waiting state only; no domain execution or progress authority. */
cJSON *mfl_request_pending_result(const struct mfl_pending_request *pending,
	uint64_t now_ms);
struct mfl_pending_request *mfl_request_find(
	struct mfl_pending_request *pending, const char *id);

bool mfl_request_is_snapshot(enum mfl_request_kind kind);
bool mfl_request_snapshot_command(char *output, size_t capacity,
	const struct mfl_request *request, uint64_t serial, const char *cookie);

bool mfl_request_is_settings(enum mfl_request_kind kind);
bool mfl_request_is_action(enum mfl_request_kind kind);
void mfl_request_clear(struct mfl_pending_request *pending);
bool mfl_request_settings_command(char *output, size_t capacity,
	const struct mfl_request *request, uint64_t serial, const char *cookie);
const char *mfl_request_part_payload(const char *message,
	const struct mfl_pending_request *pending, const char *cookie);
enum mfl_part_result mfl_request_part(
	struct mfl_pending_request *pending, const char *payload);
/* Owned result/error envelope from the selected core or helper producer. */
cJSON *mfl_request_result(const struct mfl_pending_request *pending);

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
