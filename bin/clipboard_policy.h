#ifndef FVWM_CLIPBOARD_POLICY_H
#define FVWM_CLIPBOARD_POLICY_H

#include "libs/content_policy.h"

#define CLIPBOARD_FRAME_MAX 192
#define CLIPBOARD_REPLY_MAX 384
#define CLIPBOARD_FIELDS 7
#define CLIPBOARD_REPLY_FIELDS 14

enum clipboard_verb { CLIPBOARD_STATUS = 1, CLIPBOARD_LIST, CLIPBOARD_PIN,
	CLIPBOARD_REMOVE, CLIPBOARD_CLEAR, CLIPBOARD_STATE, CLIPBOARD_QUIT };
enum clipboard_mode { CLIPBOARD_UNKNOWN, CLIPBOARD_UNLOCKED,
	CLIPBOARD_LOCKED };
enum clipboard_result { CLIPBOARD_OK, CLIPBOARD_BLOCKED, CLIPBOARD_REFUSED,
	CLIPBOARD_STALE, CLIPBOARD_INVALID, CLIPBOARD_FULL, CLIPBOARD_FATAL,
	CLIPBOARD_EMPTY };
struct clipboard_config {
	uint64_t epoch, lifetime_ms;
	size_t items, bytes, total, mapped, locked;
};
struct clipboard_request {
	uint64_t sequence;
	enum clipboard_verb verb;
	struct content_ref ref;
	uint64_t value;
};

/* Private numeric-only wire. No strings, payloads or extensible fields.
 * The parent owns lock-state qualification; a STATE frame is not X evidence.
 * Initial/uncertain state blocks. Installed consumers must not infer unlock
 * from a locker start/exit or expose STATE through general session controls. */
bool clipboard_numbers(const char *text, size_t length, uint64_t *values,
	size_t count);
bool clipboard_configure(const char *const *arguments,
	struct clipboard_config *output);
bool clipboard_request_parse(const char *text, size_t length, uint64_t epoch,
	uint64_t previous, struct clipboard_request *output);
/* Strict metadata discovery. Any recognized password hint excludes the item,
 * irrespective of its value; never request even the hint's bytes. This is
 * deliberately conservative. Absence does not prove nonsensitive content. */
enum content_hint clipboard_targets(const uint32_t *atoms, size_t count,
	uint32_t text, uint32_t sensitive, bool *supported);

#endif
