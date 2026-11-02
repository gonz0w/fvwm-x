# Native content payload owner

Owner Cam; #63 with #71/#74. Status: source implementation and
[shared Linux Q1](../verification/VERIFICATION-2026-11-01-CONTENT-DEDUP-PINNING.md)
pass; installed capture and broader qualification remain pending. The [tracker](../PROJECT-TRACKER.md) owns this
finite selection. Reuse the [allocator comparison direction](CONTENT-ALLOCATOR-COMPARISON-2026-11-01.md)
without adopting an installed service, numeric defaults or capture/UI behavior.

`libs/content_payload.[ch]` supplies one serialized native C owner. Caller-owned
bounded item/lease/payload metadata arrays remain fixed until every obligation
is released. Its embedded content policy is the sole admission/lifetime
registry; callers must not independently mutate it or acknowledge cleanup.
Payload slots retain exact references and pointers into that fixed policy array.
No generic allocator table, Go/Lua payload, hidden thread or core caller exists.

## Process and admission

`content_payload_process_prepare` is an explicit process-wide operation for a
caller-owned isolated service. It checks zero core limits and Linux process
nondumpability, returning a local PID/kind guard only after successful checks.
Partial preflight failure can already have changed this process; rollback is
not promised. Never serialize the token or reuse it after fork/exec. The owner
must not change credentials or dump policy while data exists.

Initialization verifies the token and current process settings read-only;
linking and ordinary init never change the WM's process policy. Access paths
verify the process guard again. Each mapping independently needs its checked
regional restriction or baseline mapping flag. These are configured controls,
not observed universal dump exclusion or protection from privileged inspection.

Init leaves admission disabled, following the shared policy.
`content_payload_set_admission` explicitly selects producer enable/pause state;
this is not an installed default or a desktop-lock read/history policy. Reentry
and normal operations after fatal failure are refused. Existing owner defaults
remain session-only, recognized sensitive exclusion, persistence off,
CLIPBOARD-only capture, PRIMARY sync off and history retained while locked.
The separate lock-time capture/access decision is still pending.

## Bytes and lifetime

Before reserving, derive every representation's checked encoded/decoded/derived
capacity sum and page-rounded mapping/lock model; ignore supplied physical
charges. One mapping holds its three contiguous partitions, with two guard
pages. Reserve all costs before allocation and prepare every region before
accepting bytes. Zero capacity still reserves a minimum data page, publishes
zero length and never exposes padding. Replacement keeps the existing rule:
old content retires at successful reservation, even if preparing its replacement
subsequently fails. Refusal never restores the old generation.

Writes append only at an exact initialized domain prefix, with no holes,
overwrites, overflow or implicit truncation. A call copies at most 4 KiB and
checks the exact reserved producer immediately before the copy. Writable access
closes before success. Publication derives actual lengths internally and uses
`content_finish`; callers cannot attest unwritten lengths. Valid terminal
completion still consumes once and retires on paused/disabled/expired refusal;
late retired completion drains without inspecting unprepared regions. The additive
`content_observe_time` delegates to the same clock authority so locally refused
operations cannot allow an older later time sample to revive eligibility.

Reads acquire/recheck an exact lease, validate published ranges, open read-only
access and invoke a synchronous native consumer for at most 4 KiB. No pointer
retention, blocking, event pumping, nonlocal exit or Go/Lua callback is permitted.
Close access and release the lease on every ordinary return, including consumer
refusal. Empty reads pass NULL/zero without opening the mapping. One process-wide
C trust boundary remains: a callback contract is not isolation from malicious
native code. Source, transport, decoder, UI, stack/register and X11 copies still
need their own audit; the borrowed producer source is outside this owner.

## Comparison, pinning and deduplication

Numeric description returns actual published lengths and the current pin; it
does not grant a lease or payload access. Pinning retains the shared policy's
budget and expiry rules. Original-byte comparison first requires compatible
hint/deadline policy, then the same representation ID set, kinds and encoded
lengths, independent of representation order. Decoded/derived bytes and UI
labels cannot establish original identity.

The caller owns a bounded trusted native C cursor, initialized/reset as a whole
to zero. It carries exact refs, owner epoch/PID and numeric progress; it holds
no payload pointer or lease between calls. Do not serialize it or accept it as
an authenticated proof from IPC/Go/Lua. Each step rechecks immutable LIVE
generations, compares at most 4 KiB directly under two scoped read-only leases,
then closes/releases both before committing progress. Temporary lease pressure
preserves progress for retry; failed access/closure invalidates it. Clear,
replacement and expiry invalidate subsequent work. Empty originals need no
payload access. The serialized owner still forbids callback reentry.

An explicit commit rechecks completed equality and current compatibility,
refuses a pinned duplicate and retires only the selected drop. It never extends
the survivor's lifetime or transfers a pin. Charges remain until actual cleanup;
a consumed/copied cursor cannot retire a replacement generation. Native callers
may discard/reset a cursor to cancel without draining a retained lease.

This continuation keeps bounded original comparisons within the existing owner.
A second hash/index/retention registry would add state and copies without a
current installed search requirement. No capture/transport or installed history
behavior is inferred from this source API.

## Revocation and cleanup

Retire, clear and expiry revoke policy permission immediately; they do not
assert that a failed OS protection call succeeded. A closure failure poisons
the owner, retires all items and refuses subsequent normal operations. Only
retirement/cleanup remain allowed; the future isolated service must stop.
No reset-to-healthy API exists.

`cleanup_step` processes at most one 4-KiB wipe and one region release, scanning
bounded metadata fairly. It drains a retired synchronous producer token once,
waits for leases, opens access only for wiping and closes between partial wipes.
Wipe the rounded data region before unlocking/unmapping. Retain address, wipe
cursor, lock state and full reservation charges across errors. Successful unmap
also ends any remaining lock obligation; a failed unmap after successful unlock
must not unlock twice. Only after all mappings and work/leases are gone may the
owner acknowledge policy cleanup and recycle metadata.

`error.cleanup_pending` consistently means a retired item remains in this
owner, rather than merely a live item retaining resources. Required error and
reference/progress outputs initialize on all paths. Typed policy, OS-stage,
errno, consumer-refusal and fatal results remain distinct. Preserve cleanup
errors even when later release succeeds. A time budget cannot preempt one
blocking mapping/locking syscall or a misbehaving native callback: installed
integration must use an isolated, bounded service contract, never the core loop.

## Verification boundary

Author native cases for controls/admission, domains and zero lengths, prefix
holes/duplicates, stale/replaced refs, exact publication, scoped read/reentry,
retirement/expiry and partial cleanup retries. A compile-time fixture wraps only
the small syscall boundary; production has direct calls and no runtime fault
configuration. Injected refusals establish control flow, not OS exhaustion or
enforcement. Wipe observation inspects valid memory before real/injected unmap.

Per source PR: separate review, repository/link/whitespace and affected compile.
Run authored policy/native normal and scoped sanitizer cases together with
actual production-variant OS controls at the next major payload/capture
checkpoint. Do not rerun all six comparison profiles for each helper. Existing
comparison results keep their exact source scope; this new owner is not yet
runtime-qualified. No new UI, screenshots or theme sweep at this checkpoint.
