# Notification lifecycle and topology — 2026-10-30

Six related NOTIFY-01 #76 outcomes extend the initial server: a bounded startup
termination request, actual unavailable/stalled bus behavior, producer and action
lifecycle, live monitor/work-area continuity, instrumented UI, and an early BSD
signal-contract check. The complete notification/support release gate stays open.

Base: merged PR309 `dacd80327f2071ba9d5dc452df554de352e8f2ed`.
Branch: `codex/r1-notification-lifecycle`. The
[machine-readable record](VERIFICATION-2026-10-30-NOTIFICATION-LIFECYCLE.json)
binds tested sources, commands, observations, failures and directed corrections.
Final signed commit/tree and separate artifact review are recorded in the PR.

## Behavior and ownership

The optional module acknowledges startup before connecting to X/the chosen bus.
A five-second process-local SIGALRM timer covers connection, authentication,
registration and name acquisition; it is canceled before resident service. The
default signal disposition is restored and an inherited blocked/ignored alarm
cannot disable the timer. No X/D-Bus callback runs in a signal handler. The timer
requests termination; an uninterruptible kernel operation can delay actual exit.
Missing/competing/unavailable services do not make the core wait. Recovery remains
an explicit module restart after restoring the chosen service.

Root work-area changes reuse one pending validated configuration request and one
refresh bit. Actual dock reservation/revocation now updates both passive popups
and the managed center. The center advertises ordinary resizable hints; equal
minimum/maximum bounds previously caused the core to refuse its entire combined
move/resize request. Geometry still goes through the shared command engine, and
normal user styles remain effective. Manual resizing does not yet reflow content.

Keyboard intent has a separate request-serial floor from pointer hit boxes.
Local navigation/painting and unchanged history configuration preserve queued
keys. Fresh views, explicit center resets and external model changes invalidate
old keyboard intent. Existing mapped/current-model/synthetic-event checks remain.
History configuration with the same limit is an unchanged-model no-op.

Terminal module teardown unloads fonts, destroys X resources and closes the
display before releasing the bounded fallback-pattern cache and Fontconfig state.
This process-final helper is never called during live theme/font updates. Existing
Fontconfig/Xft dependencies and optional feature policy are unchanged.

## Focused execution

The existing Ubuntu 24.04.5 ARM64 Linux development lab uses GCC 13.3.0 for the
normal optional-enabled build and the retained address/undefined profile
for instrumented execution. Both are actual ARM64 Linux execution on the owned
virtualized host, distinct from native AMD64 or physical-hardware qualification.
Builds are incremental and limited to affected module/test/manual targets.

- Startup policy passes normal and scoped ASan/UBSan execution. It exercises
  actual owned child termination, inherited blocked/ignored alarms, cancellation
  beyond the armed interval, invalid inputs and exact-child cleanup. An early
  FreeBSD 15.1-RELEASE-p3 ARM64/Clang 19.1.7 run passes the same unchanged three
  source files. This is signal/helper evidence, not a full BSD module/profile pass.
- Model policy passes normal and scoped sanitizer execution, including whole-model
  equality for unchanged/invalid history limits and truncation on a changed limit.
- Six native lifecycle groups pass: missing bus exits exactly 1 after startup ACK;
  a listener accepts actual AUTH bytes and withholds a reply while the core stays
  responsive, with observed module retirement around 5.03 seconds; slow-reader
  admission/recovery; disconnected producers and directed actions; teardown/fresh
  owner without stale callbacks; and idle SIGTERM wake/exit with a responsive core.
- Six topology groups pass on actual Xorg-dummy RandR state: automatic 96-pixel
  strut reservation/revocation, a mixed-scale primary switch with the same live
  popup/center, actual DPI change, output disable/re-enable, action continuity and
  three materially different scaled theme/font configurations.
- Eleven existing protocol/UI groups pass instrumented, including replacement,
  cross-connection IDs, malformed refusal, expiry/DND, flood/history/owner loss
  and queued keyboard actions. The queue regression crosses a 32-event turn with
  31 modifier presses followed by Down/Return.

