# Portable session startup checkpoint

Date: 2026-10-29. Owner: Cam. SESSION-01 #72 remains open.
Base: merged PR303 `886e0d17cd69725e484c8e6a7cee0a28c612a01e`; canonical
signature, reviewed tree and author DCO verified; matching branch retired.
The [finite tracker contract](../PROJECT-TRACKER.md#r1-portable-session-startup--2026-10-29)
preceded implementation. [Structured results](VERIFICATION-2026-10-29-SESSION-STARTUP.json)
bind source, commands, artifacts, failures and cleanup. The source baseline had a
direct-core login entry and no session wrapper; no invented pre-change runtime
failure is claimed for the new feature.

## Delivered slice

- Packaged `fvwm-session` login entry, literal FVWM arguments and environment
  defaults without replacing inherited authentication, bus or runtime settings.
- Private nonblocking one-shot core acknowledgment after initial StartupStuff,
  with a fifteen-second admission deadline and no implicit retry.
- Owned WM lifetime across ordinary exec Restart; external XSMP uses the existing
  direct-core path. Independent applications are not adopted session services.
- Controlled XDG autostart: filename precedence before filtering, Hidden,
  desktop filters and TryExec, NoDisplay support, literal Exec and explicit
  Terminal/unavailable outcomes. Valid Exec supplies the documented bus fallback.
- Bounded isolated acquisition, complete-result validation, ordered launches
  and distinct started/exited/failed/skipped diagnostics. Started is not ready.
- Same-literal-display record ownership with private first-login cache creation,
  conservative fallback and no lock unlink, PID-file signaling or service adoption.

The [manual](../../doc/fvwm-session.adoc) defines options, bounds, examples and
recovery. Selection follows [XDG Autostart 0.5, sections 2.1–2.2.3](https://specifications.freedesktop.org/autostart/latest/)
and reuses the accepted Desktop Entry parser/Exec policy. Menu NoDisplay filtering
is deliberately not reused for autostart. No new dependency, bus, toolkit,
preference store or authoritative X state owner was introduced.

## Actual verification

Linux ARM64 used the existing owned Podman VM, GCC 13.3.0, Clang 18.1.3 and the
retained Go 1.27.2 SDK. These are VM/profile observations, not native-CPU,
physical-hardware or whole-platform qualification. No full matrix was repeated.

| Check | Observed result |
| --- | --- |
| Affected incremental build | Core, MFL, readiness helper and wrapper pass; the directed wrapper/manual continuation also passes. |
| Go policy and child lifecycle | Four packages pass in composed ordinary and race evidence, 654 test/subtest records each, no skips. Vet passes for all four. |
| Readiness descriptor/signal helper | Actual Linux normal and ASan/UBSan unit passes; leak checking and undefined-behavior halt enabled. |
| Early actual FreeBSD gate | FreeBSD 15.1 ARM64, Clang 19.1.7; corrected production helper passes. Separate probe records both pipe ends as O_RDWR. Both guests exit normally. |
| BSD Go builds | FreeBSD/OpenBSD ARM64 commands compile with CGO disabled. This is compile-only evidence, not BSD execution or full login acceptance. |
| Scoped private install | Actual Meson tag installs wrapper, desktop entry and manual. Core companion copied from its authoritative Meson destination; this is not a complete package stage. Entry Exec, desktop names, wrapper help and generated page placement pass. |
| Six actual-X scenarios | Corrected display/literal application arguments, real WM Restart, no-autostart, external/direct core paths, source-default desktop startup and readiness timeout all pass. Combined run: 16.875 s. |
| Final manual | English generation plus host mandoc at width80 pass without diagnostics. Final shutdown/FILES prose is manual06; installation wiring was exercised on source03. |

The actual-X run uses the scoped staged wrapper/core, real MFL and nested Xvfb.
Inherited DISPLAY deliberately differs from `-d`; the application records the
selected display and preserved marker environment. Native evidence observes
application display; Go tests prove the same selected value reaches lock
acquisition. Old MFL EOF, fresh independent reconnect/echo and OS PID/PPID before
and after Restart show the same directly owned WM and one autostart launch.
Hidden/malformed/filter/missing-program winners never launch their lower copies.
The stalled StartupStuff case times out autostart while the core still answers
an independent command and exits normally on Quit.

No UI primitive changed. No new screenshot or three-theme matrix was claimed
for login wiring; the source-default desktop startup is functional evidence,
not a new appearance/accessibility qualification.

## Failures, review and composition

The first actual FreeBSD helper refused its valid duplex pipe because the
initial predicate assumed O_WRONLY. The directed fix accepts writable FIFO modes
O_WRONLY/O_RDWR while preserving type, nonblocking and CLOEXEC requirements.
The second guest's real flag probe records read=2/write=2; all helper assertions
pass. Original assertion, compiler/kernel, source and archive identities remain.

Source01 ordinary Go found one test setup error: Go testing's numbered TempDir
child is created with 0777 before umask, so it need not satisfy the private-runtime
0700 contract. The actual SDK source/hash is recorded locally. The test now
creates an explicit private leaf; production validation and negative cases remain.
Separate review also found a real DISPLAY override mismatch; pure option-arity
handling now selects the correct environment before ownership and application
startup, preserving option-looking literal values and falling back conservatively
for uncertain invocations. Directed Go and actual-X evidence covers the repair.

Source03 ordinary Go then found a diagnostic test expecting a different phrase
from the scanner's declared error. Its useful-cause assertion was corrected;
source04 reran only that command package in ordinary/race modes. Unchanged
source03 runtime, race, install and X results remain applicable. Source02 was
prepared only, never dispatched. Final manual-only corrections were generated
and inspected separately without repeating runtime qualification.

Executed source04 tree: `a87ed2f06ffe0007d9dc354db320c879d769c599`, closure
`d7daa51ce590f066ef6fdbf63d1efaea7e197a2e123f8bbbe4d90f3930d5acf1`.
Runtime bytes are unchanged from source03. The host archive has 119 verified
regular files, 585,291 compressed bytes, SHA256
`ff708c8e4ff3629f4551a5c7678bee372be87454cc6a5ba8224cdac96a062ce1`.
Separate `temper_reviewer` source/evidence review resolved the display finding
and accepts integrated tree `ba26f88490e54e365a9b7f313700f1b317ce3aac` with no
remaining actionable findings. Subsequent annotations only record that outcome. Requested
backend role/model/effort metadata remains unverified. This is agent review,
not human approval or merge authorization.

## Retention and remaining acceptance

Both disposable BSD disks retired: 6,455,230,464 allocated bytes. Guest duplicate
inputs, scoped stage, cross-build binaries and evidence retired: 98,326,925 bytes.
Earlier host full inputs became verified payload-reconstruction deltas, recovering
73,541,310 bytes. Current builds, useful full prefix, SDK/dependency caches and
the next performance reference remain. The owned Linux container/VM stopped
normally. The current compact host review bundle remains available. Superseded merged303
full source/evidence archives also retired after review; its compact versioned
reports, canonical merged source and published images remain.

#72 is not complete. Full service/provider readiness, competing display aliases,
crash-surviving service ownership, cooperative application logout and broader
installed/default-display/support qualification remain open. A WM ignoring its
cooperative shutdown request can keep the parent waiting indefinitely; this
slice adds no forced application closure. Filesystem/executable observations can
change before use, and a deadline cannot preempt every kernel operation. No bus,
locker, authentication or confidentiality property is claimed. #77/#225 retain
their owners and #257/#258 remain parked.

Performance is 3/5 from merged PR299, 4/5 only after this merge. No comparison is
due. The new wrapper's unstripped file is 4,755,247 bytes; resident RSS and
whole-session speed/memory effects were not measured here.
