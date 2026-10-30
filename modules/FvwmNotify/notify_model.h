#ifndef FVWM_NOTIFY_MODEL_H
#define FVWM_NOTIFY_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define NOTIFY_ACTIVE_MAX 32
#define NOTIFY_HISTORY_MAX 16
#define NOTIFY_ACTION_MAX 4
#define NOTIFY_OWNER_MAX 255
#define NOTIFY_APP_MAX 128
#define NOTIFY_SUMMARY_MAX 256
#define NOTIFY_BODY_MAX 2048
#define NOTIFY_ACTION_KEY_MAX 64
#define NOTIFY_ACTION_LABEL_MAX 128
#define NOTIFY_WIRE_MAX 16384
#define NOTIFY_QUEUE_MAX 65536

typedef struct
{
	char key[NOTIFY_ACTION_KEY_MAX + 1];
	char label[NOTIFY_ACTION_LABEL_MAX + 1];
} notify_action;

typedef struct
{
	uint32_t id;
	char owner[NOTIFY_OWNER_MAX + 1];
	char app[NOTIFY_APP_MAX + 1];
	char summary[NOTIFY_SUMMARY_MAX + 1];
	char body[NOTIFY_BODY_MAX + 1];
	notify_action actions[NOTIFY_ACTION_MAX];
	unsigned int action_count;
	unsigned int urgency;
	double expires;
} notify_record;

typedef struct
{
	notify_record record;
	uint32_t replaces;
	int32_t timeout;
} notify_request;

typedef struct
{
	notify_record record;
	uint32_t reason;
} notify_history;

typedef struct
{
	notify_record active[NOTIFY_ACTIVE_MAX];
	notify_history history[NOTIFY_HISTORY_MAX];
	unsigned int count, history_count, history_limit;
	uint64_t next_id;
	uint64_t revision;
	bool dnd;
} notify_model;

typedef enum
{
	NOTIFY_ACCEPTED,
	NOTIFY_INVALID,
	NOTIFY_FULL,
	NOTIFY_UNKNOWN_ID,
	NOTIFY_EXHAUSTED
} notify_result;

/* Zero-initialization is valid; IDs start at one and never wrap/recycle.
 * Validation and capacity/unknown-ID failure leave all state untouched. */
notify_result notify_model_put(notify_model *model,
	const notify_request *request, double now, uint32_t *id);
const notify_record *notify_model_find(const notify_model *model, uint32_t id);
/* Removal invalidates the live ID first, then copies optional history without
 * owner/action authority. The returned record is an owned signalling value. */
bool notify_model_close(notify_model *model, uint32_t id, uint32_t reason,
	notify_record *closed);
uint32_t notify_model_expired(const notify_model *model, double now);
double notify_model_deadline(const notify_model *model);
bool notify_model_history_limit(notify_model *model, unsigned int limit);
/* Plain UTF-8; labels reject controls, body admits LF and TAB. */
bool notify_model_text(const char *text, unsigned int maximum, bool multiline);

#endif
