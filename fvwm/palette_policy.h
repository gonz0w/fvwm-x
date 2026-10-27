#ifndef FVWM_PALETTE_POLICY_H
#define FVWM_PALETTE_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PALETTE_SCAN_MAX 4096
#define PALETTE_WINDOW_MAX 512
#define PALETTE_DESKTOP_MAX 128
#define PALETTE_ROW_MAX (PALETTE_WINDOW_MAX + PALETTE_DESKTOP_MAX)
#define PALETTE_OWNER_MAX 4
#define PALETTE_TEXT_MAX 160
#define PALETTE_ID_MAX 128
#define PALETTE_BUDGET_MAX (1024 * 1024)

enum palette_kind { PALETTE_WINDOW, PALETTE_DESKTOP };

struct palette_row {
	uint64_t identity;
	uint64_t output;
	uint64_t role;
	int desk;
	enum palette_kind kind;
	char label[PALETTE_TEXT_MAX + 1];
	char detail[PALETTE_TEXT_MAX + 1];
};

struct palette_slot {
	uintptr_t owner;
	char cookie[65];
	char generation[49];
	uint64_t topology;
	size_t count;
	bool truncated;
	struct palette_row *rows;
};

struct palette_request {
	char cookie[65];
	char serial[17];
	char generation[49];
	unsigned int index;
	bool release;
};

bool palette_request_parse(const char *text, struct palette_request *request);
bool palette_next_identity(uint64_t *next, uint64_t *identity);
int palette_slot_select(const struct palette_slot *slots, uintptr_t owner);
bool palette_slot_matches(const struct palette_slot *slot, uintptr_t owner,
	const char *cookie, const char *generation);
bool palette_page_valid(size_t offset, size_t limit, size_t count);
/* Returns false when malformed/control input or omitted bytes were replaced. */
bool palette_text(char output[PALETTE_TEXT_MAX + 1], const char *input,
	const char *fallback);
void palette_row_id(char output[PALETTE_ID_MAX + 1],
	const char *incarnation, const struct palette_row *row);

#endif
