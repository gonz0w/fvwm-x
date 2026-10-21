# FVWM-X security maintenance status

Updated 2026-10-21. This is an ongoing audit, not a security certification.
The active work list and acceptance criteria are in
[PROJECT-TRACKER.md](PROJECT-TRACKER.md).

The selected finite [safety/maintainability closeout](PROJECT-TRACKER.md#hardening-closeout)
is locally qualified and merged in PR143; advance approved
feature dependencies. Remaining performance-only tuning stays deferred. Broad
audit owners and release gates remain open.

The [function-lifetime checkpoint](verification/VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.md) fixes three reproduced failures:
selection shutdown hangs, held-classification shutdown hangs and conditional
actions after target destruction. Existing context/module/function/window
retention guards have explicit already-covered dispositions. Six targets pass
2276 standard / 47 selected optional entries, scoped guest sanitizers,
ten installs and four authenticated default BSD displays. Original controls
and orchestration errors remain in the report. No confirmed unresolved
high-impact defect remains in the selected closeout set; this does not certify
all historical audit paths or authorize release.

The [stream ownership checkpoint](verification/VERIFICATION-2026-10-21-STREAM-OWNERSHIP.md)
qualifies four FIFO/output/log adoption/error repairs across six targets.
It preserves original harness failures and uses affected continuations, with
2236 composed standard / 58 optional passes, scoped sanitizers and ten installs.
The [module/input checkpoint](verification/VERIFICATION-2026-10-21-MODULE-INPUT-OWNERSHIP.md) resolves the
LoggingFD provenance concern with an old-binary mismatch control, fork-time
identity and once-only adoption. Malformed framing, Script token ownership and
a reproduced root-context configuration crash are repaired. Six targets pass
2252 composed standard / 117 optional entries, scoped sanitizers and ten final
installs. Original fixture/runner failures and affected completions remain
explicit. Selected configuration-view and Script packet/value policies are
already covered; the selected interaction pickup is now qualified above.
Broad audit owners remain open; PR142 is merged.

OpenBSD 8.0 is the current qualified release on both VM architectures;
[its checkpoint](verification/VERIFICATION-2026-10-20-OPENBSD-80.md) resolves
the previously missing required entries. Follow future releases; 7.9 is out of
scope. Older VESA/startup observations below remain dated historical evidence,
not current 8.0 qualification failures. Recent
[string ownership](verification/VERIFICATION-2026-10-21-STRING-COPIES.md) and
[preparation evidence](verification/VERIFICATION-2026-10-21-BSD-PREPARATION.md)
checkpoints are merged in PR138/139. Native-CPU and physical/release gates remain.

Hosting note, 2026-10-13: the repository now uses private GitHub PR/issue review
and local-only checks, with Actions disabled. Older no-remote/configured-CI
wording below describes its dated snapshots. Native CPU/sanitizer and physical
qualification remain explicit requirements. Follow the
[current hosting policy](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md).

For the latest frozen checkpoint states, use the tracker's
[current summary](PROJECT-TRACKER.md#current-checkpoint-status). The dated
observations below preserve their original scope and may describe intermediate
states. Pager source `7284` and canonical-text source `3c37` are locally qualified.
The latter passes 1736/120/276, optional 222/202 and scoped 108, with seven
complete final archives. The user resumed current milestone qualification on
2026-10-11. A single `005f` / final test-only `01c2` matrix includes `a653`/`3074`;
[combined qualification](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md)
is strictly locally qualified: 1816 standard entries, 120 installed identities,
300 default-BSD-X drivers, optional 232/212 and scoped 118. Seven archives
validate all 1409 source hashes/modes and affected-fixture derivations. Native
release CI and physical checks remain open. No additional production boundary
implementation or whole-project certification follows from this gate. The
Read logical-line dependency is now qualified within the [XDG integration](verification/VERIFICATION-2026-10-12-XDG-PORT.md);
the wider DATA-01 reader/stream audit remains open.

| Area | Finding and practical effect | State and evidence |
| --- | --- | --- |
| Default BSD server startup reliability | Two retained OpenBSD AMD64 VESA/DDC failures occur before any WM/client; cause is unestablished. | The four-story diagnostic/comparison checkpoint passes affected six-target policy/teardown, 80 server-only controls and ten unchanged application fixtures. A fresh updated/rebooted image passes 16 of those starts. This is no causal fix or security finding; LIB-07 #93 stays open. [Report](verification/VERIFICATION-2026-10-14-BSD-X-STARTUP.md). |
| Optional Go dependencies/compiler | Six current native SDK scans and the actual minimum scan report no affected package/symbol findings; three unimported module observations remain. Unused Logrus is removed with six retained pins/payload bytes unchanged. | Approved optional stable Go 1.26.9/1.27.2+ policy and fixed BSD SDK preparation are locally qualified with offline build/install/CLI/X/race coverage. No general security guarantee; two pre-client OpenBSD AMD64 VESA startup faults remain under LIB-07 #93 with unchanged passing application controls. [Adoption](verification/VERIFICATION-2026-10-14-GO-BASELINE.md), [audit](verification/VERIFICATION-2026-10-14-GO-DEPENDENCIES.md). |
| Prompt transport/lifetime | Partial writes, sleep-based startup, fragmented JSON and peer EOF needed explicit framing and lifetime ownership. | Standard-library transport, bounded records/writes, strict UTF8/JSON and coordinated shutdown are strictly locally qualified on six targets with actual MFL/default-BSD-X, Go race/vet and scoped C checks. No new mandatory dependency; optional Go dependency/advisory review remains LIB-03. [Report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md). |
| XDG metadata/settings | Unbounded or evaluated metadata/preferences and a shared Form token callback type mismatch needed explicit ownership and limits. | Optional Go provider/native Form is locally qualified on six targets: bounded XML/files/layout, process-group deadline, private literal action catalogs, atomic data settings and one-use results. Logical Read lines are complete/bounded. Callback correction passes direct and actual scoped ASan/UBSan controls; installed native consumers pass. Disabled-Go predecessors, optional Go dependency review and broader audits remain separate. [Report](verification/VERIFICATION-2026-10-12-XDG-PORT.md). |
| Xlock help producer | Legacy menu discovery uses a shell pipeline and can wait for unlimited help output. | Go-enabled installs now execute the help probe directly with five seconds and 1 MiB, explicit failure and escaped external labels. Six-target probe/install/menu checks and actual Linux race utilities pass. The disabled-Go Perl fallback retains its legacy probe; broader runtime retirement remains separate. [Report](verification/VERIFICATION-2026-10-12-XLOCK-PORT.md). |
| Native/Perl event contracts | Extended constants/mask, Echo field layout and shared category-bit handler matching could misroute events. | All ten native event values/decoders and category-aware dispatch are repaired; deterministic checked metadata and exact installed consumers pass the combined checkpoint. Perl retirement remains separate. [Report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md). |
| Stacking and producer lifecycle | Managed-window retirement and producer storage/retirement paths needed stronger ownership across callbacks and allocation refusal. | Exact 1339-file lifecycle checkpoint locally qualified: 1288 standard entries/eight profiles, 120 staged identities, 116 default BSD X drivers, optional 166/146 and 45 scoped sanitizer checks. No combined source change or affected rerun was required. Broader lifetime work remains open. [Report](verification/VERIFICATION-2026-10-07-MODULE-LIFECYCLE.md). |
| Module iteration and packet construction | A pinned retired member could disrupt traversal; packet construction needed header-aware bounds, aligned field copies and correct configure identity. | Iterator cleanup holds, bounded fixed packet storage and pure structured packing implemented. Exact consumer-layout regression and focused Linux/scoped/native BSD ARM64 controls pass. Combined qualification is separate from the accepted lifecycle source. [Iterator](verification/VERIFICATION-2026-10-07-MODULE-ITERATION.md), [packets](verification/VERIFICATION-2026-10-07-MODULE-PACKETS.md), [fields](verification/VERIFICATION-2026-10-07-MODULE-FIELDS.md), [configure](verification/VERIFICATION-2026-10-07-MODULE-CONFIGURE.md). |
| Third-party native packages | Older unverified system PNG packages remain in several verification environments; tested patched profiles are separate. | Approved PNG configure default and six-target patched runtime qualification pass. Debian fixed PNG/XPM packages are authenticated and qualified in a private prefix. Both OpenBSD snapshots have signed updates; their XPM libraries match signed release sets and signed source contains the vendor fix. Ubuntu server and remaining transitive qualification remain open. [Audit](DEPENDENCY-AUDIT-2026-10-05.md), [baseline](verification/VERIFICATION-2026-10-05-PNG-BASELINE.md). |
| PNG decode recovery | An ordinary incomplete PNG aborts the unmodified shared-loader probe (SIGABRT); pixmap failure outputs also retained stale IDs. | Explicit libpng error recovery owns cleanup across nonlocal returns, checks short signatures and resets caller outputs. Linux 78/78 on three legs, BSD 67/67 on four targets, 188 installed default-BSD-X outcomes and 45 staged Linux core checks pass. PNG-disabled ARM64 build passes 45/45. Reviewed patched PNG profiles pass. Wrapper/profile cleanup passes separate focused gates; decoder budgets/isolation and native sanitizer CI remain open. [Report](verification/VERIFICATION-2026-10-05-PNG-RECOVERY.md). |
| Module clipboard input | Malformed selection data reached an unsafe string copy and terminated the old FvwmScript process in the saved control. | Fixed bounded, typed reads with owned storage and deadlines. Historical full gates were Linux 55/55 and BSD 44/44. Later complete-rebuild and fresh-output checks supersede earlier Form assertions; see the [numeric follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-VALUES.md). [Report](verification/VERIFICATION-2026-10-04-MODULE-INPUT.md). |
| Module descriptor setup | Basic setup failures lacked injected coverage; native module subprocesses could retain protocol endpoints. | Six basic failures and four parser flag failures are covered. Common startup validates endpoints and applies checked close-on-exec; five legacy modules adopt it. Fork/exec and actual Script subprocess closure checks pass. Borrowed ownership and listen-only -1 are preserved. Actual helper/session/subprocess constructor failures and broader Perl/helper ownership remain open. [Follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md). |
| Linux executable flags | Clang/lld lacked immediate binding despite other hardening defaults. | Explicit supported linker flags; produced C executables are checked for PIE, RELRO, immediate binding and non-executable stacks. Optional Go binaries are outside this check. |
| Script file processing | Long evaluated text, empty text, exact-size reads and unchecked file operations made the old implementation unsafe. | Bounded replacement implemented; focused checks pass on every Linux/BSD target. User approved the strict output-file policy; finalized. [Report](verification/VERIFICATION-2026-10-04-SCRIPT-FILE.md). |
| Script message processing | An empty queue used an invalid index for an invalid target and returned incorrectly addressed refusals. Argument indices were unchecked. | Explicit queue search and checked script indices implemented; actual-module checks exercise refusal and continued operation. |
| Script commands and output | Do evaluated function arguments twice; launch/output used fixed buffers, and startup arguments exceeded their array. | Single evaluation, bounded dynamic launch assembly, finite child slots, early communication setup, bounded startup arguments and owned output/cache storage implemented. Linux 58/58; BSD 47/47 plus default-server checks. [Report](verification/VERIFICATION-2026-10-04-SCRIPT-INTERPRETER.md). |
| Script bounds and deadlines | Invalid widget IDs and unchecked parser arrays could index outside their storage; GetOutput could wait indefinitely. | Shared widget lookup and parser boundary guards implemented; title-argument grammar stack setup fixed. User-approved five-second whole-command deadline, explicit override, bounded output and owned process-group cleanup implemented. Final combined gates pass Linux 62/62 and BSD 51/51 on both architectures, plus 22 actual-widget checks on all default BSD servers. [Follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md). |
| Script numbers, strings and For | Arithmetic could overflow or divide by zero; hex padding and framed parsing could exceed their storage; For decoded its body pointer as a third argument. | Checked helpers, owned padding, complete Parse validation and safe For limits/storage implemented. Prefix/no-digit conversion, signed hex patterns and ordinary theme results are retained. Set/Key allocation multipliers and repeated prefix scans are removed. Fresh full gates pass Linux 63/63 and BSD 52/52 on both architectures, with 24 widget checks on all default BSD servers. [Numeric follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-VALUES.md). |
| Verification provenance | Frozen source fingerprints could coexist with old guest objects; Form could read a previous output file. Snapshot packaging also dropped executable modes. | Final runners reconfigure and rebuild every overlaid source, preserve permission metadata and wait for fresh Form output. Historical Form assertions are superseded by the [numeric follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-VALUES.md). |
| Script execution and nesting | Empty or long loops and recursive signals could run without an interpreter bound. | User-approved defaults of 10000 shared work steps and 64 active executions, configurable overrides, latched failure/normal unwind and later-event recovery implemented. Linux 65/65 and BSD 54/54 full gates pass; seven execution outcomes and 24 prior widget outcomes pass on every default BSD X server. [Execution follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-EXECUTION.md). |
| Script value storage | Interpreted string growth and aggregate stored table text lacked finite configurable limits. | User-approved ValueLimit 1 MiB and MemoryLimit 32 MiB implemented. Stored names/values/NULs count; replacements credit old values. Refusal preserves prior values/accounting, stops the handler and permits next-event recovery. Linux 67/67 and BSD 56/56 pass, with six storage outcomes plus prior execution/widget checks on every default BSD X server. [Storage report](verification/VERIFICATION-2026-10-05-SCRIPT-STORAGE.md). This is not a whole-process memory cap. |
| Script widget delays | Legacy waits spun on wall-clock time; pending MiniScroll repeats could run after release and delay termination. | Shared checked monotonic sleeps plus user-approved release cancellation preserve first action/held cadence. Queued releases, restored arrows, once-only Return and held termination pass. Final Linux 70/70 and BSD 59/59 gates include sixteen widget outcomes on every default BSD server. Previous-handler control confirms the removed callback with production hashes protected. Outside-arrow polling and broader modal/FD/lifecycle handling remain open. [Release report](verification/VERIFICATION-2026-10-05-SCRIPT-RELEASE.md). |
| Script widget arithmetic and input | Progress/scrollbar calculations could overflow or divide by zero; numeric literals overlapped internal tags; scrollbar Return/release handling could wait incorrectly. | Shared checked math and user-approved rendering clamps preserve stored values. Full int32 tags/checked mutation parsing, safe MiniScroll wrap, tiny fills and poll-based scrollbar waits are implemented. Linux 69/69 and BSD 58/58 pass; final eleven widget outcomes pass on every default BSD server. The previous AMD64 control exits at an equal-range redraw; ARM64 instead continues but fails the rendering comparison. [Widget report](verification/VERIFICATION-2026-10-05-SCRIPT-WIDGETS.md). |
| Colorset records | Dense arrays allocated/initialized through the highest ID; signed count arithmetic and unchecked assignments could lose prior state. | User-approved sparse stable records, checked int32 IDs, 4096 count limit/65536 override and zero cloning are implemented. Final Linux 73/73 and BSD 62/62 pass, including seven palette outcomes on every default BSD server and fresh installs. Actual configured endpoint limits pass policy/API coverage. [Colorset report](verification/VERIFICATION-2026-10-05-COLORSETS.md). Rendering, borrowed-resource lifetime and IPC budgets remain separate. |
| Script FIFO communication | Blocking opens/writes/reads could outlive counted waits; failed writes corrupted progress, endpoint type/owner was unchecked, and full replies lacked a terminator. | User-approved five-second overall deadline/finite override, checked owned nonblocking FIFO endpoints, atomic mode-2 writes, bounded terminated replies, cancellation and cleanup implemented. Final Linux 75/75 and BSD 64/64 gates pass; ten FIFO outcomes plus prior module regressions pass on every default BSD server. All seven archives and fresh installs match tested source/code. Test-only signal cleanup and stale-build corrections are preserved. [FIFO report](verification/VERIFICATION-2026-10-05-SCRIPT-FIFO.md). Same-user message content and script sandboxing remain separate. |
| TextField/List maintenance | Text offsets/edits, selected byte ownership and list geometry/expose allocation lacked consistent bounds. | Checked offset/edit helpers, shared mouse selection, wide list math, sequential option scans, skipped-row allocation removal and font teardown implemented. Linux ARM64 GCC/Clang and AMD64 GCC pass 77/77; all four BSD targets pass 66/66. Eight text/list and sixteen prior widget outcomes pass with staged binaries on every default BSD server. Six archives match all 1173 source hashes/modes and exact expected install output. [Report](verification/VERIFICATION-2026-10-05-SCRIPT-TEXT.md). The interactive ValueLimit extension is [approved and implemented](decisions/SCRIPT-TEXT-LIMIT-PROPOSAL.md); full extension gates pass 495 suite executions and 128 installed default-X outcomes. [Limit report](verification/VERIFICATION-2026-10-05-SCRIPT-TEXT-LIMIT.md). |
| Interpreter remainder | Other widget index/text/geometry/font arithmetic, modal servicing and resource lifetime/budgets need work. | SCRIPT-01 remains open. Text/list and interactive text budgets plus checked geometry/font layout are qualified. The approved shared shadow-work limit and direct border iteration are qualified; Unicode/font-set measurement, modal servicing and broader lifetime/budgets remain. No complete interpreter-safety claim. |
| Final button-release processing | Escape with a button held stalled safe timers/module commands and used repeated pointer queries. | Approved common audited service, checked release masks, finite readiness and 20-ms monotonic pointer/cursor timing are locally qualified: eight profiles/924 entries, 120 installed identities, all default BSD X, optional 120/102 and five scoped sanitizer checks. Borrowed-state mutations and image publication stay deferred. Active Scroll and menu follow-ups are locally qualified in their separate frozen checkpoints; broader borrowed-resource lifetime work remains open. [Release report](verification/VERIFICATION-2026-10-06-BUTTON-RELEASE.md), [viewport report](verification/VERIFICATION-2026-10-06-VIEWPORT-SERVICE.md). |
| Module iteration and packet construction | Nested retirement could interrupt surviving iteration; construction used general variadic fields and temporary named storage. | Iterator lifetime, bounded fixed packet construction and corrected configure metadata are locally qualified: 1312 standard entries, 120 installed identities, 124 default BSD X drivers, optional 169/149 and 48 scoped sanitizer checks. Later typed-schema/constructor changes have separate source/evidence. [Transport report](verification/VERIFICATION-2026-10-07-MODULE-TRANSPORT.md). |
| ICE/session readiness admission | Constructors protected inheritance without checking fd_set range; library watches can also report borrowed descriptors. | Shared range/flag preparation, existing owner cleanup and bounded watched-record handling pass final normal/scoped 16+25 and both native BSD ARM64 unit/real-session controls. No old out-of-range readiness operation is executed. Next combined qualification and forced high live admission remain separate. [Admission report](verification/VERIFICATION-2026-10-07-FD-SELECT.md). |
| Descriptor-limit query | Unavailable/zero system queries propagated into the readiness/admission width; smaller limits were queried twice. | One checked query and finite FD_SETSIZE fallback pass focused normal/leak-enabled units, scoped actual-core controls and both native BSD ARM64 units. Argument growth/shared-library cleanup is also focused-qualified; the next combined batch remains pending. High-descriptor ownership/readiness remains open. [Width report](verification/VERIFICATION-2026-10-07-FD-WIDTH.md), [argument report](verification/VERIFICATION-2026-10-07-MODULE-ARGUMENTS.md). |
| Module fork refusal | Pre-start local pipes were not owned by the record destroyed on fork refusal. | Shared constructor cleanup closes the demonstrated four/two descriptor leak; actual commands, repeated refusal and real retry pass focused normal/scoped and both native BSD ARM64 controls. Schema1345 combined qualification passes: 1312 standard entries, 120 staged identities and 128 default BSD X drivers, with retained initial display-failure/recovery evidence. Other startup/process paths remain open. [Constructor report](verification/VERIFICATION-2026-10-07-MODULE-CONSTRUCTOR.md). |
| Window/resource lifetimes | Deferred commands retain events and module leases, but target resource lifetime still needs broader coverage. | INPUT-01 remains open. Existing interaction checks cover only the documented command set. |
| Image source acquisition | Ordinary primary FIFO image opens blocked the core; compressed XPM lacked a whole-operation deadline and expanded-byte cap. | Approved regular-file snapshots, 128 MiB source/expanded limits and five-second decompression are adopted. Seven full compiler legs pass 537 executions, staged Linux 192 image outcomes, default BSD X 232 image plus 40 FIFO outcomes, optional profiles 12 entries and scoped local ASan/UBSan 4/4 with leaks enabled. Opened-inode/cache identity, cursor ownership, XPM cleanup and unrelated BSD child notifications are verified. Storage latency and wider decoder CPU remain separate. [Accepted image report](verification/VERIFICATION-2026-10-05-IMAGE-INPUT.md). |
| SVG secondary resources | A regular SVG could block on library-owned secondary input; the decoder ran in the core. | Resource-limited isolated rendering and asynchronous normal-startup core completion are locally qualified. Acknowledged immutable image owners retain old images until matching success; bounded conversion/effects, decoded averages, cursor fallback and owner cleanup are included. Eight compiler legs pass 908 entries, with 120 staged code identities, 52 installed default-BSD-X driver invocations, 118/101 optional entries and 30 unique scoped sanitizer checks. All 1261 source hashes/modes match. Modules retain their existing completion path. Wider decoder/storage/permissions isolation and native release sanitizer CI remain open. [Activation report](verification/VERIFICATION-2026-10-06-IMAGE-ACTIVATION.md). |
| Script geometry | A 65536-pixel runtime widget width aborts an owned Script module; core service continues. | Approved checked geometry, font/layout preparation and popup refusal are implemented. Invalid runtime operations preserve prior state; zero/omitted widget startup sizing remains. Seven legs pass 586 suite executions, 105 staged identities, all default BSD X checks and seven installed font pixel comparisons. Three scoped local sanitizer checks pass; actual module leak checking is disabled and native release CI remains unrun. [Report](verification/VERIFICATION-2026-10-06-SCRIPT-GEOMETRY.md). |
| Font shadow work | A bounded production review observes 16640 Center positions at size 64; a loaded size-32768 font requests a square counter larger than signed int. The large iterator was not executed or rendered. | Approved 4096-position default with build override 1–65536, checked counting and direct border iteration are implemented. Shared consumer refusal/ownership passes across Linux and default BSD X: 600 full-suite executions, 105 staged identities and 77 installed consumer outcomes; scoped sanitizers and limit extremes pass. This bounds shadow positions, not total glyph/cache/library work. [Report](verification/VERIFICATION-2026-10-06-FONT-SHADOW.md). |
| Colorset shape cleanup | An ordinary opaque RGB PNG rejected as a shape released its acquired picture reference twice. | Duplicate release is fixed; the actual-core normal and ASan/UBSan fixtures pass uncached/shared-image refusal, valid shape/background reload and WindowList snapshot cleanup. Combined BSD/install qualification passes the local image activation checkpoint; broader audits remain open. [Report](verification/VERIFICATION-2026-10-06-SVG-CORE-ADAPTER.md). |
| Images and external data | Property bounds are implemented; tree walks, icon acquisition and decoder stress/isolation remain. | DATA-01 and related image work remain open. |
| Sanitizers and fuzzing | Local checks do not replace native Linux sanitizer CI. | Scoped local Linux ARM64 Clang ASan/UBSan passes nine SVG/image entries with leak detection; resource-limited worker/self-exec fixture exclusions and direct renderer coverage are explicit. Geometry checks also pass three scoped local checks; leak checking is enabled for pure/font probes and disabled for the actual Script/core/MFL run. Shared font-shadow checks also pass five scoped local entries with the same pure/probe versus actual-process leak-check distinction. Native release CI remains configured but unrun; this checkout has no remote. Full-project sanitizer/fuzz qualification remains open. The intermediate BSD interaction timing failure remains recorded separately. |

A platform message saying a response cannot be shown is separate from these
repository findings. The table records source review and test evidence, not a
claim that the project or user's machine was compromised.

Repeated ICE callback installation now preserves the earlier callback across
direct self-reinstallation. Focused normal/scoped 17+25 and both native BSD
ARM64 unit/library/core/session controls pass; combined descriptor qualification
remains pending. Broader lifecycle and third-party callback cycles remain separate.
[Retry report](verification/VERIFICATION-2026-10-07-ICE-RETRY.md).

The five-story descriptor checkpoint is locally qualified: 1336 standard
entries, 120 installed identities, 132 default BSD X drivers, optional 172/152
and 55 scoped checks. Original display/teardown failures and exact affected
recoveries remain retained. The next four-story client checkpoint has focused
passes for repeated authentication cleanup, interruption/termination-aware reads,
declared packet counts and independent OpenBSD nested-display ownership.
[Descriptor qualification](verification/VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md),
[client checkpoint](verification/VERIFICATION-2026-10-07-MODULE-CLIENTS.md).

Module write/cleanup preparation now retains partial frame progress and stops
subsequent output after an incomplete frame. A common termination-aware syscall
lets owned full-pipe controls unwind; FvwmRearrange cleanup occurs in normal flow.
Normal/scoped 3+3 and both native BSD ARM64 command/ten-case controls pass. Ordinary
blocking-write deadlines and other module handlers remain open; combined
producer1356 is locally qualified. [Write/cleanup evidence](verification/VERIFICATION-2026-10-07-MODULE-WRITES.md).

FvwmBacker now uses common termination and returns from EOF/configuration/packet
loops in normal flow. Eight actual-module controls pass normal/scoped and both
native BSD ARM64 overlays. This completes a five-story producer preparation
batch; its combined matrix is locally qualified. [Producer scope](verification/VERIFICATION-2026-10-07-MODULE-PRODUCERS.md).

Client1351 is locally qualified with 1352 standard entries/eight compiler profiles,
120 staged identities, 132 BSD default-X drivers, optional 174/154 and 57 scoped
checks. Original topology failures and the exact sanitizer-selection follow-up
remain retained. Cause of the topology failures remains unestablished; source
and deadlines were unchanged. Native release CI/physical checks remain separate.
[Final client evidence](verification/VERIFICATION-2026-10-07-MODULE-CLIENTS.md).

Producer1356 is locally qualified: 1376 standard entries/eight profiles, 120
installed identities, 140 BSD default-X drivers, optional 177/157 and 60 scoped
checks. Seven archives strictly validate; this matrix needed no affected reruns.
[Producer evidence](verification/VERIFICATION-2026-10-07-MODULE-PRODUCERS.md).

The next shared text batch bounds configuration views before exposing their
terminators and removes temporary command-list/request allocations. Normal and
scoped 3+3 and both native BSD ARM64 units pass. The action-expansion helper's
literal output copying and geometry arithmetic are the next owning audit before
combined qualification. [Focused text evidence](verification/VERIFICATION-2026-10-07-MODULE-TEXT.md).

Action output copying and geometry arithmetic are now checked in that shared
helper. The five-story text/action batch passes normal/scoped 4+3 and all three
native BSD ARM64 units; corrected 1358-file qualification now passes. Source-audit
findings were not executed against the unsafe predecessor. Initial freeze/fixture
failures and their exact snapshots remain retained.
[Action checkpoint](verification/VERIFICATION-2026-10-07-MODULE-ACTIONS.md).

The initial action matrix exposed an incorrect null-match request refusal in
the new cleanup. The repair supports the shipped colorset consumer, and its
affected actual-module check plus normal/scoped/native units pass. All six
initial suites and additional AMD64 image/paint deadlines remain retained;
those extra deadline causes are unestablished. Corrected full qualification passes
1392 standard entries, 120 installed identities, 140 BSD driver invocations,
optional 179/159 and 62 scoped checks; seven complete archives strictly validate.

Event now reuses the shared bounded reader and termination state, validates
action arguments/tails and handles builtin actions without a packet pointer.
Ident/Event short configuration lines advance; two unused quit functions are
removed. Normal/scoped 4+4 and BSD ARM64 ten Event/five Ident controls pass.
This separate 1360-file source is outside the current combined action checkpoint.
[Event/configuration evidence](verification/VERIFICATION-2026-10-07-MODULE-EVENTS.md).

Auto's fd_set owner now uses the existing cancellation-aware poll wrapper,
checked delay conversion and window-word admission. Normal/scoped 4+5 pass;
native FreeBSD ARM64 passes twelve owned-pipe cases including descriptor 4096.
OpenBSD ARM64 passes eleven cases; its inherited hard limit excludes that high
descriptor case and is preserved. Source1361 remains outside current combined
qualification. [Auto evidence](verification/VERIFICATION-2026-10-08-AUTO-READINESS.md).

Ident now validates declared native packet bodies before either consumer path
and uses the existing two-descriptor poll/termination contract for its popup.
Focused normal/scoped 4+6 and native BSD ARM64 configuration/popup controls pass;
full checkpoint qualification remains pending. High input descriptor 4096 passes
Linux/FreeBSD; the OpenBSD inherited-hard-limit gap is recorded.
[Ident scope](verification/VERIFICATION-2026-10-08-IDENT-TRANSPORT.md).

## Latest module qualification — 2026-10-08

[Consumer1361](verification/VERIFICATION-2026-10-08-MODULE-CONSUMERS.md) is locally
qualified: 1416 standard entries/eight profiles, 120 staged identities, 152 BSD
drivers, optional 182/162 and 65 scoped checks. Seven archives strictly validate;
no full-matrix failure or affected rerun. Earlier focused failures remain retained.
The six-story [Ident/Pager successor](verification/VERIFICATION-2026-10-08-POPUP-READINESS.md)
passes normal/scoped 4+7 and native BSD ARM64 controls; full qualification is next.
Its initial incomplete fixture failure is retained. High input FD coverage is
explicitly unavailable under OpenBSD's inherited hard limit. Ordinary transport
is still blocking; this does not close the descriptor/lifetime audit or native
release CI and physical/toolkit/compositor gaps.

IconMan's shared poll, tooltip timing and termination checks are now focused-
qualified in a separate successor. Buttons and borrowed-session snapshots pass
normal focused controls, including real XSMP; scoped/native checks are running.
Their [component](verification/VERIFICATION-2026-10-08-BUTTONS-READINESS.md) will
join [IconMan](verification/VERIFICATION-2026-10-08-ICONMAN-READINESS.md) before
full qualification. Initial fixture failures remain retained. FSM's session range
admission and Script's separate readiness owner remain explicit audit boundaries.

## Popup qualification — 2026-10-08

Ident/Pager [popup1363](verification/VERIFICATION-2026-10-08-POPUP-READINESS.md)
is locally qualified: 1432 standard entries, 120 identities, 160 BSD default-X
drivers, optional 184/164 and 67 scoped checks. Seven archives strictly validate;
no full-matrix failure or affected rerun. Component failures remain retained.
The next IconMan/Buttons batch is focused-qualified; Script readiness is joining
it before a full matrix dispatch. Release CI and physical coverage remain open.

## Script focused follow-up — 2026-10-08

Script's main/selection waits use shared poll, initialized monotonic periodic
timing and pending-session servicing even without a periodic block. The real
session test exposed empty SwallowExec teardown closing its own X connection;
explicit placeholder ownership repairs it and preserves external client teardown.
Final normal/scoped 5+16 and native BSD ARM64 controls pass; three complete archives
validate exact final source, runners and records. Initial fixture and compile
failures remain retained. These ten combined IconMan/Buttons/Script stories await
one wider checkpoint; focused results are not a whole-project security claim.
[Script report](verification/VERIFICATION-2026-10-08-SCRIPT-READINESS.md).


2026-10-08 readiness qualification: the exact 1366-file/SHA6439c478 source passes
1472 standard entries, 120 installed identities, 180 default BSD X driver
invocations, optional 189/169 and 76 scoped checks. Seven complete archives strictly
validate; no full-gate rerun or production change. This is local emulated coverage;
native release CI and physical checks remain separate. Subsequent Script packet
and Form timer/draw ownership changes are outside this qualified snapshot.
[Report](verification/VERIFICATION-2026-10-08-MODULE-READINESS.md).


2026-10-08 Form focus: ordinary timed input is now serviced through common poll and
nonblocking libevent countdown work. OpenBSD exposed a shared timeout draw-string
double free; failed exact-source evidence is retained before the borrowed-buffer
repair. Final normal/scoped 6+18 and native BSD ARM64 controls strictly validate
from complete archives. This source joins Script packet/widget cleanup in one
running successor qualification, and is not yet fully qualified. Remaining Form
text/parser and partial-read ownership are still open. [Form evidence](verification/VERIFICATION-2026-10-08-FORM-READINESS.md).


2026-10-08 Form input focus: declared packet views, empty lines, owned string copies
and timeout prepare-before-publish now pass final normal/scoped 7+19 and native
BSD ARM64 units/actual controls. Complete focused archives strictly validate;
intermediate translation API compiler failures are retained. This is outside
running module-ui1370 qualification, whose original runtime/storage failures also
remain retained. No claim closes all Form parser/geometry or process owners.
[Input evidence](verification/VERIFICATION-2026-10-08-FORM-INPUT.md).


2026-10-08 harness focus: outer SIGTERM now unwinds main-thread runtime cleanup through existing finite child teardown and restores signal ownership. Ordinary baseline and final normal/scoped/native BSD ARM64 controls are retained; the OpenBSD observer restriction is corrected in the fixture. Exact focused archives validate, with explicit hard-SIGKILL/thread/foreign-handler limits. This is verification infrastructure, not a whole-project process containment claim. [Evidence](verification/VERIFICATION-2026-10-08-HARNESS-TEARDOWN.md).


2026-10-08 module-ui qualification: exact1370 source passes required 1496/120/188, optional192/172 and scoped79. Seven archives strictly validate; initial failures and three-pass unchanged affected recovery evidence remain retained. The new Form/harness source is outside this accepted snapshot. Native release CI and physical coverage remain separate. [Report](verification/VERIFICATION-2026-10-08-MODULE-UI.md).


2026-10-08 Form argument focus: source-bound key preparation, terminator-aware cursors and field-sized text copies pass final normal/scoped/native BSD ARM64 controls. Exact focused source/payload/runners validate; the ordinary escaped-label baseline is retained. Geometry, allocation growth, expansion and blocking transport remain open; this is not a whole Form parser/security closure. [Evidence](verification/VERIFICATION-2026-10-08-FORM-ARGUMENTS.md).


2026-10-08 Form data focus: required filename/prefix views are prepared before owner replacement; incomplete directives preserve previous prepared state. Empty variable names release their copy, and full requests use the existing checked formatter/module writer. Normal/scoped/native BSD ARM64 evidence strictly validates; physical file acquisition and all remaining parser/geometry/expansion owners are separate. [Evidence](verification/VERIFICATION-2026-10-08-FORM-DATA.md).


2026-10-08 Form/harness qualification: exact source48ec passes required1520/120/192, optional195/175 and scoped81 with strict seven-archive validation. Initial capacity and menu-release observations and exact recoveries remain retained; menu-release cause is unestablished. The separate five-story argument/data checkpoint0375 is dispatched, with no configuration migration. Remaining Form arithmetic/array/expansion/partial-read owners and native release CI are open. [Qualification](verification/VERIFICATION-2026-10-08-HARNESS-TEARDOWN.md), [successor](verification/VERIFICATION-2026-10-08-FORM-CHECKPOINT.md).

2026-10-08 Form arrays: counted storage checks signed capacity/element limits before geometric growth and passes element count/size once. Normal/scoped/native BSD ARM64 archives validate; original fixture mapping observation remains retained. This does not close layout sums, editing growth, expansion or partial reads. [Evidence](verification/VERIFICATION-2026-10-08-FORM-ARRAYS.md).


2026-10-08 Form Input: positive representable widths and X11 pixel extents prepare before item publication, preserving valid fields around refused directives. Exact normal/scoped/native BSD ARM64 focus and manual paragraph checks validate. Five storage/dimension stories await one combined gate; complete layout, command expansion, editing growth and partial-read work remain open. [Evidence](verification/VERIFICATION-2026-10-08-FORM-GEOMETRY.md).


2026-10-08 argument/data qualification: exact0375 passes required1544/120/204, optional198/178 and scoped84 with strict seven-archive validation and no combined failure/rerun. Subsequent checked Form storage, Input and command/cancellation preparation share one nine-story1375 checkpoint. Final affected and native command evidence validates; existing substitution bytes remain. Recursion, history/editing growth, shell and partial reads stay open. [Qualified](verification/VERIFICATION-2026-10-08-FORM-CHECKPOINT.md), [processing](verification/VERIFICATION-2026-10-08-FORM-PROCESSING.md).

2026-10-08 Form editing: four history/storage/count stories are focused-qualified (normal/scoped and native BSD ARM64, strict complete archives). A fifth X-service story runs focused controls after retaining a count-only timer-lag observation. Configuration syntax remains; whole Unicode/layout, recursive expansion, synchronous callbacks and partial packet owners remain open. [History](verification/VERIFICATION-2026-10-08-FORM-HISTORY.md), [service](verification/VERIFICATION-2026-10-08-FORM-SERVICE.md).

Processingba7b is locally qualified at1576/120/216, optional202/182 and
scoped88 without a combined failure/rerun. The five-story editing/service3159
focus passes normal/scoped and native BSD ARM64 controls; one successor matrix
follows. [Processing](verification/VERIFICATION-2026-10-08-FORM-PROCESSING.md),
[editing/service](verification/VERIFICATION-2026-10-08-FORM-SERVICE.md).

The shared runtime packet reader and Form/Script ownership are focused-qualified at
source49a5. Partial native header/body progress yields to UI, timers and session
work; declared lengths precede body arithmetic, with cancellation and bounded
read attempts. Complete Linux normal/scoped and BSD ARM64 payloads validate.
Buttons/Pager follow-up is being verified separately; other consumers, startup
acquisition and synchronous configured callbacks remain open.
[Reader focus](verification/VERIFICATION-2026-10-08-MODULE-READER.md).

The five-story reader successor sourcec249 is strictly focused-qualified for
Form, Script, Buttons and Pager. Actual UI/session controls permit progress while
header/body fragments are pending, then verify completion and partial-input exit.
One combined gate remains pending; continuous X flooding and configured blocking
callbacks remain outside this result.
[Scope](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

Editing3159 is strictly locally qualified at1592/120/224, optional204/184 and
scoped90, with all seven archives checked and no combined failure/rerun. Reader
sourcec249 follows as one separate five-story checkpoint.
[Editing evidence](verification/VERIFICATION-2026-10-08-FORM-EDITING.md).

The remaining IconMan/Ident UI-reader source d21f is separately focused-qualified.
IconMan's reproduced missing-display-text drawing assertion now uses owned empty
rendering until the later name is present, preserving stored metadata. Normal/
scoped and native BSD ARM64 controls and complete payloads validate. Ident target
reselection, startup and continuous-X service remain outside this result; the
combined successor remains pending.
[Evidence](verification/VERIFICATION-2026-10-08-REMAINING-UI-READER.md).

Runtime-reader sourcec249 is strictly locally qualified at1632/120/240,
optional209/189 and scoped95, with complete final archives and no combined
failure/rerun/source change. Native release CI remains separate.
See [qualified runtime reader](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

The five-story UI service sourcecf44 is strictly focused-qualified: Linux
normal/scoped2 C +15 actual per profile, BSD ARM642 C +15 actual, exact174/153/165
payloads and fourteen native consumed sources. Bounded X/coalescing service
removes reproduced pipe starvation in four UI owners. Nested drag/lifecycle
waits and configured blocking callbacks remain open. Combined qualification
and native release CI are separate; [scope](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).

Pager cleanup sourcef641 is strictly focused-qualified at normal/scoped Linux
4 C +8 actual and BSD ARM644 C +8 actual, exact80/92/99 payloads and thirteen
consumed sources. Declared bodies, fixed restack vectors, temporary tokens and
DesktopSize preparation have dedicated controls. Initial compile failure is
retained. Monitor-line/DesktopConfiguration numeric parsing, nested waits and
other consumers remain open; combined qualification is pending. See the
[scope](verification/VERIFICATION-2026-10-08-PAGER-PACKET.md).

Remaining live-UI reader sourced21f is strictly locally qualified at1648 standard
eight-profile entries,120 installed identities,248 default-X drivers,
optional211/191 and scoped97. Seven final archives validate exact source/runners;
no combined failure/rerun/source change. Separate sourcecf44 five-story X-service
qualification is dispatched; required1688/120/264 optional216/196 scoped102 remain
requirements. [Reader report](verification/VERIFICATION-2026-10-08-REMAINING-UI-READER.md).

Pager source7284 adds checked monitor records, token release on all branches and
absent-monitor/highlight guards. Normal/scoped4 C +8 actual and native BSD ARM64
focused controls pass; complete80/96/105 payloads validate. One eight-story Pager
owner checkpoint combines these with the undispatched sourcef641 batch; full
qualification remains pending. Drag-entry guards are audited, not active-drag
cancellation coverage. [Scope](verification/VERIFICATION-2026-10-08-PAGER-MONITOR.md).

Sourcee717 guards Buttons/IconMan runtime field/string access with owner-specific
admission and one shared declared-text helper. Synchronous refusal still unlocks
IconMan's producer. Normal/scoped5 C +10 actual and both native BSD ARM64 overlays
pass; complete92/98/107 payloads validate. The initial ignored-type fixture is
retained and corrected without production/deadline changes. Full qualification
remains pending; [scope](verification/VERIFICATION-2026-10-08-UI-PACKET.md).

Canonical source3c37 removes duplicate text checks in Pager/Script/Ident and
includes the focused-qualified Buttons/IconMan guards. Normal/scoped6 C +14 actual
and both native BSD ARM64 overlays pass; complete152/123/135 payloads validate
eight directly consumed files, fourteen reused object/executable identities and
the1382-file donor. Full qualification remains pending; [scope](verification/VERIFICATION-2026-10-08-UI-TEXT.md).


Shared X-service sourcecf44 is strictly locally qualified at1688/eight-profile
standard entries,120 installed identities,264 BSD default-X invocations,
optional216/196 and scoped102. Seven complete archives validate; original startup
timing failure and three unchanged affected passes remain retained, cause unknown.
One eight-story Pager7284 qualification is running; current monitor protocol,
selection and Rearrange work-area edits remain unverified development work.
[Qualified service scope](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).


Monitor sourcee761 is strictly focused-qualified at normal/scoped Linux and
native BSD ARM648 C +17 actual per profile. Complete192/133/148 payloads validate
all1398 source hashes/modes, exact runners and native binary records. Core quoted
publication/refusal, Rearrange atomic area preparation and disabled Pager selection
have owned controls. Initial build-input/header failures remain retained; full
qualification and physical/default-server scope remain pending.
[Monitor scope](verification/VERIFICATION-2026-10-08-MODULE-MONITOR.md).


Source30be's monitor lookup and checked UI numeric fields are strictly
focused-qualified: normal/scoped Linux and both BSD ARM648 C +17 actual controls
pass;193/99/114 complete payloads validate. An actual Pager startup control
reproduced null-member offset UB; standard offsetof/size_t repairs all owning
style call sites. Failed source/logs remain retained. These four stories and the
four monitor stories share one undispatched eight-story checkpoint; no separate
e761 extensive gate. Desk-range limits require a configuration decision, while
startup defaults and other geometry remain open.
[Owner scope](verification/VERIFICATION-2026-10-08-UI-NUMERIC.md).

Pager startup follow-up: the approved4096/65536 desk allowance is implemented. Safe initial page geometry, wide desk-index arithmetic and a reproduced high-ID style lookup repair join one four-story focused batch. Normal/scoped Linux and native BSD ARM64 focus is strictly qualified, including the65536 override and restored identical default binary. Extensive qualification is pending; no broader closure claim. See [Pager startup](verification/VERIFICATION-2026-10-08-PAGER-STARTUP.md).

Shared numeric parser follow-up is focused-qualified: normal/scoped Linux and
native BSD ARM64 controls validate checked int conversion, rectangle preparation
and percentage unit/token ownership. The actual old-library baseline accepted
out-of-range values through four APIs and remains retained. Full qualification
and unrelated numeric/pixel arithmetic remain open. See
[parser evidence](verification/VERIFICATION-2026-10-08-PARSE-INTEGER.md).

## Font/modal service checkpoint — 2026-10-11

The [font/modal checkpoint](verification/VERIFICATION-2026-10-11-FONT-MODAL.md)
is strictly locally qualified across Linux/FreeBSD/OpenBSD AMD64/ARM64, including
default BSD X servers and scoped local sanitizers. Script modal and selection
waits borrow one bounded readiness owner; explicit widget resource revisions
prevent pointer identity from establishing unchanged font/title lifetime. UTF-8
scalar offsets, core conversion and long font-set measurement use checked
progress and arithmetic. Configuration files and dependencies are unchanged.
The address-reuse concern is an audit safeguard, not a reproduced failure.
Broader font resource budgets, shaping/IME and native release CI/physical checks
remain separate; this checkpoint does not close those owners.

The [2026-10-12 CI profile/default-X checkpoint](verification/VERIFICATION-2026-10-12-CI-PROFILES.md) qualifies optional offline Go client gates and authenticated local BSD default servers, including four cookie-less client refusals. Thirteen selected asset downloads match published hashes; the provider does not yet enforce these fixed identities before execution/cache reuse. Candidate framebuffer differences, inherited container contents and actual native release CI remain open. This is an affected local qualification, not a completed transitive-dependency review.
