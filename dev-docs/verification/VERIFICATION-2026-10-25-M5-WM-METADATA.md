# M5 temporary WM metadata ownership — 2026-10-25

Owners: CLEAN-01 #34, SCRIPT-01 #4 for Script. Base merged PR249:
`9e3351103be2cfef13abea14de461360bee61e2d`.
**Focused checks pass after disk-capacity recovery; broader qualification pending.**

Script OpenWindow now uses zero-initialized local XSizeHints/XWMHints instead
of two heap records. Flags, position branches, dimensions, zero base dimensions,
WM input/state and class/title values remain. XSetWMProperties serializes these
inputs synchronously; converted title storage is released afterward. Failed text
conversion supplies NULL for both name/icon properties while still publishing
other metadata, rather than passing uninitialized output. No ordering/error
barrier is removed.

Ident's WM_ZOOM_HINTS query uses a local initialized output, removing an allocation
that was freed only on successful lookup. Existing supplied_return interpretation
and aspect row contents remain, including 0/0 rows for a complete property without
PAspect. This is ownership cleanup, not a change to that presentation policy.
IconMan releases the first converted name when icon-name conversion fails; its
normal-size hints are already independently published and remain present.

## Controls and focused checkpoint

Actual unchanged module objects link to private observational/failure wrappers;
no implementation source inclusion, production test API or shared X helper is
added. The baseline has eight real-module/X cases. Script explicit/default-position
startup retains three temporaries (text, size hints and WM hints). Ident absent
WM_ZOOM_HINTS retains one hint record; present/aspect and present/no-aspect paths
free it. IconMan second conversion refusal retains one name property; success and
first refusal retain none. The unsafe old Script conversion-refusal path is not
executed. Private baseline cleanup occurs only after retention is recorded.

Nine candidate cases pass in both the reused normal GCC 13.3.0 and scoped Clang
18.1.3 ASan/UBSan Linux ARM64 profiles. They include the eight baseline cases plus
Script conversion refusal with deliberately poisoned failure output. Every watched
candidate temporary retires exactly once, and none of these modules allocates a
heap size/WM hint record on these paths. Actual X property round trips check
published title/icon/class/normal/WM hints. Ident's real draw calls validate aspect
labels and values for absent, explicit aspect and no-aspect input.

The fixture waits for both mapping and completed Ident drawing, including the
second aspect value, before termination. This supports both pre-map pixmap drawing
and post-map Expose delivery without sleeps. Refusal still checks other metadata
and preserved IconMan size hints. Source and test scope is six changed inputs;
557 initial baseline inputs plus two unchanged helper additions and two new tests
produce 561 verified final hashes/modes. Actual profiles, compile commands, outputs,
all cases and invocation results are in the [receipt](VERIFICATION-2026-10-25-M5-WM-METADATA.json).
No new pure policy helper is introduced; verification exercises the changed actual
module paths directly rather than adding implementation-mirroring unit tests.

The combined normal module/probe build and nine cases pass on their first
candidate invocation. The first scoped build compiles but fails during linking:
`No space left on device`. No code diagnostic or sanitizer runtime failure occurs
in that attempt. After verified capture cleanup, only the scoped build and nine
scoped cases resume and pass; normal successes are retained without repetition.
Recorded commands total 4.906 seconds including the failed link/retry; storage
inspection/recovery and engineering time are outside that total.

Full-module LSan detection is disabled for the scoped X run because unrelated
font/base/process-lifetime owners remain. Exact selected XFree assertions remain
enabled, with ASan/UBSan halt-on-error. This establishes selected ownership and
observed invalid-access/UB coverage, not whole-module leak cleanliness.

Initial baseline testing caught a fixture oracle error: IconMan normal hints
precede name conversion, so refusal does not remove them. Separate source/test
review caught the draw-readiness race; both fixture corrections precede the
candidate run. Initial source/binaries/logs and corrected controls survive.
Production code stayed fixed during fixture corrections and disk recovery.

## Evidence and housekeeping

Canonical private archive: October 25 `m5-wm-metadata/native-evidence.tgz`,
181 payloads plus manifest, 5,267,634 bytes; SHA256
`d64525d4b289722f05eba538dd01a6adff0ee5eb93a95cadaf45fad55e2c780c`.
Every payload hash, size and mode is verified. It retains old sources/controls,
failed fixture and scoped-link attempts, exact source/profile/output identities,
raw passing normal/scoped cases and space-recovery receipts. The 182 verified
guest capture files and duplicate archive are retired after exact manifest and
no-active-reference checks; the canonical host archive remains.

Capacity recovery removes only two historical duplicated guest captures,
style-subject-work-combined1 and style-wildcard-cursors-combined1: 462 files each,
327,452,893 bytes total. Every hash/size/mode matches its preserved October 18 host
qualification archive; neither tree has active process references. Their canonical
archive hashes and exact retirement receipts are retained. All source/build caches,
stages, VM disks and PR243/PR248 performance references remain.

Separate final source/test/raw-evidence/report review passes with no remaining
findings. All 561 input identities, 181 payloads, baseline/candidate outcomes and
cleanup inventories/archives were independently checked. Repository checks/lint
pass; signed author-DCO delivery follows. Requested
Temper role/model settings follow PROJECT.md; actual runtime metadata is unavailable.
No new UI/theme/screenshot pass or new benchmark. PR248 reference is 1/5 after
PR249, 2/5 when this candidate merges. No BSD/AMD64/install/full default-server,
whole-module leak-clean, physical/hardware/toolkit/compositor or release claim.
Guest execution does not establish separate native-CPU qualification. Broader
#4/#34 remain open; #31/#35/#36 stay completed and #93 not planned.
