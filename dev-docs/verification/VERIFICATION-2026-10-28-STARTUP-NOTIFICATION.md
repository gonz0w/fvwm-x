# Application startup activity — focused checkpoint

The #56/#67 batch adds shared Desktop Entry startup metadata, a bounded native
announcement/exec helper, core notification/window observation and a native
read-only Launch activity view. It preserves literal arguments, cwd, Terminal,
old private menu catalog references and independent application lifetime.
No new dependency, resident watcher or session bus is added. Configuration files
require no migration. The [frozen contract](../decisions/STARTUP-NOTIFICATION-2026-10-28.md)
and [machine receipt](VERIFICATION-2026-10-28-STARTUP-NOTIFICATION.json) bind scope.

## Observed results

- Native Linux ARM64: affected core/helper/MFL and four Go production targets
  build with ordinary production flags. Policy unit and ASan/UBSan pass; the MFL
  request unit is retained from its unchanged source. Normal and race each pass
  908 Go test records across seven packages; minimum Go 1.26.9 passes 514 across
  three policy/adapter packages. Go 1.27.2 is the current SDK; vet and 18 real CLI
  cases pass. Normal retains one native package's 445 records from a test-only
  no-DWARF link; the other six packages and race use ordinary flags. Full-debug
  host Darwin normal/race/vet also pass, including review fixes; these host runs
  are distinguished from native Linux execution.
- Independent native X: 13 short protocol groups and complete cleanup pass.
  They cover fragmented/interleaved messages, UTF-8/quoting, orphan/late messages,
  dead and different producers, direct/group IDs (optional final NUL), malformed
  properties, Latin-1 and ambiguous class matching, public class-completion remove,
  capacity, pagination and partial expiry. A separate real interval observes
  sixty-second absolute expiry during idle and busy X, including a non-extending
  change. Query also expires state; source-reviewed normal/modal event-loop hooks
  and pure-clock tests supplement this observation. It is not an independent
  background-service oracle or hard real-time bound.
- Ten real helper/Desktop consumer groups pass: unique IDs, exact raw-byte argv
  and cwd, reaped announcer before exec, unavailable/blocked display fallback,
  known exec failure/removal, true/false/absent-class/ordinary metadata, and helper
  absence in an owned executable layout. No launch is retried. Protocol ended
  means an observed message, not application success.
