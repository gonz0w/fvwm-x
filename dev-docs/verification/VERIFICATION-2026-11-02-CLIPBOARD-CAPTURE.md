# C1: session-owned clipboard capture and retention

The native child/client and session policy portion of C1 passes the combined
Linux source checkpoint. The four selected stories are implemented. Actual
child/client execution composes ordered CLIPBOARD discovery/capture, protected
window-bounded history, dedup/pin/expiry/clear, native lock controls and cleanup.
Session Run orchestration is source-reviewed; its configured lifecycle and
pause-to-locker/start/logout integration remains queued. Installed startup
remains off pending qualified lock-state observations and explicit limits.
This does not close #62, #63, #71 or #74's wider acceptance.

See the [contract](../decisions/CLIPBOARD-CAPTURE-2026-11-02.md),
[owner decisions](../decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md),
[tracker](../PROJECT-TRACKER.md) and [source-bound JSON](VERIFICATION-2026-11-02-CLIPBOARD-CAPTURE.json).

## Observed checkpoint

- Frozen branch base: merged PR341 `fedd6c3c05f8b9b0af43b6c7eb942eeedecb53c7`.
- Plan: `5086ed5575152f82eee3439c6f97496699ab2a43fabc0b8cd42036f037aa813e`.
  The separate analyst checked effects, ownership, source guards, bounded
  execution/collection and teardown before dispatch. Separate source review
  resolved the deadline, clear-ordering, primitive-link and terminal-log defects
  and strengthened the fixture oracles before execution.
- Existing owned Linux ARM64 Ubuntu/Fedora AppleHV profile, GCC and offline
  Go 1.27.2. No TCG, packages, disk expansion, whole-WM rebuild or install.
- Forty-two compile/link commands pass in 10.30 seconds: fresh normal/scoped
  ASan+UBSan C composition and three Go package binaries plus clipboard race.
- Eight top-level runtime jobs pass. Ordinary clipboard/sessionservice/
  sessionowner packages have 5/4/82 top-level passes; the five clipboard tests
  pass again with race instrumentation. The actual Go/native metadata test
  passes inside private X. Its ordinary/race invocations deliberately skip
  without that environment; those skips are not positive evidence.
- Both normal and scoped-sanitizer private-X suites pass direct and INCR,
  recognized/malformed/unavailable hints, observed same-window reacquisition,
  ABA/loss, actual dedup after unequal-length comparison, capacity-before-payload,
  stale refs, pin/expiry, control interruptions, ordered clear, lock cancellation,
  entered-producer EOF and shutdown.
- A separate test-only release-fault executable must exit exactly 1 with the
  exact cleanup-unconfirmed diagnostic. Its independent fixture passes; this is
  an intentional negative control, not a normal clean native exit.
- A separate slow-protection executable combines 650 ms discovery and 450 ms
  locking. The original capture deadline expires; no text request/publication
  is allowed. Both profiles pass this reproduced-review-defect regression.
- All 806 regular source archive entries and 103 reuse pins are bound; the latter
  include the thin archive's 101 external object files. Binary/object hashes are
  checked between leaves. All four UID fields are checked for residual nobody
  processes; none remain. All 109 stopped-filesystem export hashes verify.
- Build/run/quiesce/export took 50.62 seconds after lab readiness. The exact
  container was stopped before collection, then the independent holder verified
  both container and machine stopped. No active native job remains.

Only public patterns and a runner-owned private X server were used. No real
clipboard content or user desktop was inspected. Source remained frozen during
qualification. Private receipts/current builds remain under the named C1
checkpoint; installed stages and other work were untouched.

## Limits retained

Scoped ASan/UBSan instruments fresh child, transport, native owner, observer,
policy and fixture objects. The existing FEvent/allocation/log archive remains
uninstrumented. Leak detection is explicitly off. Go race covers the clipboard
supervisor, not the entire session owner. This is native Linux execution, with
its virtualization/profile stated, not macOS or TCG evidence.

Host strict C11 object and offline Go compilation pass. An earlier host compile
of the shared FSelection implementation failed because the host lacks the
existing X11/Intrinsic.h header; its original receipt remains. The Linux
composition subsequently compiles, links and executes with the new transport.
No host X runtime or full Meson product build is implied by those object checks.

No full compiler/feature/six-target/install/default-X or physical-lock gate was
repeated. No concrete new platform-specific API required an early BSD sweep;
those acceptance gates remain queued. There is no new UI or theme/screenshot
claim. No broad legacy suite was rerun solely for this PR.

The stored format-8 UTF8_STRING bytes are original and unvalidated. Marker
absence does not prove nonsensitive content. A later capture may expire shortly
after admission because the configured lifetime bounds the entire retention
window. Pins never bypass budgets or deadlines. One bounded wiped Xlib reply is
allowed without a swap-exclusion guarantee; no Go payload copy is introduced.

The actual Go/native case drives clipboard.Client directly. Session tests cover
metadata admission/replay, disabled-owner refusal and reserved diagnostics; they
do not configure Run with a clipboard owner. Observe that orchestration at the
qualified lock-state/consumer integration checkpoint before claiming it passed.

UNKNOWN/LOCKED both block capture/history access. Synthetic state transitions
in private fixtures do not qualify desktop locker readiness or external locks.
A running/exited locker never resumes history. Installed numeric defaults,
qualified state integration, images, previews/restore/export, persistence/crypto,
PRIMARY synchronization and full manager UI remain separate acceptance.
