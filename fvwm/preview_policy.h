#ifndef FVWM_PREVIEW_POLICY_H
#define FVWM_PREVIEW_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include "libs/module_query.h"

#define PREVIEW_NAME_MAX 64
#define PREVIEW_TIMEOUT_MIN 1000
#define PREVIEW_TIMEOUT_MAX 30000
#define PREVIEW_TIMEOUT_DEFAULT 15000

enum preview_operation {
	PREVIEW_QUERY, PREVIEW_START, PREVIEW_CONFIRM, PREVIEW_REVERT,
	PREVIEW_ABANDON
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
};

enum preview_recovery { PREVIEW_WAIT, PREVIEW_RESTORE, PREVIEW_CONFLICT };

bool preview_request_parse(const char *text, struct preview_request *request);
uint64_t preview_deadline(uint64_t now_ms, unsigned int timeout_ms);
int preview_remaining(uint64_t now_ms, uint64_t deadline_ms);
enum preview_recovery preview_recovery_check(uint64_t now_ms,
	uint64_t deadline_ms, const char *expected, const char *current,
	bool force);

#endif