- Scoped Meson install checks the actual helper/Desktop/worker/menu/browser
  bytes and executable modes. Slate dark, Dune light and enlarged Classic each
  pass initial display, paging, refresh and close, with six inspected captures.
  The [gallery](../../doc/development-preview.md#application-startup-activity)
  publishes three unmodified images and configuration/source identities.
  Host mandoc renders all four exact roff outputs at width80; this is not a BSD
  installed-man gate. Classic uses enlarged monospace type; the form's explicit Colorset 2 overrides
  its configured legacy Fore/Back defaults. That precedence is recorded rather
  than claiming those legacy colors were exercised. This is not complete theme
  or accessibility acceptance. Four affected AsciiDoc manuals generate to roff.
- Fresh task-owned FreeBSD 15.1 ARM64 and OpenBSD 8.0 ARM64 fixtures compile/run
  the source08 pure policy and helper. An independent assertion-enabled C application
  checks literal argv, physical cwd and stale-ID removal; missing executable
  returns 127 with stderr. Both guests stop normally and job keys are removed.
  FreeBSD's minimal immutable seed needed the existing required Xlib package;
  this changes no product dependency. Images came from verified cached seeds;
  package provisioning is not claimed offline or as a vulnerability assessment.

The final small grammar correction accepts valid UTF-8 unknown extension keys.
The old refusal is reproduced by the new pure unit; the affected Linux C targets,
unit/ASan/UBSan and one actual valid/malformed-key X11 producer pass. Known ASCII
keys, bounds, APIs and data layout are unchanged. Go, rendering, helper entrypoint
source and BSD-facing calls are unchanged, so those earlier exact-input results
are retained; the final portable parser delta is not described as a new BSD run.
The capture fixture's stricter final exit assertion also ran in this targeted
native probe with real exit0 receipts. Its separate real-process exit7 regression
reproduces and rejects the old failure-path oracle.

The fixed core observer state is 216,104 bytes (about 211 KiB); the debug-profile
installed helper is 120,080 bytes. These are bounded size observations, not an
RSS or performance comparison. The helper replaces itself with the application
and keeps no resident observer.

## Corrections and failed evidence

The initial native adapter lacked normal core include declarations; its compile
failed and was corrected before the passing build. Separate review corrected
valid extension-key/colon grammar, browser-worker stderr forwarding and a fixture
that could accept an already-exited nonzero client. The real-process regression
reproduces the old exit 7 acceptance and verifies rejection plus exit 0 acceptance
and terminal receipts. Old menu references and the original closed-owner callback
assertion remain intact. A controller cleanup regression emitted KillModule with
no owned view; the unchanged assertion found it, and focused coverage now passes.

Setup failures are retained: a source guard refused post-freeze changes; a host
source-layout/socket restriction was corrected; a delta based on an unapplied
snapshot was refused and rebased; native fixture compilation needed the normal
source include path. An artifact-destination typo refused before execution. The
first UI oracle searched raw TextHex directives as plain text; decoding bounded
fields preserves the same label/state expectations. Only the three affected UI
profiles repeated. A scoped archive exceeded its RAM cap, leaving original data
intact; direct host streaming then verified the complete archives.

Linux linking and expanded unit work hit ENOSPC. RAM-backed link intermediates
allowed the unchanged production build. An overlapping native-unit cache write
then filled the disk and prevented protocol cleanup receipts; all scenario
assertions, actual expiry and core/server exit 0 observations remain recorded,
while that overall run remains failed. After capacity recovery, checks resumed
serially; the short protocol/cleanup portion repeated without another full TTL
interval. A test2json executable attempted from noexec RAM failed; later ordinary
native checks pass. The task-owned partial test copy was byte-verified against
its RAM source before retirement so normal container cleanup could run.

The first BSD shell argv probe had an unintended substitution and did not gate
its comparisons on child failure. Its earlier argv/cwd/environment claims were
withdrawn. Both affected targeted probes repeated from final source with the
independent C oracle; compile/policy/exec-failure history remains preserved.

## Capacity, evidence and continuation

The 100 GiB Podman disk is shared lab storage, not a window-manager requirement.
Read-only accounting found about 54 GiB in the FVWM/root-owned container store
and 40 GiB in the separate user store (mostly other projects, plus older FVWM
containers); OS/logs/metadata account for the remainder. The active container's
/tmp held about 44 GiB of accumulated campaigns. Its source was about 70 MiB,
current GCC build 506 MiB and scoped stage 23 MiB.

The owner approved retirement of exactly two archived October 16 checkpoint trees,
including their old copied install stages. All 809 files passed host archive CRC,
payload hash, mode and link checks before exact guarded deletion, with no live
process using either tree. About 292 MiB was recovered. Canonical host archives,
working sources, reusable builds, SDKs/caches, current performance references,
VM disks and other projects remain. The disk was not expanded; no global cleanup
or retention-policy change is implied by that exception.

Canonical Linux evidence: 338 files and 47 symlinks, archive SHA256
`4428d2858cd6b13a12a6ab489ebdc5f56c150596c44a4e98204e5555cd6edcf6`
(1,395,765 bytes), fully verified on the host. Final BSD results, host Go/probe
records and frozen source/fixture deltas are linked by the machine receipt under
`builddir/codex-2026-10-28/r1-startup-notification`. The 31-file final parser supplement is separately hash-verified. The 338 regular
files/47 symlinks and 31 supplement files were retired only after host verification;
source/build/stage/cache/performance/VM state is preserved. No candidate process
remained. The first Linux teardown wrapper returned125 while the VM was already
reported stopped; this failure is retained. The final bounded resume stopped both
its container and VM normally. All fresh BSD jobs are stopped, with normal final
QEMU exits and job-key removal. Separate source/evidence review accepts the candidate and all findings are
resolved. Final exact-tree confirmation of the metadata reconciliation is retained
with the local signed-candidate receipt before handoff.

Remaining: late properties on otherwise-unmanaged group leaders, broader
installed/provider/lifecycle/service acceptance, FileManager1 under #77 and full
six-target/release qualification. No whole #56/#67 issue closes from this subset;
#257/#258 stay parked. Performance reference `m5-performance-xdg-20261028` remains
3/5 after PR #296 and 4/5 on the next runtime merge; no comparison is due.
