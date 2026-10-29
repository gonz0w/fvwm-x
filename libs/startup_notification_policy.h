#ifndef FVWM_STARTUP_NOTIFICATION_POLICY_H
#define FVWM_STARTUP_NOTIFICATION_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STARTUP_TEXT_MAX 255
#define STARTUP_STREAM_MAX 32
#define STARTUP_MESSAGE_MAX 4096
#define STARTUP_RECORD_MAX 64
#define STARTUP_PARTIAL_MS UINT64_C(5000)
#define STARTUP_PENDING_MS UINT64_C(60000)
#define STARTUP_RETENTION_MS UINT64_C(60000)

enum startup_activity_state {
	STARTUP_UNUSED,
	STARTUP_ORPHAN,
	STARTUP_PENDING,
	STARTUP_PROTOCOL_ENDED,
	STARTUP_WINDOW_OBSERVED,
	STARTUP_EXPIRED
};

struct startup_record {
	enum startup_activity_state state;
	bool visible;
	char id[STARTUP_TEXT_MAX + 1];
	char name[STARTUP_TEXT_MAX + 1];
	char icon[STARTUP_TEXT_MAX + 1];
	char application_id[STARTUP_TEXT_MAX + 1];
	char wmclass[STARTUP_TEXT_MAX + 1];
	uint64_t started_ms;
	uint64_t deadline_ms;
	uint64_t window;
};

struct startup_stream {
	uint64_t producer;
	uint64_t root;
	uint64_t deadline_ms;
	size_t length;
	char message[STARTUP_MESSAGE_MAX];
};

struct startup_notification_state {
	struct startup_stream streams[STARTUP_STREAM_MAX];
	struct startup_record records[STARTUP_RECORD_MAX];
	uint64_t generation;
	uint64_t dropped;
	uint64_t malformed;
	uint64_t partial_expired;
	unsigned int screen;
};

void startup_notification_init(struct startup_notification_state *state,
	unsigned int screen);
/* Fixed storage and caller-supplied monotonic time; no X or allocation. */
void startup_notification_feed(struct startup_notification_state *state,
	uint64_t producer, uint64_t root, bool begin,
	const unsigned char fragment[20], uint64_t now_ms);
void startup_notification_expire(struct startup_notification_state *state,
	uint64_t now_ms);
int startup_notification_timeout_ms(
	const struct startup_notification_state *state, uint64_t now_ms);
bool startup_notification_pending(
	const struct startup_notification_state *state);
/* A present malformed ID blocks class fallback. Class strings use
 * ICCCM Latin1. */
/* Returned ID is borrowed until the next mutation; NULL means no match. */
const char *startup_notification_observe(
	struct startup_notification_state *state,
	const char *id, bool id_present, const char *res_name,
	const char *res_class, uint64_t window, uint64_t now_ms);
const char *startup_notification_state_name(enum startup_activity_state state);
bool startup_notification_text_valid(const char *text, bool nonempty);

#endif
