# Style command ownership checkpoint

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Base: merged PR #116, `f9325ee41665430d1580f01cad6426a818a81910`.
Composed affected checkpoint passes; required CI/SVG omissions remain.

## Three related improvements

1. UseStyle borrows existing rules but owns its parsed selector. Release its
   temporary fields on success, miss and parse refusal; preserve copied style
   resources and option order. An empty list is a diagnosed miss.
2. DestroyStyle borrows its parsed selector for synchronous removal and window
   invalidation. Release temporary fields after removal/miss/refusal; retained
   rule/index/window ownership remains with its existing owner.
3. Style/FocusStyle/WindowStyle schedule a global pass only after admitting their
   selector/input. Refused commands release temporary storage and preserve any
   already pending update without creating a new pass. Accepted option parsing,
   including partial-option behavior, stays unchanged.

Actual-core allocation observations, style copying/removal, command order,
ordinary pending updates and X frame/client geometry will be checked together.
No persistent selector/window cache, format migration or moved application/grab
barrier is introduced. Existing warm native builds and known CI/SVG required
omissions/install gap remain; VESA #93 and native-CPU/hardware/release coverage
stay separate.

## Baseline and diagnostic preparation

Unchanged production with the completed allocation fixture retains nine selected
selector fields across seven observations: three UseStyle calls and six fields
from found/missing DestroyStyle calls. Three safely refused input cases each
still schedule one server grab. Style copies survive source removal and actual
frame/client geometry plus _NET_FRAME_EXTENTS retain their expected values.

The original bare Style/FocusStyle command sequence fails under scoped UBSan at
style.c's compatibility-name comparison (null name); a separate direct NULL
handler preparation reaches CreateFlagString's nonnull input precondition.
The final parser refuses missing input/token before constructing a compatibility
ID, initializes output state and reports a missing selector. Accepted selectors
and partial-option behavior stay unchanged. The fixture separately exercises
ordinary command-engine empty arguments and direct missing arguments after the
fix. Unterminated-selector controls supply unchanged baseline scheduling costs.

Earlier fixture preparations incorrectly treated comma-separated UseStyle input
as one option, then tried a whole-option quote which changes tokenization. The
completed fixture uses existing single compatibility/explicit-name forms and
multi-field DestroyStyle. All original failures, inputs, sanitizer diagnostics
and completed baseline are retained in the ignored owner directory; no broad
suite has run during this preparation.

## Focused candidate checkpoint

Seven Linux development-profile entries and 41 retained real-client cases pass.
The same seven selected ownership observations retain zero selector fields;
three safely refused cases perform zero server grabs. Ordinary missing-selector
Style/FocusStyle commands and direct missing inputs return cleanly, and a valid
subsequent style/removal still applies exact frame/client geometry. Existing
style matching, filter, lookup/profile, window-update and colorset/context
regressions pass. These are selected operation counts, not a universal timing
claim. One combined checkpoint follows on the frozen candidate.

## Scheduler fixture ordering correction

The original broad checkpoint records 2013/2014 executed standard and 443/443
optional passes. Linux AMD64 Clang's scheduler-dispatch fixture fails its
cancellation-barrier assertion; its profile is not installed. One unchanged
isolated control passes on identical binaries and limits. The original failure
and control retain separate source/evidence identities.

The fixture sends two 40-ms jobs and Deschedule as separate module commands;
that sequence does not establish cancellation before timer dispatch. The
corrected fixture installs/cancels the group within one synchronous Read, using
the same boundary its existing overdue-backlog control already relies on.
All 40/80-ms schedules, periodic/overdue assertions and connection/test limits
remain unchanged. A controlled 100-ms producer pause compares the old separate
commands with the corrected atomic admission; this is diagnostic timing, not a
production deadline change. Only this Python fixture/report change after the
frozen production checkpoint. Ten targeted fixture checks and the missing Clang
install/client probes pass; no full suite repeats.

## Source-bound composed checkpoint

Three style-command fixes pass a composed checkpoint: one original broad run
(2013/2014 executed standard, 443/443 optional), then ten scheduler-fixture
correction checks on unchanged production binaries. Fifteen ownership drivers
retain zero selected fields and perform zero refused-input grabs. Three scoped
sanitizer checks, nine composed fresh stages and four authenticated BSD
displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its
install/staged-geometry gap remain without retries.

