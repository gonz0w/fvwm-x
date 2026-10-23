# Proposals and decisions

- [Appearance policy observations](IPC-APPEARANCE-STATE-2026-10-22.md): configuration freshness, bounded menu/decor pages and preserved copied opacity; safe preview recovery stays gated.

- [Core colorset inspection](IPC-COLORSET-STATE-2026-10-22.md): bounded pages, exact sparse IDs and owner freshness; preview remains gated.

- [Owned desktop operations](IPC-DESKTOP-OPERATIONS-2026-10-22.md): typed refresh admission, state/results, cancellation, finite reconnect retention and coalesced invalidations; one accumulated IPC cycle checkpoint.

- [Desktop-provider observation](IPC-PROVIDER-OBSERVATION-2026-10-22.md): capability/configuration state, opt-in bounded invalidations and conditional preference snapshots; SESSION service lifecycle stays separate.

- [Binding inspection scopes](IPC-BINDING-SCOPES-2026-10-22.md): IPC-02 regular/fallback menu records, normalized placement controls and context/modifier policy; focused merge gates with shared-cycle coverage deferred.

- [Directed core state queries](IPC-CORE-STATE-QUERIES-2026-10-22.md): IPC-02 committed topology, workspace/cache observations and revision-checked global binding pages; combined affected cycle qualified; broader release acceptance remains separate.

- [Settings validation and pending waits](IPC-REQUEST-LIFECYCLE-2026-10-22.md): IPC-02 candidate validation, connection-local snapshots and wait-only cancellation; submitted actions may still commit.

- [Correlated desktop settings provider](IPC-SETTINGS-PROVIDER-2026-10-22.md): IPC-02 registry/value snapshots, generation-checked atomic updates and scoped reset/reload; the helper owns persistence and honest submission outcomes.

- [Feature checks and shared qualification cycles](FEATURE-TEST-CADENCE-2026-10-22.md): accepted user direction; focused PR merge gates, one affected platform checkpoint per roughly three feature PRs, full milestone/release acceptance and explicit deferred coverage.

- [First structured MFL request contract](IPC-REQUESTS-2026-10-22.md): IPC-02 correlation/schema, authoritative core queries, honest stream barriers, bounded lifetimes and one-way read-only admission. Broader providers/actions/preview remain open.

- [Safety and maintainability closeout](HARDENING-CLOSEOUT-2026-10-21.md): approved priority change; defer remaining performance-only tuning to the next release and use finite safety checkpoints before feature handoff.

- [OpenBSD 8.0 release baseline](OPENBSD-8.0-BASELINE-2026-10-20.md): current qualified baseline on AMD64 and ARM64; follow future OpenBSD releases, exclude 7.9, retain Linux and FreeBSD.

- [Public repository hosting](PUBLIC-HOSTING-2026-10-16.md): continue public code and PR review with local verification; release ownership remains separate.

- [Window-update ownership and ordering barriers](WINDOW-UPDATE-BARRIERS-2026-10-15.md): current DRAW-01 application/painting, retirement and query/publication contract for measured STYLE-01/STATE-01 proposals.

- [Approved optional Go patched compiler baseline](GO-TOOLCHAIN-BASELINE-PROPOSAL.md).

- [CI patched PNG input](CI-PNG-INPUT-PROPOSAL-2026-10-13.md): approved verified source bootstrap for the existing dependency; implementation and fresh-provider qualification locally accepted, with actual native CI separate.

- [CI VM provider choice](CI-VM-PROVIDER-PROPOSAL-2026-10-13.md): approved replacement of inactive external provider wrappers with a bounded repository-owned QEMU gate; affected runner scope locally qualified; fresh provider build/adoption gates remain.

- [Coordinated XDG Go integration](XDG-GO-INTEGRATION-2026-10-12.md): optional profile locally qualified; interpreter retirement and release gates remain separate.

Each record identifies the owning tracker item, status, concrete contract,
compatibility implications, evidence and remaining gates. Proposed/deferred
records are not approved implementations. Current status and execution order
belong to [PROJECT-TRACKER.md](../PROJECT-TRACKER.md).

Existing records were relocated with references repaired. Their dated contents
and user decisions remain intact. Update the relevant record when the contract
changes; create a new record for a distinct cross-cutting decision.

The website/manual brief is refined by the latest request: prioritize OS man
pages and retain lightweight optional HTML. Website visual redesign remains
separate; the former public-helper output decision is still deferred.

## Existing records

- [Milestone cutoff and feature handoff](FEATURE-HANDOFF-2026-10-21.md) — approved finite manager-target checkpoint, then BUTTON/CFG/IPC features; additional general maintenance deferred to a future release.

- [Runtime language standard: C, Go and Lua](RUNTIME-GO-LUA-2026-10-06.md)
  — runtime-first Go helper migration and explicit Perl retirement approved in
  direction; additive Go-hosted Lua primitive policies scoped, go-lua adoption
  gated; concrete ports/install changes and later tooling migration pending

