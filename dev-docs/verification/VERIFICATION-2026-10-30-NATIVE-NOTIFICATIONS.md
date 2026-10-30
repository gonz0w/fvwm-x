# Initial native notification server — 2026-10-30

Eight related NOTIFY-01 #76 outcomes now have a focused Linux checkpoint:
exclusive optional service ownership, bounded protocol/model, replacement and
expiration/closure, opaque action replies, passive native popups, explicit
managed keyboard center, live native appearance/topology consumption, and shared
DND plus optional bounded history. This is an initial server implementation;
whole #76 and broader #72/#77 acceptance remain open.

Base: merged PR308 `6fd12b7e1dd21dc5e0530b912e90d1005c13a51e`.
Branch: `codex/r1-native-notifications`. [Machine-readable inputs/results](VERIFICATION-2026-10-30-NATIVE-NOTIFICATIONS.json)
bind all changed runtime/build/test inputs, commands, original failures and final
receipts. Final commit/tree and separate artifact review are recorded in the PR.

## Scope and result

One native module uses the accepted optional libdbus feature and existing
Flocale/colorset/module monitor/work-area primitives. Startup is explicit;
another notification owner is never queued behind or replaced. Missing bus,
module exit or disconnect does not prevent the desktop from operating. Passive
popups stay unmanaged/noninput. Explicit keyboard focus and later managed-center
geometry use the core command engine. No toolkit, package installation, new
settings store, generic bus bridge, arbitrary command execution or tray was added.

Only body/actions are advertised. IDs are server-global so the existing
transient helper can close a previous connection's notice. A replacement binds
future directed signals to the new sender. Unknown replacement IDs are refused.
History is memory-only, default zero, capped at 16, and strips live owner/action
authority. Active records cap at 32; at most three passive windows are visible.
DND closes/suppresses without later replay. Text/actions/wire/dispatch/output
have explicit bounds in the [manual](../../doc/FvwmNotify.adoc).
Libdbus's 64 KiB receive threshold can admit one final 16 KiB message: about 80 KiB
queued payload, not a total process-memory guarantee. Startup connect/auth/register/
name acquisition and individual X/kernel calls can block inside this isolated
process; event budgets do not preempt them. Startup acknowledgment releases the
core barrier before this optional work.

## Focused qualification

One reused Ubuntu 24.04.5 ARM64 development container in the owned Linux VM,
GCC 13.3.0, pinned Go 1.27.2, libdbus 1.14.10. Physical/native CPU CI and six-target
release qualification are separate. No dependency or SDK was downloaded/updated.

- **282 ordinary and 282 race passes**, zero failed/skipped cases, affected
  desktop-controller package; affected vet passes. Other packages were reused.
- Incremental enabled module, native peer, model/protocol tests, changed Go
  desktop and installed manual built in 0.949s after 1.381s reconfiguration.
- Two normal C policy/protocol entries pass. Scoped ASan/UBSan model and real
  DBus-message decoder entries pass. This does not claim instrumented UI execution.
- Tagged `notifications` install contains the exact enabled module and manual;
  the actual installed-stage module drives the native fixture. The default-disabled
  profile has neither module target nor executable install entry. Its manual
  remains available. No broad default-profile install was repeated.
- Real private-bus/nested-X tests pass: information/capabilities, nonzero IDs,
  same-XID replacement, cross-connection IDs, malformed/oversized refusal,
  focus preservation, keyboard/directed action and closure, rapid queued
  Down+Return, expiration/close/DND reasons, 32-active/three-visible flood/refusal/
  recovery, display-only history, competing owner, explicit restart, bus loss
  and surviving desktop Echo. Actual FvwmDesktop commits DND on/off and the
  native server observes suppression/no-replay. No OS power or user authentication.
