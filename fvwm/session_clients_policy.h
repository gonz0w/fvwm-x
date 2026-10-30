#ifndef FVWM_SESSION_CLIENTS_POLICY_H
#define FVWM_SESSION_CLIENTS_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SESSION_CLIENTS_MAX 512
#define SESSION_CLIENTS_PAGE_MAX 128
#define SESSION_CLIENTS_SCAN_MAX 4096
#define SESSION_CLIENTS_TITLE_MAX 80

enum session_clients_operation {
	SESSION_CLIENTS_DELETE,
	SESSION_CLIENTS_QUIT_IF_EMPTY,
	SESSION_CLIENTS_DELEGATE_XSMP
};

struct session_clients_request {
	char cookie[65];
	char serial[17];
	char target[49];
	char generation[49];
	enum session_clients_operation operation;
};

/* Exact membership, not a lossy hash. No application state is retained. */
struct session_clients_membership {
	uint64_t identities[SESSION_CLIENTS_MAX];
	size_t count;
	uint64_t revision;
};

bool session_clients_parse(const char *text,
	struct session_clients_request *request);
/* Overflow/wrap refuses the observation; unchanged membership is stable. */
bool session_clients_membership_update(struct session_clients_membership *state,
	const uint64_t *identities, size_t count);
void session_clients_title(char output[SESSION_CLIENTS_TITLE_MAX + 1],
	const char *input);

#endif
