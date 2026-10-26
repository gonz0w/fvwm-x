# GitHub roadmap index

Library evaluations approved 2026-10-25 reuse #48/#54/#211/#46/#212/#81/#66/
#74/#77, with cost/profile coordination in #226. No duplicate issue or stable ID
is created. Existing status, priority, stage, milestone, Work batch and native
relationships remain unchanged. The [owner/comparison map](decisions/LIBRARY-EVALUATIONS-2026-10-25.md)
requires simplicity, measured performance and complete dependency costs across
Linux/FreeBSD/OpenBSD AMD64/ARM64; the tracker retains execution authority.

Hardening scope approved 2026-10-25: six P2 Backlog additions below mirror the
[tracker](PROJECT-TRACKER.md#portable-and-openbsd-hardening-issue-scope--2026-10-25)
and [scope record](decisions/PORTABLE-OPENBSD-HARDENING-2026-10-25.md). #232–#234
are native children of #41; #235–#236 are native children of #5. D3 owns scoped
worker policy and Qualification owns the verification additions. Existing
maintenance pickup, statuses and qualification evidence remain separately owned.

Current release authority, 2026-10-26: the [three-release plan](decisions/RELEASE-PLAN-2026-10-26.md)
and [finite contracts](decisions/RELEASE-CONTRACTS-2026-10-26.json) assign every
open owner to R1/R2/R3 and Major/Minor breadth. R1 includes all currently planned
features and evaluations; R2/R3 hold explicitly deferred maintenance. Milestones
are releases; Stage below is technical ownership, not the release target.
Old maintenance-first/modernization scheduling is historical. Closed components
remain closed, and current statuses remain in the authoritative tracker/mirror.

Stable ID mapping imported on 2026-10-13. This is not a second live backlog.
The [tracker](PROJECT-TRACKER.md#execution-order) owns scope/execution;
[GitHub issues](https://github.com/gonz0w/fvwm-x/issues) and the
[Project](https://github.com/users/gonz0w/projects/6) mirror review status manually.
Update mappings when IDs or owning stages change. See [workflow](GITHUB-WORKFLOW.md).

| Stage | Tracker ID | Issue |
| --- | --- | --- |
| 05 | HARDEN-01 | [#231 — Publish the R1 hardening evidence matrix](https://github.com/gonz0w/fvwm-x/issues/231) |
| 05 | BOX-02 | [#232 — Sandbox OpenBSD image workers with pledge and locked unveil](https://github.com/gonz0w/fvwm-x/issues/232) |
| 05 | BOX-03 | [#233 — Constrain FreeBSD image workers with Capsicum descriptor rights](https://github.com/gonz0w/fvwm-x/issues/233) |
| 05 | BOX-04 | [#234 — Restrict Linux image workers with detected syscall and filesystem controls](https://github.com/gonz0w/fvwm-x/issues/234) |
| 01 | VERIFY-02 | [#235 — Verify installed R1 C and Go binary protections](https://github.com/gonz0w/fvwm-x/issues/235) |
| 01 | VERIFY-03 | [#236 — Qualify the R2 OpenBSD allocator profile](https://github.com/gonz0w/fvwm-x/issues/236) |
| 01 | IPC-01 | [#2 — Qualify the original module/IPC correctness repair](https://github.com/gonz0w/fvwm-x/issues/2) |
| 01 | GEO-01 | [#1 — Qualify the original geometry correctness repair](https://github.com/gonz0w/fvwm-x/issues/1) |
| 01 | SCRIPT-01 | [#4 — Close the R2 Script resource and unwind manifest](https://github.com/gonz0w/fvwm-x/issues/4) |
| 01 | VERIFY-01 | [#5 — Qualify the frozen R1 candidate](https://github.com/gonz0w/fvwm-x/issues/5) |
| 01 | COMPAT-01 | [#3 — Qualify the R1 configuration corpus](https://github.com/gonz0w/fvwm-x/issues/3) |
| 02 | LIB-02 | [#11 — Verify the R1 image and transitive dependency closure](https://github.com/gonz0w/fvwm-x/issues/11) |
| 02 | LIB-07 | [#13 — Verify the R1 X11 and font dependency manifest](https://github.com/gonz0w/fvwm-x/issues/13) |
| 02 | LIB-01 | [#10 — Qualify JSON import and encoding boundaries](https://github.com/gonz0w/fvwm-x/issues/10) |
| 02 | LIB-03 | [#12 — Review optional Go dependency/toolchain reachability](https://github.com/gonz0w/fvwm-x/issues/12) |
| 02 | MAINT-02 | [#14 — Finish the R2 adopted-tool manifest](https://github.com/gonz0w/fvwm-x/issues/14) |
| 02 | DATA-01 | [#6 — Close the R2 checked-input ownership manifest](https://github.com/gonz0w/fvwm-x/issues/6) |
| 02 | IMAGE-01 | [#8 — Close the R2 image-worker boundary manifest](https://github.com/gonz0w/fvwm-x/issues/8) |
| 02 | FD-01 | [#7 — Close the R2 descriptor-constructor manifest](https://github.com/gonz0w/fvwm-x/issues/7) |
| 02 | PROC-01 | [#15 — Close the R2 helper-process constructor manifest](https://github.com/gonz0w/fvwm-x/issues/15) |
| 02 | INPUT-01 | [#9 — Close the R2 interaction-lifetime manifest](https://github.com/gonz0w/fvwm-x/issues/9) |
| 02 | STANDARDS-01 | [#16 — Qualify the R1 adopted requirements matrix](https://github.com/gonz0w/fvwm-x/issues/16) |
| 03 | PERL-PROTO-01 | [#20 — Qualify native Perl packet metadata and transport repair](https://github.com/gonz0w/fvwm-x/issues/20) |
| 03 | PROMPT-01 | [#21 — Qualify Prompt/Command transport and installed clients](https://github.com/gonz0w/fvwm-x/issues/21) |
| 03 | CI-01 | [#17 — Publish the R1 local verification and review recipe](https://github.com/gonz0w/fvwm-x/issues/17) |
| 03 | UTIL-01 | [#24 — Qualify standalone utility and XDG provider ports](https://github.com/gonz0w/fvwm-x/issues/24) |
| 04 | QUEUE-01 | [#30 — Qualify constant-time queue drain ownership](https://github.com/gonz0w/fvwm-x/issues/30) |
| 04 | OUTPUT-01 | [#26 — Qualify bounded asynchronous module output](https://github.com/gonz0w/fvwm-x/issues/26) |
| 04 | OUTPUT-02 | [#27 — Qualify one total synchronous-output deadline](https://github.com/gonz0w/fvwm-x/issues/27) |
| 04 | OUTPUT-03 | [#28 — Qualify bounded synchronous module startup](https://github.com/gonz0w/fvwm-x/issues/28) |
| 04 | PERF-01 | [#29 — Freeze and enforce R1 speed and memory budgets](https://github.com/gonz0w/fvwm-x/issues/29) |
| 04 | DRAW-01 | [#25 — Decide the R2 dirty-state experiment](https://github.com/gonz0w/fvwm-x/issues/25) |
| 04 | STACK-02 | [#32 — Qualify per-window stacking state publication](https://github.com/gonz0w/fvwm-x/issues/32) |
| 04 | STACK-01 | [#31 — Complete stacking identity, walks and retirement work](https://github.com/gonz0w/fvwm-x/issues/31) |
| 04 | CLEAN-01 | [#34 — Close the R2 core/module ownership manifest](https://github.com/gonz0w/fvwm-x/issues/34) |
| 04 | DEAD-01 | [#35 — Retire verified unused code and fallbacks](https://github.com/gonz0w/fvwm-x/issues/35) |
| 04 | STYLE-01 | [#38 — Decide the R2 style-application optimization](https://github.com/gonz0w/fvwm-x/issues/38) |
| 04 | STATE-01 | [#37 — Decide the R2 state-burst optimization](https://github.com/gonz0w/fvwm-x/issues/37) |
| 04 | CACHE-01 | [#33 — Decide the R2 cache-cost experiment](https://github.com/gonz0w/fvwm-x/issues/33) |
| 04 | LOOP-01 | [#36 — Consolidate core readiness and event-loop servicing](https://github.com/gonz0w/fvwm-x/issues/36) |
| 04 | XIO-01 | [#39 — Decide the R2 X-query and barrier experiment](https://github.com/gonz0w/fvwm-x/issues/39) |
| 05 | MONITOR-01 | [#19 — Qualify R1 hotplug, monitor roles and scale behavior](https://github.com/gonz0w/fvwm-x/issues/19) |
| 05 | EWMH-01 | [#18 — Finish the R1 ICCCM/EWMH conformance rows](https://github.com/gonz0w/fvwm-x/issues/18) |
| 05 | SELECTION-01 | [#22 — Qualify R1 selection transfer contracts](https://github.com/gonz0w/fvwm-x/issues/22) |
| 05 | MODULE-SCALE-01 | [#51 — Qualify R1 native module scaling](https://github.com/gonz0w/fvwm-x/issues/51) |
| 05 | BOX-01 | [#41 — Accept R2 per-mode worker capability policies](https://github.com/gonz0w/fvwm-x/issues/41) |
| 05 | LIB-05 | [#47 — Review library boundaries and duplication](https://github.com/gonz0w/fvwm-x/issues/47) |
| 05 | LINK-01 | [#49 — Declare target-owned dependencies and qualify reduced profiles](https://github.com/gonz0w/fvwm-x/issues/49) |
| 05 | LIB-04 | [#46 — Decide LunaSVG versus the R1 SVG baseline](https://github.com/gonz0w/fvwm-x/issues/46) |
| 05 | LIB-06 | [#48 — Decide the R1 status serializer](https://github.com/gonz0w/fvwm-x/issues/48) |
| 05 | COMPAT-02 | [#42 — Retire the R2 Perl runtime framework](https://github.com/gonz0w/fvwm-x/issues/42) |
| 05 | RUNTIME-GO-01 | [#23 — Require Go and complete the Python-free runtime](https://github.com/gonz0w/fvwm-x/issues/23) |
| 05 | LUA-01 | [#50 — Ship R1 isolated Lua and migration examples](https://github.com/gonz0w/fvwm-x/issues/50) |
| 05 | TOOLING-GO-01 | [#55 — Retire the R3 first-party Python and Perl helper manifest](https://github.com/gonz0w/fvwm-x/issues/55) |
| 05 | GEN-01 | [#44 — Qualify modern parser/scanner generation](https://github.com/gonz0w/fvwm-x/issues/44) |
| 05 | PORT-01 | [#52 — Qualify the R1 Linux/BSD package manifest](https://github.com/gonz0w/fvwm-x/issues/52) |
| 05 | I18N-01 | [#45 — Finish R1 source/catalog coverage](https://github.com/gonz0w/fvwm-x/issues/45) |
| 05 | REPRO-01 | [#53 — Qualify reproducible offline R1 source builds](https://github.com/gonz0w/fvwm-x/issues/53) |
| 05 | DOCS-01 | [#43 — Qualify installed R1 manuals and examples](https://github.com/gonz0w/fvwm-x/issues/43) |
| 05 | ARCHIVE-01 | [#40 — Retire the qualified R2 predecessor manifest](https://github.com/gonz0w/fvwm-x/issues/40) |
| 05 | XDG-01 | [#56 — Complete desktop/application interoperability](https://github.com/gonz0w/fvwm-x/issues/56) |
| 05 | TEXT-01 | [#54 — Establish native text, shaping and input contracts](https://github.com/gonz0w/fvwm-x/issues/54) |
| 06 | BUTTON-01 | [#58 — Add native button keyboard focus and navigation](https://github.com/gonz0w/fvwm-x/issues/58) |
| 06 | BUTTON-02 | [#59 — Add bounded atomic dynamic button updates](https://github.com/gonz0w/fvwm-x/issues/59) |
| 06 | CFG-01 | [#60 — Build typed settings and native configuration UI](https://github.com/gonz0w/fvwm-x/issues/60) |
| 06 | CFG-02 | [#61 — Add non-executing configuration diagnostics](https://github.com/gonz0w/fvwm-x/issues/61) |
| 06 | IPC-02 | [#70 — Add structured state queries and operation results](https://github.com/gonz0w/fvwm-x/issues/70) |
| 06 | SESSION-01 | [#72 — Build the portable default session foundation](https://github.com/gonz0w/fvwm-x/issues/72) |
| 06 | FILE-VIEW-01 | [#69 — Build a configurable native file browser view](https://github.com/gonz0w/fvwm-x/issues/69) |
| 06 | FILE-DATA-01 | [#66 — Build reusable directory and collection data](https://github.com/gonz0w/fvwm-x/issues/66) |
| 06 | FILE-ACTION-01 | [#65 — Build typed file and collection actions](https://github.com/gonz0w/fvwm-x/issues/65) |
| 06 | FILE-JOBS-01 | [#68 — Add reliable bounded file-operation jobs](https://github.com/gonz0w/fvwm-x/issues/68) |
| 06 | DESKTOP-ICONS-01 | [#64 — Add an optional desktop pinboard](https://github.com/gonz0w/fvwm-x/issues/64) |
| 06 | FILE-INTEGRATION-01 | [#67 — Add portable file-manager interoperability](https://github.com/gonz0w/fvwm-x/issues/67) |
| 06 | CONTENT-01 | [#63 — Build typed clipboard content and transfer primitives](https://github.com/gonz0w/fvwm-x/issues/63) |
| 06 | RETENTION-01 | [#71 — Define bounded clipboard retention and history](https://github.com/gonz0w/fvwm-x/issues/71) |
| 06 | STORE-01 | [#74 — Review protected clipboard storage and defaults](https://github.com/gonz0w/fvwm-x/issues/74) |
| 06 | CLIPBOARD-01 | [#62 — Deliver text/image clipboard consumer handoff](https://github.com/gonz0w/fvwm-x/issues/62) |
| 06 | SETTINGS-01 | [#73 — Add a shared native preference bridge](https://github.com/gonz0w/fvwm-x/issues/73) |
| 06 | A11Y-01 | [#57 — Improve native keyboard and accessibility contracts](https://github.com/gonz0w/fvwm-x/issues/57) |
| 06 | THEME-01 | [#75 — Qualify shipped theme roles, states and interaction](https://github.com/gonz0w/fvwm-x/issues/75) |
| 07 | SESSION-02 | [#77 — Extend the portable session with optional bus integration](https://github.com/gonz0w/fvwm-x/issues/77) |
| 07 | NOTIFY-01 | [#76 — Add isolated optional native notifications](https://github.com/gonz0w/fvwm-x/issues/76) |
| 07 | TRAY-01 | [#78 — Add native tray protocol integration](https://github.com/gonz0w/fvwm-x/issues/78) |
| 08 | RELEASE-01 | [#79 — Accept the R1 Configurable Desktop Lite candidate](https://github.com/gonz0w/fvwm-x/issues/79) |
| 09 | I18N-02 | [#80 — Complete R1 localization for all eleven existing languages](https://github.com/gonz0w/fvwm-x/issues/80) |
| 06 | FILE-ADVANCED-01 | [#81 — Decide and deliver accepted R1 file-manager candidates](https://github.com/gonz0w/fvwm-x/issues/81) |

The bounded [repository-foundation review](https://github.com/gonz0w/fvwm-x/issues/82)
is a child of CI-01. Twelve imported issues backfill accepted local components;
they do not close broader release, hardware or remaining audit gates.

MAINT-02 has two stage2 children consolidated into one source-tool PR:
[#84 — Bound source inventory and archive memory](https://github.com/gonz0w/fvwm-x/issues/84)
and [#85 — Record dependency inventory provenance and Go directives](https://github.com/gonz0w/fvwm-x/issues/85).
Their limited acceptance does not close MAINT-02 or the LIB-03 audit.

[MAINT-02 #87 — Incremental build reuse and consolidated PR checkpoints](https://github.com/gonz0w/fvwm-x/issues/87)
groups build-step and review-cadence improvements into one stage 2 child batch.

[MAINT-02 #89 — Restore referenced developer docs and check local links](https://github.com/gonz0w/fvwm-x/issues/89)
groups frozen documentation/evidence, portable navigation and local checks in
one stage 2 checkpoint.

[LIB-03 #91 — Audit target reachability and trim unused Go vendor inputs](https://github.com/gonz0w/fvwm-x/issues/91)
groups advisory analysis, same-pin vendor regeneration, the approved compiler/SDK
policy and offline native checks.
The approved patched optional compiler/SDK policy and affected qualification
are included in the same PR under LIB-03 #12.

[LIB-07 #93 — Investigate intermittent OpenBSD AMD64 VESA startup crashes](https://github.com/gonz0w/fvwm-x/issues/93)
retains two pre-client server faults separately from the passing Go application
controls. Cause is unestablished. The user deferred this investigation until
much later on 2026-10-14; preserve it open while runtime backlog work resumes.

[MAINT-02/CLEAN-01 PR #107](https://github.com/gonz0w/fvwm-x/pull/107)
interleaves production core object reuse and narrow style dependency boundaries.
Its [affected checkpoint](verification/VERIFICATION-2026-10-18-DEVELOPMENT-BUILD-REUSE.md)
retains the separate assertion-enabled build and all test registrations; the broad
owners #14/#34 and release gates remain open.

STYLE-01 #38's [style index retention checkpoint](verification/VERIFICATION-2026-10-19-STYLE-INDEX-RETENTION.md)
follows merged PR119 with live-domain counts, empty-table release and typed
category flags. One combined checkpoint serves all three improvements; required
OpenBSD CI/SVG/install and physical/release gaps remain. STYLE-01 stays open.

MAINT-02 #14 / CLEAN-01 #34: the [command/update dependency checkpoint](verification/VERIFICATION-2026-10-19-COMMAND-TYPE-BOUNDARIES.md) is merged in [PR #121](https://github.com/gonz0w/fvwm-x/pull/121). Three layouts compile six/four/four objects; 100 affected native entries and exact allocated-section parity pass. Broader owners and retained runtime gaps stay open.

STATE-01 #37: [EWMH publication work](verification/VERIFICATION-2026-10-19-EWMH-PUBLICATION.md) is merged in [PR #122](https://github.com/gonz0w/fvwm-x/pull/122). Three related improvements share one combined checkpoint. Broader state-burst work, native-CPU/hardware/release checks and required OpenBSD gaps remain open.

MAINT-02 #14 / CLEAN-01 #34: [font/picture/monitor boundaries](verification/VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES.md) are merged in [PR #123](https://github.com/gonz0w/fvwm-x/pull/123). ARM GCC layout fanout falls 252/289/226→49/66/92; 170 affected entries and exact native code/data parity pass. PR122 manual review is closed on request. Broad owners and required OpenBSD/release gaps stay open.

STATE-01 #37 / CLEAN-01 #34: [existing legacy tray membership](verification/VERIFICATION-2026-10-19-EWMH-TRAY.md) is merged in [PR #124](https://github.com/gonz0w/fvwm-x/pull/124). Three related insertion/retirement/publication improvements share one combined checkpoint. New TRAY-01 features remain later-stage work; broad owners and required OpenBSD/native-CPU/hardware/release gaps remain open.

STYLE-01 #38 / CLEAN-01 #34: [style matching work](verification/VERIFICATION-2026-10-19-STYLE-MATCH-WORK.md) is merged in [PR #125](https://github.com/gonz0w/fvwm-x/pull/125). Selector preparation, universal explicit predicates and ordered candidate merging share one combined checkpoint. Eight focused entries are retained by exact source/binary identity; larger plans/grabs, broad owners and required OpenBSD/native-CPU/hardware/release gaps remain open.

STYLE-01 #38 / CLEAN-01 #34: [style flag predicates](verification/VERIFICATION-2026-10-19-STYLE-FLAGS.md) is merged in [PR #126](https://github.com/gonz0w/fvwm-x/pull/126). Subset/intersection/masked equality share one checkpoint; nine focused results are retained by exact source/eleven binary identities. Broader owners, update/grab work and required OpenBSD/native-CPU/hardware/release gaps remain open.

STYLE-01 #38 / CLEAN-01 #34: [style flag mutations](verification/VERIFICATION-2026-10-20-STYLE-FLAG-MUTATIONS.md) is merged in [PR #127](https://github.com/gonz0w/fvwm-x/pull/127). Typed union/intersection/clearing share one checkpoint; ten focused results are retained by exact source/twelve binary identities. Broader owners, plans/grabs and required OpenBSD/native-CPU/hardware/release gaps remain open.

MAINT-02 #14 / CLEAN-01 #34: [private condition/startup/monitor record boundaries](verification/VERIFICATION-2026-10-20-PRIVATE-RECORD-BOUNDARIES.md) are merged in [PR #128](https://github.com/gonz0w/fvwm-x/pull/128). Three condition/startup/monitor-adapter dependency improvements pass 149/149 affected native entries across six targets/ten profiles (15 retained focused / 134 fresh). All 5914 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 214/214/230 to 2/22/2 objects. Broader PR127 runtime evidence and required OpenBSD gaps remain source-scoped. MAINT-02/STYLE/STATE are Ready and broad owners stay open.

CACHE-01 #33 / CLEAN-01 #34: [picture-cache lookup and retirement](verification/VERIFICATION-2026-10-20-PICTURE-CACHE-WORK.md) is merged in [PR #129](https://github.com/gonz0w/fvwm-x/pull/129). Three picture-cache variant/name/retirement improvements pass one combined checkpoint: 2045/2045 standard results (2038 fresh / seven retained by exact source and nine binary identities), plus 451/451 optional entries across six targets/ten profiles. Selected pathname comparisons fall 65 to one/two; two predecessor walks fall 16,384/32,385 to zero. Five scoped guest sanitizers, nine fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. CACHE-01 is Ready; broad owners remain open.

DATA-01 #6 / IMAGE-01 #8 / CLEAN-01 #34: [donated icon retention](verification/VERIFICATION-2026-10-20-EWMH-ICON-RETENTION.md) is merged in [PR #130](https://github.com/gonz0w/fvwm-x/pull/130). Three donated-icon record/size/retirement improvements pass one combined checkpoint: 2045/2045 standard results (2037 fresh / eight retained by unchanged production/test/build inputs and nine binary identities), plus 451/451 optional entries across six targets/ten profiles. Selected partial deletion allocations fall three to two. Fifteen actual-core drivers verify 510 icon cases; four scoped guest sanitizers, nine fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. DATA-01 is in Review, IMAGE-01 retains Validation for broader gaps and CLEAN-01 remains In progress.

DATA-01 #6 / CLEAN-01 #34: [window-tree walks](verification/VERIFICATION-2026-10-20-WINDOW-TREE-WALKS.md) is merged in [PR #131](https://github.com/gonz0w/fvwm-x/pull/131). Three ancestor/pointer/frame lookup improvements pass one combined checkpoint: 2061/2061 standard results (2055 fresh / six retained by unchanged runtime/test/build inputs and eight binary identities), plus 455/455 optional entries across six targets/ten profiles. Fifteen real-X drivers verify 45 hierarchy/pointer/reparent observations; ten boundary-policy profiles and three scoped guest sanitizers pass. Nine fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. Broader owners stay open.

DATA-01 #6 / IMAGE-01 #8 / CLEAN-01 #34: [visual startup ownership](verification/VERIFICATION-2026-10-20-VISUAL-STARTUP.md) is merged in [PR #132](https://github.com/gonz0w/fvwm-x/pull/132). Three startup ownership improvements pass one combined checkpoint: 2077 standard results (2071 fresh / six retained by unchanged runtime/test/build inputs and six exact binary identities), plus 459 optional results across six targets/ten profiles. Eleven pure policy drivers verify 352 observations; fifteen real-X drivers verify 60 initialization observations. Three scoped guest ASan/UBSan checks, nine fresh stages and four authenticated default BSD displays pass. Three inherited required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. PERF-01 #29 tracks the [periodic reference](verification/PERFORMANCE-2026-10-20.md) and five-checkpoint cadence; broader owners remain open.

FD-01 #7 / PROC-01 #15 / DATA-01 #6 / CLEAN-01 #34: [process/session startup](verification/VERIFICATION-2026-10-20-PROCESS-STARTUP.md) is merged in [PR #133](https://github.com/gonz0w/fvwm-x/pull/133). Three process/session constructor repairs pass one composed checkpoint after a single full matrix: 2141 composed standard results (2116 original fresh passes, 24 retained after unchanged-input/eight-binary checks per focused target, and one corrected Exec probe), plus 468 optional results across six targets/ten profiles. Eleven actual-core Exec drivers verify 77 observations; seventy session fault/retry entries and nineteen real session integrations pass. Nine scoped guest ASan/UBSan checks, nine fresh stages and four authenticated default BSD displays pass. Three inherited required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. Performance reference PR132 is accepted, at 0/5 subsequent merged implementation checkpoints; no comparison is due. FD-01, PROC-01 and DATA-01 are in Review; CLEAN-01 remains In progress. Broader owners remain open.

DATA-01 #6 / FD-01 #7 / CLEAN-01 #34: [swallowed-client session ownership](verification/VERIFICATION-2026-10-20-SESSION-CALLBACKS.md) is ready for review after merged PR133. Three registration/pending-queue/property repairs pass a composed six-target/ten-profile checkpoint after one original full run: 2197 standard and 475 optional results. The composition retains 2516 unaffected original passes and uses 156 affected entries (122 fresh / 34 retained by exact inputs and eleven binaries per focused target). Fifty callback entries, twenty pending-peer entries, nineteen real-session drivers with 57 independent clients and 171 property replies, fifteen scoped guest sanitizer entries, nine fresh stages and four final authenticated default BSD session displays pass. Three inherited required OpenBSD AMD64 omissions and its install/two staged-geometry gap remain without retries. The existing no-automatic-save policy is preserved. Performance reference PR132 remains at 1/5; no comparison is due. DATA-01/FD-01 remain in Review; CLEAN-01 remains In progress; broad owners stay open.

Core record dependency checkpoint: MAINT-02 #14 / CLEAN-01 #34; Three module/icon-box/event-filter dependency improvements pass 185 affected native checks across six targets/ten profiles (19 focused retained / 166 fresh). All 5981 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 68/216/37 to 30/6/2 objects. Broader PR134 runtime evidence and inherited OpenBSD gaps remain source-scoped. [Report](verification/VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES.md). PR134 is merged; performance cadence 2/5, no comparison due.

## Modernization issues — 2026-10-25

| Stage | Tracker ID | Issue |
| --- | --- | --- |
| 08 | MODERNIZE-01 | [#208 — Accept the three-release roadmap and issue contracts](https://github.com/gonz0w/fvwm-x/issues/208) |
| 01 | FOUNDATION-READY-01 | [#209 — Accept the finite R1 feature foundation](https://github.com/gonz0w/fvwm-x/issues/209) |
| 06 | GRAPHICS-01 | [#210 — Qualify shared native XRender drawing and resource contracts](https://github.com/gonz0w/fvwm-x/issues/210) |
| 06 | GRAPHICS-02 | [#211 — Evaluate direct Cairo drawing and bounded native effects](https://github.com/gonz0w/fvwm-x/issues/211) |
| 06 | LAYOUT-01 | [#212 — Build reusable responsive native panel and collection layouts](https://github.com/gonz0w/fvwm-x/issues/212) |
| 06 | BIND-01 | [#213 — Reuse typed revisions and event-driven bindings across native views](https://github.com/gonz0w/fvwm-x/issues/213) |
| 06 | THEME-02 | [#214 — Add composable semantic theme roles and independent appearance choices](https://github.com/gonz0w/fvwm-x/issues/214) |
| 06 | ICON-01 | [#215 — Share named icon lookup and scalable icon-theme assets](https://github.com/gonz0w/fvwm-x/issues/215) |
| 06 | ICON-02 | [#216 — Add explicit icon-font runs with named icons and reliable fallback](https://github.com/gonz0w/fvwm-x/issues/216) |
| 06 | RELOAD-01 | [#217 — Reload supported components with last-working-state retention](https://github.com/gonz0w/fvwm-x/issues/217) |
| 06 | INSPECT-01 | [#218 — Inspect effective configuration, bindings and theme provenance](https://github.com/gonz0w/fvwm-x/issues/218) |
| 06 | ACTIONS-01 | [#219 — Build a shared command palette for windows, apps and actions](https://github.com/gonz0w/fvwm-x/issues/219) |
| 06 | WORKSPACE-01 | [#220 — Add configurable hybrid layouts and snapping policies](https://github.com/gonz0w/fvwm-x/issues/220) |
| 06 | WORKSPACE-02 | [#221 — Add named scratchpads with predictable summon and focus behavior](https://github.com/gonz0w/fvwm-x/issues/221) |
| 06 | WORKSPACE-03 | [#222 — Remember monitor roles and restore scoped docking profiles](https://github.com/gonz0w/fvwm-x/issues/222) |
| 06 | WORKSPACE-04 | [#223 — Add a keyboard-accessible workspace and window overview](https://github.com/gonz0w/fvwm-x/issues/223) |
| 07 | EFFECTS-01 | [#224 — Prototype optional compositor-assisted workspace transitions](https://github.com/gonz0w/fvwm-x/issues/224) |
| 07 | LOCK-01 | [#225 — Integrate qualified locker readiness, idle locking and suspend coordination](https://github.com/gonz0w/fvwm-x/issues/225) |
| 05 | BUILD-PROFILES-01 | [#226 — Publish R1 profiles and resource costs](https://github.com/gonz0w/fvwm-x/issues/226) |
| 03 | FUZZ-01 | [#227 — Qualify the finite R2 local fuzz corpus](https://github.com/gonz0w/fvwm-x/issues/227) |

## Release-specific finite owners — 2026-10-26

See the release contract manifest for pending new parked/acceptance owner identities.