- [Desktop exploration and shared primitive scope](DESKTOP-EXPLORATION-2026-10-06.md)
  — later candidates wait until existing work is complete; capability/state,
  typed actions, operation progress/cancellation, collection filtering and owned
  preview/revert contracts scoped into existing owners; APIs/backends and
  implementation/qualification pending

- [Clipboard manager and protected content primitives](CLIPBOARD-PRIMITIVES-2026-10-06.md)
  — primitives-first backlog approved; qualified text/image contracts enable a
  separate configuration consumer, encrypted persistence follows; password
  retention/defaults/backend choices and runtime qualification remain pending

- [Configurable file manager and desktop icon primitives](FILE-MANAGER-PRIMITIVES-2026-10-06.md)
  — direction/phased backlog approved; qualified browser primitives enable a
  separate config/theme consumer, jobs/pinboard/integration follow, advanced
  candidates deferred; runtime/API/dependency choices pending

- [Slate SVG artwork](THEME-SVG-ARTWORK-2026-10-06.md)
  — approved; self-contained vector default assets, native Linux review complete,
  broader theme/platform qualification pending

- [Default session manager and optional D-Bus integration](SESSION-INTEGRATION-2026-10-06.md)
  — direction/scope approved; portable default lifecycle plus one extended profile,
  implementation/settings/dependency contracts pending
- [Native notifications and system tray](NATIVE-DESKTOP-SERVICES-2026-10-06.md)
  — roadmap scope approved; runtime/dependency contracts pending, locker deferred
- [Rendering compatibility — retain existing capabilities](RENDERING-COMPATIBILITY-2026-10-05.md)
  — image-decoration removal, SVG removal and wallpaper-only image support declined
- [COLORSET-STORAGE-PROPOSAL.md](COLORSET-STORAGE-PROPOSAL.md)
- [PNG-SECURITY-BASELINE-PROPOSAL.md](PNG-SECURITY-BASELINE-PROPOSAL.md)
- [SCRIPT-FIFO-PROPOSAL.md](SCRIPT-FIFO-PROPOSAL.md)
- [SCRIPT-TEXT-LIMIT-PROPOSAL.md](SCRIPT-TEXT-LIMIT-PROPOSAL.md)
- [SCRIPT-VALUE-LIMITS-PROPOSAL.md](SCRIPT-VALUE-LIMITS-PROPOSAL.md)
- [WEBSITE-MANUALS-BRIEF.md](WEBSITE-MANUALS-BRIEF.md)
- [WEBSITE-OUTPUT-PROPOSAL.md](WEBSITE-OUTPUT-PROPOSAL.md)

- [Image input policy](IMAGE-INPUT-PROPOSAL.md) — approved and production adoption qualified

- [Compressed XPM limits](IMAGE-COMPRESSED-INPUT-PROPOSAL.md) — approved and qualified
- [SVG isolation and limits](IMAGE-SVG-ISOLATION-PROPOSAL.md) — approved; bounded security checkpoint qualified, responsiveness integration open

- [Script geometry boundaries](SCRIPT-GEOMETRY-PROPOSAL.md) — approved, implemented and qualified; broader font/resource work remains open

- [Font shadow work](FONT-SHADOW-WORK-PROPOSAL.md) — approved 4096 default/65536 override; checked iteration and consumer ownership qualified

- [SVG completion in the event loop](SVG-ASYNC-REPLACEMENT-PROPOSAL.md) — approved; owned transport and core resource integration in progress

- [Ambiguous core image completion](IMAGE-ASYNC-FORMAT-PROPOSAL.md) — approved; compressed XPM and unusual-format timing is covered, owner adoption and combined qualification remain pending

- [Deferred image-cursor bitmap fallback](CURSOR-CORE-FALLBACK-PROPOSAL.md) — approved 2026-10-06; use the existing FVWM bitmap conversion when the deferred ARGB path is unavailable.

- [Bounded file-image background averages](COLORSET-AVERAGE-PROPOSAL.md) — approved 2026-10-06; decoded RGB summaries fix opaque-image averages with the accepted palette/partial-alpha differences.

- [File-image colorset effect preparation](COLORSET-EFFECTS-PROPOSAL.md) — approved 2026-10-06; preserve existing backend pixels, isolate software blending with finite limits and extend asynchronous completion to file-image effect changes. Implementation in progress.

- [Bounded button-release servicing](BUTTON-RELEASE-SERVICE-PROPOSAL.md) — approved; common-wait ordering and 20-ms cursor/pointer timing.

- [Active viewport servicing](VIEWPORT-INTERACTION-SERVICE-PROPOSAL.md) — approved and locally qualified; 952 entries across eight compiler profiles, 120 installed identities and 16 default-BSD-X driver invocations. Native release CI remains separate.