- Three theme overlays and seven final captures are retained under
  [screenshots](../../doc/development-preview.md#native-notifications).
  Loaded font heights 17/20/29 at scale 1.0 are observed before capture. Slate
  Sans/dark, Dune Serif/light and enlarged Classic Mono/legacy borders were
  inspected. Text/buttons remain bounded; large headers and long text can clip.
  These are not accessibility, mixed-DPI or physical-display acceptance.
- Repository foundation, whitespace, JSON/fixture syntax and affected links pass.
  The broad portable test/matrix was not repeated for this feature batch.

## Failures, directed corrections and reuse

Initial native startup failed: the new adapter set aggregate received FD limit 0,
which prevents ordinary reads. Source02 uses 1, matching the existing client,
while the per-message limit stays0. Its affected rebuild/install/native repeat
passes in 0.227/0.087/1.753s. No parser or Go implementation changed; their initial
normal/race/sanitizer evidence is retained, not invented as a fresh repeat.

Source review found queued navigation could act on an older painted ID. Immediate
ordered selection binding fixes it; the two-record queued-key regression passes.
The observer also reacquires the ephemeral center after Escape destroys it.

Visual inspection invalidated the first enlarged-text claim: missing `xft:` font
prefixes caused fixed-font fallback. Source03 adds correct font names and observes
actual loaded metrics/config completion before final captures. A fixed numeric
Status command and inherited logging adoption support configuration diagnosis.
The final module/manual rebuild, install and enlarged-font/shared-DND native
repeat pass in 0.233/0.088/1.801s. Original native01 failure and superseded native02
captures remain identified in current review evidence.

The first cost observer used the wrong ManagedProcess PID accessor; its owned
children were retired. Corrected exact-child sampling passes. This was a fixture
failure, not a runtime result.

Source03 closure:
`04b20aa13b90b49d51e47f086551bb105fb8b3969c8db485f97322236c2763f3`.
Final runtime equals that freeze; subsequent changes only document results.

## Scoped runtime cost

Optional module only, selected build, fixed default font, no desktop-wide or peak
claim: three 100 ms-spaced snapshots per state show **5488 KiB RSS / 2215 KiB PSS**
idle, and **5744 KiB RSS / 2435 KiB PSS** after 32 maximal text/action records.
Both roughly 0.2 s settled windows show zero CPU-tick/context-switch deltas and
blocking poll. This is a short observation, not a long-term wakeup/latency budget.
Full-capacity admission itself accumulated 163 coarse CPU ticks; it is not idle
work and has no comparative throughput claim.

Installed module: 1,444,376 bytes; `size` text/data/bss 360,418/12,016/10,800 bytes.
The JSON retains full linkage, raw samples and 100 Hz tick/page identities.
It shares existing native libraries and the optional libdbus closure. This is
additional process cost, not incremental total session memory. Off/disabled
installs start no server. Periodic comparison remains 3/5 since merged PR305;
this implementation merge becomes 4/5. The last paired reference is preserved.

## Remaining acceptance and continuation

Complete only the frozen #76 gaps: new-module six-target/installed/default-X
support, mixed-DPI/hotplug, slow consumers/teardown faults, relevant instrumented
UI and accessibility/physical qualification. No current screenshot or source
review substitutes for those results. Images/markup/sound/persistence/tokens
remain unimplemented/unadvertised, not hidden passing capability claims.
Native tray stays with TRAY-01. #257/#258 remain parked.

Separate source review resolved the queued-navigation P2. Final exact artifact,
evidence and documentation review is recorded in the PR; agent review is not
human approval. Temper role invocations succeeded; backend model/effort metadata
is unverified. Owned labs are stopped. Current useful builds/SDKs/prefix/performance references
are retained; 5,284,315 host and 3,913,309 guest duplicate bytes
were retired. The tested installed-stage module and manual were verified against
the retained current prefix before stage removal. An initial build-to-installed
byte comparison was inapplicable to the install transformation; installed-to-
installed comparison succeeds. Current review logs and published screenshots
remain available; this does not preserve every superseded executable.
