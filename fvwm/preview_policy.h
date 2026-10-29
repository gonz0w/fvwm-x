#ifndef FVWM_PREVIEW_POLICY_H
#define FVWM_PREVIEW_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "libs/module_query.h"

#define PREVIEW_NAME_MAX 64
#define PREVIEW_TIMEOUT_MIN 1000
#define PREVIEW_TIMEOUT_MAX 30000
#define PREVIEW_TIMEOUT_DEFAULT 15000
#define PREVIEW_PREPARATION_MS 5000
#define PREVIEW_ROLE_COUNT 14
#define PREVIEW_PLAN_HEX_LENGTH 336
#define PREVIEW_METADATA_MAX 65536

enum preview_operation {
	PREVIEW_QUERY, PREVIEW_START, PREVIEW_CONFIRM, PREVIEW_REVERT,
	PREVIEW_ABANDON, PREVIEW_PREPARE, PREVIEW_APPLY
};

struct preview_request {
	char cookie[65];
	char serial[17];
	char view[17];
	enum preview_operation operation;
	char generation[MODULE_QUERY_GENERATION_LEN + 1];
	char lease[MODULE_QUERY_GENERATION_LEN + 1];
	char target[PREVIEW_NAME_MAX + 1];
	unsigned int opacity;
	unsigned int timeout_ms;
	uint32_t colors[PREVIEW_ROLE_COUNT][4];
};

enum preview_recovery { PREVIEW_WAIT, PREVIEW_RESTORE, PREVIEW_CONFLICT };
enum preview_resource_guard {
	PREVIEW_RESOURCE_VALID, PREVIEW_RESOURCE_CONFLICT,
	PREVIEW_RESOURCE_OWNER_GONE, PREVIEW_RESOURCE_EXPIRED
};

bool preview_request_parse(const char *text, struct preview_request *request);
bool preview_plan_parse(const char *text, uint32_t colors[14][4]);
bool preview_metadata_add(size_t *used, size_t bytes);
bool preview_generation_room(const char *generation);
enum preview_resource_guard preview_resource_guard_check(uint64_t now_ms,
	uint64_t deadline_ms, bool generation_matches, bool owner_alive);
bool preview_resource_colors_supported(int fg_tint, int bg_tint,
	unsigned int fg_alpha);
uint64_t preview_deadline(uint64_t now_ms, unsigned int timeout_ms);
int preview_remaining(uint64_t now_ms, uint64_t deadline_ms);
enum preview_recovery preview_recovery_check(uint64_t now_ms,
	uint64_t deadline_ms, const char *expected, const char *current,
	bool force);

#endif