- [Active menu servicing](MENU-INTERACTION-SERVICE-PROPOSAL.md): approved and locally qualified; 984 entries across eight compiler profiles, 120 installed identities and 12 default-BSD-X driver invocations, with documented fixture-only affected reruns. Native release CI remains separate.

- [Bounded asynchronous module output](MODULE-OUTPUT-SERVICE-PROPOSAL.md): approved and locally qualified, 64-KiB/32-packet/128-attempt asynchronous service bounds; ordinary core baseline confirms 1,000 packets in one pass.

- [Synchronous module deadline](MODULE-SYNC-DEADLINE-PROPOSAL.md): approved on 2026-10-07; keep the 30-second configurable ModuleTimeout value and apply one monotonic deadline through output and unlock. Combined Linux/BSD/default-X/optional/scoped-sanitizer qualification passes; native release CI remains separate.

- [Synchronous startup deadline](MODULE-STARTUP-DEADLINE-PROPOSAL.md): approved 2026-10-07; use ModuleTimeout when startup Timeout is omitted, retain positive overrides and asynchronous continuation after timeout. Implementation and qualification in progress.

- [JSON UTF-8 output boundary](JSON-UTF8-BOUNDARY-PROPOSAL.md): approved 2026-10-07; actual legacy-byte MFL record fails a strict JSON reader. Replace invalid bytes at JSON presentation while preserving FVWM/native bytes; implementation and qualification pending.

- [Synchronous expose delivery and bounded servicing](EXPOSE-SERVICE-PROPOSAL.md): approved 2026-10-07; bounded delivery is locally qualified with atom batching and icon identity; queue/core/pixel and combined gates pass.

- [Transient parent cycles in Layer](TRANSIENT-PARENT-CYCLE-PROPOSAL.md): approved 2026-10-07; use the originally invoked window. Guard and real-window focused acceptance pass; combined stacking/query qualification passes.

- [Cyclic Raise/Lower/RestackTransients redirection](STACK-REDIRECTION-CYCLE-PROPOSAL.md): approved 2026-10-07; use the originally invoked window. Production regression checks pass; combined qualification is locally qualified.

- [Relative Layer arithmetic](LAYER-ARITHMETIC-PROPOSAL.md): approved 2026-10-07; refuse overflowing relative offsets and preserve previous layer/order. Reviewed candidate activated; four-story arithmetic checkpoint is locally qualified.

- [Iterative parent-walk storage failure](PARENT-WALK-STORAGE-PROPOSAL.md): approved 2026-10-07; invoked-window fallback for deep-walk storage failure; production adoption locally qualified from 1319 files.

- [Layer restack storage refusal](LAYER-RESTACK-STORAGE-PROPOSAL.md): approved 2026-10-07; preserve current layer/order and keep the WM alive on failed Layer array preparation; production adoption locally qualified from 1328 files across Linux/BSD/default-X/optional/scoped sanitizer gates; this is separate from approved parent-walk fallback.

- [Unmanaged-window correction storage refusal](OVERRIDE-RESTACK-STORAGE-PROPOSAL.md): proceeds under configuration-only compatibility; retain completed managed Raise, skip only this correction on array refusal, and keep the WM running. Private normal/scoped sanitizer and real-client checks pass; production implemented/focused-passed, combined qualification pending; no separate approval hold.

- [Pager displayed desk range](PAGER-DESK-RANGE-LIMIT-PROPOSAL.md) — proposed;4096 by default, finite build override through65536. This refuses valid larger configuration ranges and needs the user's configuration decision. Private normal/scoped pure and ten actual controls pass; production is unchanged.

- [Complete language support as late polish](LOCALIZATION-POLISH-2026-10-08.md) —
  scope and late scheduling requested; I18N-02 implementation queued, translation
  tooling/services proposed.

- [Script font measurement and modal service](FONT-MODAL-SERVICE-2026-10-11.md) —
  accepted configuration-preserving contract: bounded shared readiness, explicit
  widget resource revisions and checked font conversion. No new dependency;
  qualification is recorded in the linked font/modal report.

- [Local review and GitHub hosting](LOCAL-REVIEW-GITHUB-2026-10-13.md): approved private gonz0w/fvwm-x, signed PR review and roadmap mirror; all checks local, Actions disabled; native evidence requirements retained.

- [Runtime menu-opacity preview](RUNTIME-OPACITY-PREVIEW-2026-10-22.md): one
  core-owned conditional scalar lease, deadline/conflict recovery and native
  runtime-only confirmation; saved-preference preview remains open.

- [Saved launcher-opacity preview](SAVED-OPACITY-PREVIEW-2026-10-22.md): bounded
  integer preference, schema migration, native scoped confirmation and captured
  editor-draft handoff; runtime and persistence outcomes remain separate.
