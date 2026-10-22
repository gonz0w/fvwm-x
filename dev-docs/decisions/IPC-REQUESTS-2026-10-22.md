# First structured MFL request contract

Selected implementation under IPC-02, after merged PR153. Existing configuration
syntax, native packet types/Perl metadata, JSON events and writable text clients
remain unchanged. Qualification is tracked in PROJECT-TRACKER; this contract is
not a claim of complete IPC-02 or release acceptance.

Three related features form this checkpoint: strict versioned correlation and
bounded terminal errors; authoritative core desktop/work-area queries and an
honest processed-stream barrier; and one-way per-connection read-only admission.
The FvwmMFL manual defines the wire format, supported resources and errors.

## Ownership and ordering

MFL reports only its capabilities, connection mode and subscriptions locally.
Desktop/viewport/work-area fields are sampled by fixed Send_Reply expansions in
the existing single-threaded core. There is no persistent window-state cache,
new core API, native packet type or helper settings authority. Request IDs never
become command text. Internal module-wide serials associate directed MX_REPLY
packets with an embedded client slot, rather than retaining a client pointer in
core work. Client teardown and expiry invalidate slots; late responses cannot
complete a different connection/request.

Core-bound requests join the existing module command queue and retain its
per-producer ordering/barriers. Local requests and event messages can overtake
core-bound replies, so consumers correlate IDs. A barrier reports only that the
core reached that stream position. It cannot certify command validity, applied
state, repaint, persistence or asynchronous process/service completion. Those
results require their domain owners, not an Echo or Send_Reply timing inference.

## Admission and bounds

The legacy 1000-byte line, 32-client, input/output queue and callback budgets
remain. A flat exact schema, four-level preflight, 64-byte ASCII IDs and rejected
NUL escapes bound parser work and avoid cJSON string truncation ambiguity.
Eight embedded pending slots per client have absolute five-second monotonic
deadlines. Local queries stay usable while the core waits. The shared command
queue is checked before admission; no unbounded per-request allocation or
replay/result history is added. Core queries already forwarded are not retracted
on expiry/disconnect. All replies use the same bounded output transport.

Read-only mode permanently narrows future admission for that connection, while
allowing supported queries/barriers and subscriptions. It does not undo prior
queued work. Reconnecting retains existing writable behavior; this is explicitly
not a security/confidentiality boundary for the same user or shared X display.

## Remaining owner scope

The next [settings-provider contract](IPC-SETTINGS-PROVIDER-2026-10-22.md)
adds helper-owned registry/value queries and typed generation-checked settings
actions. It retains the bounds/ownership above and makes no core-preview claim.

Complete topology/binding queries, other helper/service providers, observable
shared-core generations/CFG-01 preview, typed domain actions, applied/failed
results, progress/cancellation and service lifecycle providers remain open.
No new UI is selected here; later consumers retain actual-X screenshot gates.