Signed/author-DCO production source: `01aa1652088561547d77af81775115e6c4fff48c`;
tree `ebf36835dabcd04645f0219d5cd2f7d24419a70c`; source archive `b9fc2213cef818e7ff2cb90fcf386f7fa789de285acb84e206e623dc9179da58`
(2010 regular files). Final fixture-only source:
`778058c279baebbe6104575d8d08c26832d6124a`; tree `2cb68cdfe78b9cb33802bbf59d9fcb71237b40e9`;
archive `da1aa60985b8e7e80e7dccc625fb7fed3e7d2a3ac9f1b0f5a0993da8d46555cd` (2010 regular files).
Only scheduler_live.py and this report differ between those sources. All C,
headers, Meson inputs, ownership probes and other drivers are exact. Final upkeep
changes docs/evidence only. [Machine receipt](VERIFICATION-2026-10-19-STYLE-COMMAND-OWNERSHIP-COMBINED.json) preserves
both source scopes, raw failed results, corrected controls, source/mode/script/
log identities, compiler/options and exact native/install bindings.

| Scope | Result |
| --- | --- |
| Original broad standard | 2013/2014 executed; 2013/2016 including two required omissions; original scheduler failure stays failed |
| Original software/minimal | 443/443 |
| Corrected scheduler fixture | 10/10 across six targets/ten profiles, exact unchanged production binaries, no rebuild |
| Actual-core ownership | 15 drivers; eight groups plus final status; nine selected retained fields become zero |
| Refused input | Three selected cases avoid server grabs; ordinary missing selectors/direct missing arguments and pending-update controls pass |
| Scoped ASan/UBSan | 3/3: command ownership, style profile, update barriers; process-fixture leak detection disabled |
| Composed fresh install | 9 stages / 135 code / 88 catalog identities; eight original plus the formerly blocked Clang stage |
| Authenticated default BSD X | 4 displays / 30 fixtures, unauthenticated refusal and clean exit |
| Composed staged geometry | 18 invocations / 126 cases |
| Composed real-client regression | 10 complete invocations / 410 cases |
| Portable checks | 166 passes / eleven native-Meson skips, once |
| Compiler review | 11 original logs; no diagnostics in affected C files; fixture correction compiles nothing |

One unchanged scheduler control passes; the 100-ms producer-delay control emits
cancelled-a before Deschedule in the original fixture, then fails its barrier
assertion. Corrected admission passes the same delayed observation, periodic
stop and 256-job ordered/fair backlog checks. Scheduling/cancellation production
sources are unchanged. The corrected ten-entry scope resolves the fixture's
assumption; it does not relabel the original broad failure as a pass. Clang's
fresh stage follows its corrected fixture pass; its first staged client/geometry
probes pass. The other eight stages/probes are retained without repeating them.

Accepted configuration syntax and partial-option behavior, copied resources,
removal/index ownership, actual geometry, update/grab/publication barriers and
resource generations remain. There is no new dependency, ABI/install default or
production deadline. Selected allocation/grab counts are not universal latency
claims. Native warm builds use Ninja two jobs and serial Meson tests on eighteen
host logical CPUs / 128 GiB, with at most six target jobs.

Linux AMD64's first attempt stops on the /tmp space floor before source staging,
compilation or tests. Two byte/mode/mtime-verified completed owned archives move
to its larger owner filesystem; original paths remain readable symlinks. Builds,
sources and other owners are untouched by relocation. The same frozen checkpoint
resumes, retaining the failed preflight and separate recovery record. No space
requirement is lowered, no VM/disk/package change occurs and no full suite repeats.
The later scheduler assertion is a separate fixture failure, retained above.

OpenBSD AMD64 retains two required omissions: CI-01's two-second asset worker
failure (PR #113 runtime `2cf1f2f470d65cf1ebfb44dfe69a5edce11e7118`) and IMAGE-01's
SVG/MFL startup refusal (PR #109 runtime `97c728358660675353de35f866c833c926b4f203`).
Required registrations remain; neither test is retried or counted as an executed
pass. That gate stays nonzero; successful capture is separate. Its fresh install
and two staged geometry probes remain unperformed; built probes pass on
authenticated Xenocara. The corrected PR #116 image-effects fixture is registered
and passes in this full scope. Omission causes remain unproved; VESA #93 stays
deferred. Guest checks do not establish physical native-CPU sanitizer coverage;
hardware/toolkit/compositor and release acceptance remain separate.
