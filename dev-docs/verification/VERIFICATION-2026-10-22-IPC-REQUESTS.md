# Correlated MFL queries and read-only admission — IPC-02

Implementation and focused Linux/FreeBSD ARM64 checks pass. Combined six-target
qualification is pending. This first IPC-02 subset adds three related features:
versioned bounded requests/errors; authoritative desktop/viewport/work-area
queries and processed-stream barriers; and one-way read-only admission.

The [request contract](../decisions/IPC-REQUESTS-2026-10-22.md) and FvwmMFL manual
record schema, ownership, bounds, failure behavior and remaining scope. Existing
configuration syntax, native packet types, Perl metadata and legacy event/text
framing remain. The core stays authoritative through existing Send_Reply
expansions. Processed stream position does not establish applied action success
or asynchronous completion. No new UI is selected by this transport batch.

Focused native policy and real-X consumers cover strict schema/IDs, correlation,
independently observed desktop/work-area state, legacy traffic, eight pending
slots, absolute deadlines, late/disconnected replies, read-only refusal and
healthy-client isolation. Pre-freeze review handles malformed duplicate IDs
without terminating an older request, checks deadlines even when input precedes
an already-due timer, and preserves temporary JSON ownership on allocation
failure. Final combined evidence will replace this pending status before PR
publication. General topology/binding/settings providers, typed applied actions,
progress/cancellation, CFG-01 preview and wider release acceptance remain open.
