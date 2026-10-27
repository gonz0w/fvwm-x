# Native module scale handoff checkpoint

Owner: Cam. Base merged PR271 `69dda71d290c8a02842ab27b7d326a64136615b3`;
candidate `codex/r1-form-scaling`. **Focused Linux foundation passed; #51 and
#69 remain open.** [Contract and consumer manifest](../decisions/MODULE-UI-SCALE-HANDOFF-2026-10-27.md),
[source/command/archive receipt](VERIFICATION-2026-10-27-MODULE-UI-SCALE.json).

The existing Monitor14 record has no effective scale or topology generation.
Module-local RandR state cannot reconstruct core scale overrides. This batch
adds a versioned core-owned snapshot on the existing configuration channel,
a bounded native receiver, shared monitor ownership selection, startup delivery
and actual native recovery evidence. Form/browser font and geometry adoption is
the next dependent slice; no automatic UI scaling or parent issue closure is
claimed by this foundation.

## Qualification

One retained Linux ARM64 GCC 13.3.0 incremental profile builds the affected core,
library, non-installed native probe and six policy targets. `module-ui-scale`,
`module-monitor`, `monitor-ownership`, `monitor-scale`, `monitor-scale-config`
and `monitor-topology` pass. The new receiver policy passes scoped ASan/UBSan
with leak detection. Seven configure/build/test commands take 3.580 seconds;
this excludes transport, review, manual rendering and the native checkpoint.
The changed WM manual renders with Asciidoctor without diagnostics.

The actual Xorg-dummy checkpoint passes four startup factors (1, 1.25, 1.5, 2)
and a live automatic DPI/mixed-monitor case. Each early synchronous module
finishes its first configuration query before scale finalization, receives no
premature scale snapshot, and automatically receives final effective factors.
Every identity is compared with server atoms; rectangles/primary flags are
compared with independent XRandR facts. Complete replay is unchanged. Existing
Monitor14 rows retain their shape, parsing and final aggregate pixel extents.

Changing the second output's millimetres changes its automatic factor from 1
to 2 with a newer epoch. Omitting one actual completion retains the last good
snapshot; explicit abort and complete replay recover it. Real managed-window
movement is observed in root pixels before checking shared ownership and factor.
Monitor removal publishes a newer coherent snapshot. A fresh consumer starts
with the remaining topology. A real synchronized string withholds its unlock
for a 150ms observation; the following command stays blocked until actual
release, proving there are no surplus acknowledgments from configuration replay.
The corrected full native command takes 1.209 seconds, excluding transfer.
These times describe test commands, not a WM performance comparison.

All 11 native module children have actual zero-exit Wait receipts. The five
cores, Xorg server and owned command children exit zero; no unexpected observer
X error or cleanup failure remains. No default server, hardware or native-CPU
release sanitizer qualification is inferred from this guest checkpoint.

## Review and original failure

Separate source review catches and repairs two problems before qualification:
queuing a whole snapshot under one sync wait would miscount per-record unlocks;
modules started during configuration would otherwise miss final scale metadata.
The implementation retains every existing synchronization barrier and explicitly
broadcasts finalized metadata. Probe request controls unlock before requesting
another synchronous stream. Atom and movement observations are independent.

The first native run passes all four startup cases but stalls around the
ownership client's placement/move: the minimal fixture omitted a placement
policy and the core can enter interactive placement. Its original logs, real
probe lease-expiry exit 8 and unsuccessful cleanup assertion are preserved.
The identity-checked owned core is stopped to release blocked Xlib work; all
processes are then observed absent. Explicit fixture-only PositionPlacement
repairs this setup. Only the native driver reruns. No compiled product, native
probe or policy input changes after the passed build/sanitizer checkpoint.
Two earlier lab-preflight attempts find a stopped socket before any build/test;
the controlling VM session is retained for the actual gate. Neither is a pass.

## Scope and costs

The 1,534-file source/build/policy manifest remains exact. The final Python
fixture is separately frozen after its placement correction. Whole bundles own
all facts before callbacks; complete consumer publication is atomic. Per-record
sync callbacks may interleave transactions, so receivers supersede incomplete
staging and refuse stale/conflicting data while retaining last-good state.
A topology epoch is sufficient while scale configuration is startup-only; future
live policy changes must revise that generation contract. No work-area epoch is
invented. Snapshot bounds are 64 rows and 256 name bytes; invalid or unavailable
bundles publish nothing. Before the first successful snapshot, scale is unavailable.

The matched core grows 18,152 bytes (5,521,504 to 5,539,656). Form and MFL binaries
remain byte-identical. No new default resident, dependency, installed artifact,
polling source or packet type is added. Existing native packet/Perl metadata and
configuration commands remain unchanged. No speed improvement is claimed.
PR269's periodic comparison counter is 2/5 after PR271, becoming 3/5 on this merge.

This checkpoint has no user-visible rendering change and does not repeat theme
screenshots, Go race tests or an installation matrix. Form/browser scaling,
relevant six-target consumer acceptance, physical/accessibility checks and full
#51 manifest dispositions remain open under their existing contracts. Parked
#257/#258 source is not inspected or changed. Explicit current Temper role/model
assignments are used; native role loading and backend identity are not attested.

Host archive verification checks all 151 payloads/modes/hashes and gzip CRCs,
including original failure evidence and qualified core/probe binaries. Only
those verified guest duplicates (6,930,374 bytes) are retired. Source/build caches,
stages, VM disks and performance references are preserved. The owned container
and VM are stopped. Separate final source/docs and saved policy/native evidence
review finds no remaining scoped blocker, verifies source hashes and original
failure retention, and confirms the atom/movement and real-reaping oracles.
This agent review is not human approval or broader release acceptance.
