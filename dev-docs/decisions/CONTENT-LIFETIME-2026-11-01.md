# Native content lifetime foundation

Owner: Cam. Selected scope: CONTENT-01 #63 and RETENTION-01 #71, with the
owner's 2026-11-01 default answer informing STORE-01 #74. Status: implementation
foundation; no installed clipboard manager or runtime qualification.

## Defaults and boundary

The owner chose session-only history, recognized sensitive/password items
excluded, persistence off. No-hint data may contain secrets; an absent hint is
never evidence of confidentiality. Capture/synchronization defaults, format
mapping, installed numeric limits, lock behavior and protected-memory adoption
remain separate decisions. Nothing in this change captures a selection.

The native [policy](../../libs/content_policy.h) holds only numeric metadata. It
will be used directly by the eventual isolated C content owner. Keeping this
authority native avoids a Go lease registry coordinating another native ledger.
Existing Go collection/view/action models remain reusable projections; add their
adapter when the native transport exists. The file-transfer URI channel and
plaintext pinboard storage are not adopted for clipboard payloads.

There are no payloads, labels, paths, snippets, hashes, keys, allocations, X calls,
callbacks, storage, crypto, service process, new dependency or user syntax here.
The policy is compiled by its production-linked test target; it is not linked
into the WM or installed as a service. Ordinary metadata memory is not protected
content memory. Protected delivery, wiping, actual lease revocation and backend
cleanup still require their owning implementation and evidence.

## Three implementation stories

1. Validate typed immutable representation descriptors and reserve bounded items,
   representations, encoded/decoded/derived bytes and producer work atomically.
2. Issue exact epoch/item/generation identities and nonreused lease tickets;
   retain overlapping generation charges until work, leases and cleanup drain.
3. Apply expiry, clear, pin and metadata/action checks within that same owner.
   Sensitive, malformed and unavailable hint classifications refuse admission;
   changed classifications before completion refuse publication of the whole item.

Text, image and URI are representation kinds, not parsers or proof of valid
content. Their byte reservations are worst-case bounds supplied before any
future payload allocation/decoding. The backend must refuse work exceeding a
reservation. Original bytes remain separate from display normalization.

## Lifetime and failure contract

The owner supplies fresh nonoverlapping arrays, a nonzero epoch unique across
owner lifetimes, explicit limits and monotonic milliseconds. It serializes all
calls and owns all state; no reentrant callbacks or concurrent mutation occur.
Only `enabled` and `paused` are caller-controlled policy fields after init;
arrays, accounting and counters are implementation state. Inputs/outputs must
not alias that state. Init is once per fresh owner, never a reset of live work.
Zero limits are not installed defaults. An uninitialized owner refuses operations.

Structural ceilings are 4096 items, 4096 leases and eight representations per
item. These are maximum scan/storage bounds, not allocated arrays or retention
defaults. The caller allocates only configured capacity. One producer-work
token belongs to each reserved generation; no extra asynchronous work may be
started outside those reservations. Every descriptor is copied and unused
representation slots are cleared. Admission checks subtraction before additions.

References include epoch, item ID and generation, never an array index. New item
and lease IDs increase without wrapping; exhaustion refuses before mutation.
Replacement reserves a fresh slot with the same item ID and next generation,
then **immediately revokes** the old generation. Both remain charged. A second
replacement of that old generation refuses. This simple revocation contract
avoids speculative competing publications; failed replacement does not restore
old content. A refused reservation leaves old content intact. Replacement cannot
extend the original deadline or change its hint classification.

Finish consumes the exact generation's producer token once. Retired completion
can drain work but cannot publish; duplicate/foreign completion cannot touch
another generation. Publication rechecks enabled/pause, classification and
expiry. Source eligibility changes must retire the candidate before completion.
Acquisition and safe numeric description independently check live state and
`now < deadline`, even without a sweep. Every valid clock observation advances
the clock floor, including refused operations; backward observations refuse.
An observed expiry cannot be undone by a later stale time sample.

Retirement revokes new leases, description and publication immediately. Existing
leases remain exact cleanup obligations; retirement cannot physically revoke a
future backend pointer. Clear affects only this ledger, never X selection
ownership. Pin is metadata and cannot extend lifetime or reduce charges. Dedup
compatibility requires equal lifetime/classification only; byte equality and
representation validity remain independent backend checks. No dedup operation
or budget discount exists here.

Cleanup acknowledgement is the caller's assertion that the exact retired
generation's backend resources have already been released. It refuses until
producer work and leases are zero; an early acknowledgement is not saved.
Only successful acknowledgement frees the slot and remaining charges. Holding
charges after cancellation prevents repeated cancel/reserve from evading caps.
The owner cannot free its arrays until all obligations are gone.

## Verification and continuation

Authored [production-linked cases](../../tests/test_content_policy.c) cover
arithmetic/admission, immutable snapshots, hint exclusion, cancellation with late
and duplicate completion, slot/lease reuse, overlapping replacements, cleanup,
pin/expiry, stale epoch, exhaustion and no-sweep publication/access refusal.
These are queued for the clipboard shared runtime checkpoint. Per-PR checks are
separate review, repository/static checks and native pure-C compile/link only.
No new runtime, X, sanitizer, theme, install or six-platform suite is selected.
No UI is added, so no new screenshot cycle is needed.

Next: select the protected-memory ownership/backend evaluation and finite
native owner/transport adoption before adding a collection consumer. Keep #63,
#71 and #74 open; this foundation does not complete their release contracts.
