# Session control and optional services — 2026-10-29

Owner: Cam; R1 Major checkpoint under #72/#77, with an outbound #76 consumer.
Base: merged PR304 `94cf28be170bb66258de7c931feb420250e02128`.
The [machine-readable record](VERIFICATION-2026-10-29-SESSION-SERVICES.json)
contains exact source closures, commands, failures, artifacts and retention.

## Delivered checkpoint

One fixed five-item session inventory and private bounded owner channel support
native Session controls, three shared preferences, opt-in owned/borrowed bus
startup, explicit reprobe and an actual existing notification-server client.
Schema 5 reads older saved schemas automatically; public settings expose fourteen
preferences while the appearance editor keeps eleven and links to the compact
three-field session editor. Bus policy applies at next login; notification/DND
configuration uses the live owner handshake. No new mandatory package or Go
module is added.

Cooperative logout gathers the entire bounded eligible-client snapshot before
any Delete. Real guarded WM_DELETE requests, cancellation, a thirty-second
budget and a final core empty-generation check preserve the running desktop when
applications refuse, appear during logout or outlive the deadline. Eligibility
covers managed normal/dialog windows, excluding exact active module classes,
other EWMH types and tear-offs. This classification is descriptive, not process
authentication. No forced close or restoration of already-closed clients is
promised; final action replies acknowledge a request, not a completed exit.

The [adapter decision](../decisions/SESSION-SERVICES-ADAPTER-2026-10-29.md)
selects an isolated libdbus client, disabled by default. It sends plain bounded
notifications, tracks only its own IDs for one server/bus generation and honors
DND. It exports no methods and supplies no notification server/history/actions.

## Focused results

| Check | Observed result |
| --- | --- |
| Incremental normal build | Core, MFL, Form, Desktop, wrapper, native helper, selected units and three manuals pass; initial affected build 4.10 s |
| Go ordinary and race | Six packages, 391 composed test/subtest passes in each mode; no failed/skipped records in the selected final package results |
| Go vet | Six-package initial pass; affected owner/controller repeat passes |
| C policy and scoped sanitizers | Session-client membership/grammar/title policy, core query JSON and MFL request units pass normally and under ASan/UBSan |
| Optional native client sanitizers | Actual no-address, absent-server, GetCapabilities, Notify and Close pass under isolated GCC ASan/UBSan; distribution libdbus is uninstrumented |
| Default-disabled feature | Explicit refusal stub and no libdbus linkage verified; this reused sanitizer profile's binary size is not a normal release-size comparison |
| Install | Actual Meson session-startup tag stages wrapper/helper/login entry/manual; authoritative installed destinations supply the private integration prefix |
| Existing startup integration | Six real-X startup/direct/external/restart/readiness cases pass at source01; later owner admission changes are covered by the affected source02 checks |
| New session integration | Slate/Dune pass at source04; final Classic with legacy colors/borders passes at source07; real clients, native keys, cancellation, stale/new-client guards and normal wrapper exit observed |
| Deadline | One real Slate deadline: 30.064 s, one Delete, application and desktop still alive; other profiles reuse that evidence |
| Optional bus integration | Actual borrowed and owned bus cases, Notify/Close/DND, absent/replaced server, fresh Desktop provider with stable parent identity, borrowed-bus loss with usable WM, borrowed preservation and owned teardown pass at source06 |
| Portability | FreeBSD/OpenBSD ARM64 wrapper/Desktop cross-builds pass at source01; compile-only, before later portable admission/panel fixes |

Source02 changes the affected owner/controller runtime and tests; source03–07
change only the integration fixture. Passing unaffected builds, C policies,
manual/install rules and package results are composed rather than rerun for each
fixture correction. The final runtime inputs match the source07 manifest.
No six-target service matrix, BSD login execution, default-display/hardware or
native-CPU performance qualification is claimed by this focused gate.

## Review and retained failures

Separate Temper source review closes helper shutdown/reaping, cancellation before
child admission/mutation, stale completions and retry-after-owned-bus-exit state
findings. Review also covers the adapter, build wiring and manuals. Native images
exposed duplicate Session modules and obscured fallback notices; the corrected
projection retires changed panels, preserves identical polls and emits notices
last. Regression source and real single-panel/stacking observations cover this.

The original preview test supplied fourteen fields to an intentionally scoped
eleven-field editor; its fixture now uses the actual plan. Native observer
failures for NeverFocus keyboard/pointer timing are retained. The focused gate
observes the new F6 fallback and production stacking, then uses ordinary
WM_DELETE cleanup; it does not requalify the unchanged notice Dismiss button.
Root fixture errors (a removed stacking helper, post-Quit echo and managed-process
API mismatch) are retained and corrected. The optional adapter's first aggregate
FD limit blocked reads; its corrected threshold passes. Invalid fork-inherited
RSS samples are discarded in favor of GNU time measurements. A missing time
utility was a lab prerequisite failure, not a passing measurement.

The final annotated candidate receives a separate artifact/evidence review in
the PR handoff. Agent review is not human approval; backend model/effort metadata
is unverified. GitHub Actions remains disabled.

## Screenshots, cost and retention

[Three inspected captures and configuration examples](../../doc/development-preview.md#session-and-services)
include Slate, Dune and enlarged Classic with explicit legacy Fore/Back/ItemFore/
ItemBack and border customization. Semantic Session colorsets retain precedence;
the separate legacy Form demonstrates the fallback color path. Full 1440×1000
images are unmodified; frame/control containment passes. Raw configurations and
source/image hashes are in the [manifest](../../doc/screenshots/session-services-manifest.json).

The finite matched comparison records median transient-client RSS 2,912 KiB
(libdbus), 5,204 KiB (godbus) and 996 KiB (disabled stub). Prototype build/closure
and timing limits are explicit in the decision. The current unstripped wrapper
is 5,221,580 bytes, +466,333 from PR304. This is feature-size growth, not a resident
memory or desktop speed claim. The periodic whole-WM comparison remains 4/5 from
PR299 and becomes due after the next runtime merge.

All 264 exported evidence files were verified against the 3,028,659-byte host
archive (`ea0c8c86cfbc0582c2389aa0b8a69ac10585405fc3ddebc0d50c72f9a70d1391`).
Retired 75,997,463 bytes of owned guest duplicate evidence/stages/cross-build and
comparison scratch, plus 2,552,255 host duplicate bytes. Current source, reusable
normal/sanitizer builds, useful installed prefix, caches and performance reference
remain. Owned fixture processes are absent; the Linux container/VM stopped normally.

#72/#77/#76 remain open for their frozen broader acceptance. Power/locker/inhibitor,
tray, selected-agent/portal adapters and full service/platform release checks
retain their owners. #257/#258 remain parked.
