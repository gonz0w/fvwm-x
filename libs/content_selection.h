#ifndef FVWM_CONTENT_SELECTION_H
#define FVWM_CONTENT_SELECTION_H

#include "libs/content_payload.h"
#include "libs/FSelection.h"

enum content_selection_status {
	CONTENT_SELECTION_OK,
	CONTENT_SELECTION_INVALID,
	CONTENT_SELECTION_CLOCK,
	CONTENT_SELECTION_TRANSFER,
	CONTENT_SELECTION_PAYLOAD
};
typedef bool (*content_selection_clock)(void *context, uint64_t *now_ms);
struct content_selection_options {
	Display *display;
	Atom selection, target, property;
	Time time;
	int timeout_ms;
	FSelectionSource source;
	FSelectionSourceCurrent current;
	FSelectionService service;
	content_selection_clock clock;
	void *context;
	enum content_hint hint;
};
struct content_selection_result {
	FSelectionStatus transfer;
	enum content_payload_result payload, retirement;
	/* Primary diagnostics plus aggregate retired cleanup status; retirement
	 * diagnostics remain separate and never replace the primary failure. */
	struct content_payload_error error, retirement_error;
	bool producer_taken;
	size_t length;
	Atom type;
};

/* Module-only synchronous adapter, not an installed capture/default policy.
 * Caller has already reserved/prepared one empty encoded-only representation;
 * its capacity (up to the 32-KiB role cap) bounds transport before delivery.
 * It remains caller-owned if argument/clock/producer preflight fails. After
 * successful preflight producer_taken records ownership of completion: publish
 * once on complete
 * transfer, otherwise retire the exact ref. It never releases cleanup charges;
 * caller must schedule cleanup, preserve failures and stop a fatal owner.
 * No concurrent/reentrant receive or independent writes to this producer.
 *
 * Options are snapshotted. Clock is bounded/nonblocking and cannot pump events
 * or mutate owner state. Service may cancel/expire/clear, but never reenter the
 * requestor or write its producer. Source-current has FSelection's qualified
 * observer contract. Its token alone is not server evidence. Publication is
 * rechecked against source, exact prefix and owner expiry after transport;
 * no atomic X-server/native-memory transaction is implied. timeout_ms bounds
 * the transport, not one blocking Xlib/native syscall or entered callback.
 * Caller must approve/audit Xlib's transient copy boundary before sensitive
 * use. Outputs initialize; failure may have accepted a private prefix
 * but never returns published data. No Go/Lua callback or core-loop use. */
enum content_selection_status content_selection_receive(
	struct content_payload_owner *owner, struct content_ref ref,
	uint32_t representation,
	const struct content_selection_options *options,
	struct content_selection_result *result);

#endif