The directed final run passes normal/instrumented fallback-cache teardown,
normal lifecycle and both instrumented UI suites; its report scan finds no new
ASan/UBSan/LSan diagnostics. Exact commands and receipts are in the JSON.
The missing-glyph font regression performs real U+10FFFF fallback lookup/reuse
before finalization. Leak detection remains enabled; historical diagnostics are
retained separately from final-run report scans.

The slow-reader workload is finite: four 256-call bursts, drained by known serial
between bursts, yield 32 admitted records and 992 explicit errors. A capabilities
barrier then establishes a separate interval with no peer D-Bus calls while 32
closures are generated. Another peer and the core progress; admission recovers.
It does not establish a bound on the external broker's memory. Four departed
producer connections, absent-owner action retirement and a new live directed
action are observed. Teardown covers stored live actions/center, not a guaranteed
mid-transport partial reply. IDs belong to each server owner; a new owner starts
an independent sequence. PIDFD exit observation is not a parent waitpid/reap or
signal-status receipt. The timer unit owns and waits for its own children.

## Failures retained and corrected

The merged309 baseline reproduces the work-area defect: root/core reserve y=96
while the popup stays at y=10. Adding root refresh moves the popup to y=106;
the first combined gate then exposes the center's equal-size-hint refusal.
Inspection of the core's existing resize policy and actual frame/client rectangles
identifies that cause; corrected center hints pass without policy bypass.

The first lifecycle flood run fails its peer barrier: 1,024 undrained replies
exceed the test peer's existing receive threshold. The server owner remains live,
active count stays 32 and another peer succeeds. The corrected fixture drains
bounded known-serial replies between bursts, keeps its receive limit, and states
its separate nonreading closure interval honestly. Missing-bus testing now
requires normal exit 1 rather than accepting any crash as a pass.

The first instrumented legacy UI run exposes queued-key rejection after local
painting. A separate keyboard floor plus idempotent same-limit configuration
corrects it; the strengthened cross-turn regression passes. The first topology
observer also copied a sample before center enrichment; retaining the enriched
object fixes that reporting omission without weakening geometry assertions.

The second gate's UI behavior passes, but its independent report scan fails on
320 bytes in three Fontconfig initialization allocations at some module exits.
The process-final cache/library teardown addresses ownership; the directed final
run verifies its result with leak detection and abort-on-error still enabled.
Original failed receipts and full diagnostics remain available in the owned raw
checkpoint. No failing assertion, receive limit or sanitizer setting was relaxed.

## Screenshots and remaining acceptance

[Development preview](../../doc/development-preview.md#notification-lifecycle-and-mixed-scale)
contains actual scaled Slate/Sans, Dune/Serif and enlarged Classic/Mono captures
and their existing module overlays. Popups can overlap the explicitly opened
center; they do not steal focus. Bounded word wrapping can split long words, and
large headers can truncate. These are observed development limits.

The retained optional prefix was refreshed with the tagged module/manual install.
Meson transforms the ELF at installation: the build/installed hashes differ, their
text sections match, and the actual installed module passes all six lifecycle
groups. An initial collector assertion expected whole-binary hash equality; its
failure and the subsequent installed execution are retained separately. An earlier
collector used a missing PATH entry; using the existing Meson path corrects it.

No unchanged Go, disabled-install or complete platform matrix was repeated for
this PR. The initial report's 282 ordinary/race passes and installed enabled/
disabled evidence remain accepted for their unchanged contracts. The early BSD
unit adds scoped portability evidence only. Complete six-target installed/default-X
qualification, physical display/input and relevant accessibility evidence remain
open, as does complete session/dependency/peak cost reconciliation. Current
normal and sanitizer builds, prefix, SDK/cache and runnable performance references
are retained; consumed duplicate evidence/input stages are retired after hash
verification, and owned labs are stopped.

#76/#72/#77/#225 remain open. #257/#258 stay parked. Performance is 4/5 after
PR309; merging this runtime batch reaches 5/5, so the next combined checkpoint
includes the due paired comparison. No performance improvement is claimed here.
Workflow/implementer/analyst/reviewer roles were used; backend model/effort metadata
is unverified. Separate agent review is not human approval or full release acceptance.
