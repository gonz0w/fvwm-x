# Cleanup and tooling roadmap

Recorded 2026-10-05. The [project tracker](PROJECT-TRACKER.md) owns ordering and
status. This document expands the cleanup findings and library decisions for
the next build thread. Effort ranges are engineer-days, including focused
verification; they are estimates, not completion dates or accumulated totals.

Current selection, 2026-10-23: the user pauses feature pickup and prioritizes
source/legacy/dependency maintenance. The [maintenance-first batch register](decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md)
and tracker supersede the historical handoff and effort estimates below.
The old utility/parser/catalog checkpoints are retained, not reimplemented;
performance-only tuning stays deferred. Use finite related batches and one
combined focused gate rather than full platform checks per small cleanup.

Updated language direction on 2026-10-06: the user requests Perl out, Python
runtime helpers replaced by Go, and Lua/go-lua where useful. The
[runtime scope](decisions/RUNTIME-GO-LUA-2026-10-06.md) supersedes earlier Python
utility-port recommendations and optional default-preserving Perl packaging.
C core/native modules remain; Go helper migration and isolated Lua bindings are
queued under RUNTIME-GO-01/LUA-01. Existing Python maintenance tools remain
operational until TOOLING-GO-01's later qualified replacement. Earlier checkpoints
and effort estimates are historical evidence, not fresh Go-port estimates.

## Current maintenance checkpoint

The build wrapper and shared Meson/manual tooling are now active; see the
[tooling report](verification/VERIFICATION-2026-10-05-TOOLING.md) for actual profile/output
comparisons and remaining qualification limits. BSD ports gettext discovery
is repaired. The public website migration is deferred to
[DOCS-01](decisions/WEBSITE-MANUALS-BRIEF.md); the prior output candidate remains
implemented and rendered for review, without blocking other adoption. Parser/catalog adoption is locally qualified. Prompt/protocol metadata is locally adopted; source-release and installed utility ports remain open. The staging description below
records the original candidate batch, not the current adoption status.

## Next build handoff

The [tracker's execution order](PROJECT-TRACKER.md#execution-order) is the
single scheduling authority. The FIFO change is complete with seven final
local gates and four default BSD X servers; see the
[FIFO report](verification/VERIFICATION-2026-10-05-SCRIPT-FIFO.md). SCRIPT-01 remains open
for text/list edits, geometry/font arithmetic, modal servicing and resource
lifetime/budgets. Finish one bounded change and record its evidence before
switching; do not treat all interpreter work as a prerequisite for every other
item.

1. Take the next bounded TextField/List correctness batch. Keep compatibility
   checks and native Linux sanitizer preparation attached to runtime work.
   Start short LIB-02/LIB-07/LIB-01/LIB-03 package and reachable-API triage
   alongside it; confirmed serious findings advance immediately.
2. Build/manual adoption has reached the tooling checkpoint. Continue the remaining MAINT-02 gates;
   the website/manual redesign is deferred under DOCS-01.
   Interleave verified tool adoption
   with bounded DATA-01/IMAGE-01, FD-01/PROC-01 and INPUT-01 work. Candidate
   tools already exist; they are not a reason to defer every migration until
   all hardening is finished. Decoder isolation remains a later design choice.
3. Repair PERL-PROTO-01 and PROMPT-01 contract mismatches before new primitives.
   Adopt checked metadata generation with the Perl fix and prepare UTIL-01
   behavioral fixtures alongside PROC-01. The Go-enabled FvwmCommand port is
   [locally qualified](verification/VERIFICATION-2026-10-12-COMMAND-PORT.md);
   standalone Perl ports and disabled-Go profile migration remain separate.
   CI verification/native sanitizers
   remain P1; publishing ownership is a separate pending decision.
4. Capture a responsiveness baseline, then fix QUEUE-01 before OUTPUT-01 and
   remaining PERF-01 tuning. Coordinate STACK-01 identity with XIO-01 query
   reduction, and DRAW-01 barriers with STYLE-01/STATE-01. CACHE-01 follows
   relevant image ownership checks; LOOP-01 consolidates the interaction/output
   contracts. The [optimization roadmap](OPTIMIZATION-ROADMAP-2026-10-05.md)
   expands these dependencies; STACK-02 is an earlier bounded correctness check
   and button-release waits extend INPUT-01. Pure cleanup accompanies its owner;
   do not wait for every optional security redesign or duplicate existing fixes.
5. Continue small DEAD-01/CLEAN-01 batches and fixture-backed utility ports
   between runtime checkpoints; safe cleanup may happen earlier. Larger
   sandbox/dependency/compatibility/parser/portability changes retain their
   gates. MAINT-02 incremental adoption is P1; ARCHIVE-01 follows each accepted
   replacement only after parity, reference and distribution checks.
6. Keep BUTTON/CFG/IPC primitives queued in the owning build thread after the
   current security/correctness and responsiveness checkpoints. Library
   replacement evaluations remain optional and evidence-driven.

Runtime modifications belong to the owning build thread. This document's tooling
work is separate from the newly scoped installed Go/Lua runtime migration.

## Runtime and installed-component backlog

| ID | Priority | Finding, acceptance and effort |
| --- | --- | --- |
| PERL-PROTO-01 | P1 / repair strictly locally qualified | Ten event values, exact named extended mask, three-word Echo decoding and category-marker-free handler matching are repaired. Checked header/command generation preserves exports/POD and emits provenance plus JSON companions. Native packets, installed consumers and actual Echo/Reply integration pass all six targets and default BSD X servers. See [current report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md). |
| PROMPT-01 | P1 / strictly locally qualified | Standard-library transport repairs newline framing, checked writes, incremental bounded JSON, EOF and coordinated lifetime. One-shot writes finish before exit; Quit confirms locally. History remains. Controlled-peer and actual MFL checks pass the combined six-target/install/default-X checkpoint. Public CLI consolidation and Go utility ports remain separate. See [current report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md). |
| DEAD-01 | P2 / inactive functions locally qualified; unbuilt-source removal focused-passed | Seven proven inactive functions were removed with active-preprocessing equality and combined gates. The empty template and legacy formatting source are now removed after six-target compile/link and system formatter review; active config declarations remain. [Cleanup evidence](verification/VERIFICATION-2026-10-07-STACK-GROUP-CLEANUP.md) is separate from the preceding runtime checkpoint. |
| COMPAT-02 | P2 / retirement direction approved | Retire FvwmPerl/perllib and Perl-specific installed APIs/tooling after reference, package, manual and migration checks. The user's explicit Perl-removal decision supersedes the former optional preservation plan; arbitrary Perl is not transparently translated. Coordinate qualified Go utility replacements and Lua selected-use-case migration, preserving unrelated FVWM/Script behavior and user files. See runtime scope; former effort estimate is superseded. |
| UTIL-01 | M1 implementation / combined acceptance in progress | Mandatory Go family and replaced utility retirement are owned by DEP-GO-01 #179, DEP-XDG-01 #180 and LEGACY-01 #181. The [exact disposition](decisions/M1-RUNTIME-BASELINE-2026-10-23.md) preserves qualified native utilities and freezes original CLI contracts. Full framework retirement remains M6; remaining runtime controllers M2. |
| PROC-01 | P1 audit / P2 cleanup | Review shell image conversion in `fvwm-menu-desktop`, dialog helpers in Perl Toolkit, and helper descriptor ownership under FD-01. Replace constructed subprocess shell strings with literal argv where semantics allow; preserve explicitly requested shell execution. Test unusual paths, failed exec, inherited descriptors and cancellation. Do not label quoting patterns an exploit without a reproducer. 3–6 days. |
| GEN-01 | P2 | Validate regenerated FvwmScript parser/scanner with a recorded Bison/Flex version pair, grammar fixtures, rejected-input diagnostics and native/X gates. Current committed parser identifies Bison 2.3; scanner identifies Flex 2.6.4. Keep committed generated sources so release users need no generators. New tooling emits candidates only; runtime parser adoption and old syntax-helper removal follow acceptance. 4–8 days. |
| PORT-01 | P2 | Use compiled symbols, callbacks, dispatch tables and conditional builds to prove active code is orphaned. Review unsupported historical platform branches against SUPPORT.md; retain BSD and Motif/Open Look behavior. Review installed legacy Form/Script examples and FvwmTalk's old public wrapper before archival or optional packaging. Never delete based solely on a search count or date. 1–3 weeks, bounded batches. |
| LINK-01 | P2 | Replace broad `all_found_deps` linkage with proven per-target dependency sets. Test minimal/default/all-supported feature configurations and clean installs on six targets; preserve transitive static-library dependencies. 2–4 days. |
| I18N-01 | P2 / maintenance adopted | Shared language discovery, complete candidate publication and consistent NLS detection/catalog selection are locally adopted. The [whole-source checkpoint](verification/VERIFICATION-2026-10-11-I18N-ADOPTION.md) passes1743 entries/six standard plus two feature profiles,120 staged code identities,77 catalogs and six-target138-key parity. The old helper and ten unused GMO binaries are retired after deterministic source-fixture, reference and exact install checks, with backup. New UI extraction/translation coverage and font/input checks remain; language polish is separate. No public-release/native-CI claim. |

Current maintenance evidence: [GEN-01](verification/VERIFICATION-2026-10-08-PARSER-GENERATION.md)
has deterministic modern generation and thirteen ordinary configuration matches
on all six native targets; runtime/adoption gates remain. [I18N-01](verification/VERIFICATION-2026-10-08-I18N-MAINTENANCE.md)
uses all eleven maintained catalog locales, preserves both fuzzy compilation
policies, passes clean catalog-only Meson generation/install on Linux/BSD ARM64,
and corrects four Language headers. Whole-source/AMD64 migration gates remain;
the audit rows above retain their original motivation.

C11 remains the core language. Go is now the approved first-party runtime helper
target; isolated Go-hosted Lua is scoped for programmable primitive consumers,
with engine/dependency adoption gates. Perl framework retirement is an explicit
approved compatibility exception, not a claim of automatic source translation.
Existing Python build/maintenance tooling remains until its separate migration;
no working C module, native UI toolkit or external system service is replaced by
inference. The tracker owns port/removal checkpoints and their acceptance.

## Library review

Reviewed upstream sources on 2026-10-05. Vendored versions are fixed in this
checkout; shared-library versions depend on the actual target package set and
its security backports. The new inventory tool can record pkg-config versions,
but version strings alone do not establish a security patch status.

| ID | Priority / recommendation | Evidence and acceptance |
| --- | --- | --- |
| LIB-01 | P1: cJSON update and maintenance decision, 2–4 days | Vendored 1.7.17 is behind 1.7.19. Upstream's [changelog](https://github.com/DaveGamble/cJSON/blob/master/CHANGELOG.md) lists fixes to NULL/overlap handling, heap bounds and duplicate recursion. A collaborator's [maintenance notice](https://github.com/DaveGamble/cJSON/issues/1080) says prompt fixes cannot be assumed. Audit our serializer call sites, compare local changes, import reviewed fixes with source hash/license, and stress allocation/depth/number behavior under native sanitizers. Current FVWM call sites generate JSON; no external cJSON parse or duplicate call was found. The [2026 JSON Patch advisory](https://github.com/advisories/GHSA-5q3m-r3x7-8phg) concerns cJSONUtils, which is not shipped here: do not mark that path reachable. Recheck other advisories rather than declaring 1.7.19 fully safe. |
| LIB-02 | P1: PNG and transitive image-library security baseline, 1–3 days plus IMAGE-01 work | Meson accepts libpng >=1.2. The [upstream security page](https://www.libpng.org/pub/png/libpng.html) identifies 1.6.59, released 2026-09-30, as fixing CVE-2026-46675 in compressed ancillary-chunk handling. Inventory actual linked packages/backports on all six targets and reproduce relevant decoder paths. Prefer distro-supported patched releases; choose a supported API floor only after BSD package verification. Preserve PNG/XPM formats and connect resource containment to IMAGE-01. Include zlib, FreeType, Fontconfig and SVG transitive dependencies in the package advisory inventory; no claim that every listed advisory reaches our loader. |
| LIB-03 | Current audit/compiler adoption locally qualified | The [combined checkpoint](verification/VERIFICATION-2026-10-14-GO-BASELINE.md) adopts the approved optional Go 1.26.9/1.27.2+ stable compiler baseline, with no auto-download during configuration. Six actual current SDKs, the actual minimum and seven native source/symbol scans qualify. Unused Logrus removal preserves six used pins/426 third-party payload files. BSD recipes select fixed verified SDKs instead of the retired Go package. Compiler patch upkeep remains routine; the independently failing OpenBSD VESA startup is tracked under LIB-07 #93. |
| LIB-04 | P2: SVG backend/API floor, 2–4 days | Meson accepts librsvg >=2.13.92 and old Cairo alternatives. [Upstream supported-version policy](https://gnome.pages.gitlab.gnome.org/librsvg/devel-docs/supported_versions.html) currently lists 2.61.x/2.62.x. Our loader already has a modern >=2.52 render-document path. Select a supported package/security baseline, prefer librsvg with Cairo, and retire obsolete backend branches only after feature/visual tests on all targets. Do not impose the newest upstream build toolchain on end users. |
| LIB-05 | P2: dependency and interpreter requirements, 2–4 days | Audit mandatory xkbcommon: no project-owned C include/call was found, though Meson requires it. Remove only if there is no actual dependency and planned input work does not require it. Declare tested Python/Go minimums separately for build, desktop and developer tools. Record X11, XRandR, Xft, Xt, ICE, libevent and optional libraries per target; trim inaccurate docs and linkage. [libevent](https://libevent.org/) lists 2.1.13 stable (2026-07-01); prefer maintained distro packages and review fixes before raising the >=2.0 floor. Retain Xlib/libevent/Xft architecture. |
| LIB-06 | P2 evaluation: JSON alternative, 3–5 days; migration separately estimated | Compare a maintained cJSON fork/patch set with [yyjson](https://github.com/ibireme/yyjson) using our actual status serialization, UTF-8, numeric, escaping, allocation and memory/CPU limits. Preserve configuration-facing use; update protocol consumers together. Only a configuration-file/format break or required migration needs a compatibility decision. Review license, maintenance, fuzz coverage and BSD/ARM64 builds. yyjson is a candidate, not an approved replacement. Add no mandatory package; a reviewed vendored alternative must have a maintenance owner. Whole-core XCB, a GUI toolkit, or an image-library rewrite is not approved by this review. |

LIB-07 (P1 package/security audit, 1–3 days) covers the X11 library/server
baseline, coordinated with LIB-02 rather than a second decoder rewrite.
[X.Org's security advisories](https://www.x.org/Development/Security/) list
CVE-2026-4367 in libXpm before 3.5.19. Our Linux verification environment reports
3.5.17 via pkg-config, so confirm distro backports and actual linked libraries
before deciding patch status. Review Xlib/Xt/ICE/Xft/Xcursor/XRender and the Xorg
or Xenocara server as separate packages; server security fixes are not repaired
by a WM source change. Record OS package versions/patches on every target and
refresh verification images. No automatic system package upgrades are performed
by this thread. Include supported font-library advisories under LIB-02/LIB-05.

## Maintenance staging and removal

Safe work completed in this thread: current documentation index and roadmap
links, historical guidance banners, build-documentation corrections, ignored
macOS Finder files, and independent developer tools/tests. No Meson target,
CI workflow, current build wrapper, runtime source or installed utility was
changed for this maintenance batch.

| ID | Stage | Scope and gate |
| --- | --- | --- |
| MAINT-01 | Parallel tools created | Python build wrapper, shared manual manifest/rendering, Perl metadata candidates, version-checked parser generation, gettext maintenance, read-only source/dependency inventory and deterministic committed archives. See the tools guide for verification and current limitations. |
| MAINT-02 | P1 / partially adopted | Build wrapper and shared Meson/manual list/extraction are active, with actual FVWM/Asciidoctor evidence in the tooling report. Linux AMD64 Go qualification, clean source-release gates and other generators remain. Website/manual replacement is deferred under DOCS-01 and does not block these gates. Continue one replacement at a time with rollback and its owning compatibility/runtime checks. |
| CI-01 | P1 local profiles/default-X and selected downloads qualified / provider/native CI open | [Go/default-X qualification](verification/VERIFICATION-2026-10-12-CI-PROFILES.md) adds optional offline client profiles and authenticated local BSD default-X checks. Thirteen selected assets match download hashes; inherited container digests remain observations. [Full-SHA candidates](ci/README.md) stay inactive. Finish provider hash/cache enforcement, provider-specific framebuffer/input checks, actual native/package/sanitizer runs and container-content review. Release repository/website/destinations/credentials and upstream notifications remain separately owned; no publishing or live-workflow change. |
| REPRO-01 | P2 / queued | Pin developer/container/Nix environments and generator versions with a refresh policy; keep platform-native package builds available. Preserve offline builds and dependency licenses. Decide source-vs-binary assets per release. 2–4 days. |
| ARCHIVE-01 | P2 / queued, after migration | Remove retired build/generator scripts, disabled Travis config and unused docs only after references, packaging and parity checks. Current dist hook explicitly removes `.disabled-travis.yml`; deleting it first can break packaging. Retired planning/upstream notes are now under `dev-docs/history/` and reports under `dev-docs/verification/`, with references/hashes retained. Keep that history discoverable in Git; exclude agreed historical content from release bundles rather than destroying evidence. Never recursively delete ignored build trees, VM disks or user data. 1–2 days. |

The replacement config-header concatenator was not duplicated: the current
`meson-scripts/conf-cmd.sh` already handles failed commands and avoids changing
timestamps for identical output. Retain it unless a measured need appears.

LIB-01 update, defined numeric edges, approved UTF-8 presentation and checked
print sums are now [locally qualified](verification/VERIFICATION-2026-10-07-JSON-BOUNDARY.md)
from the exact 1282-file snapshot. The table records the original audit
motivation. This is not a version-only security verdict or a LIB-06 replacement
approval; downstream maintenance and native release CI remain separate.

## 2026-10-07 runtime checkpoint

Independent atom batching, approved 32-record expose collection/merging and
first-icon WM_STATE identity are locally qualified in the exact 1291-file
[combined checkpoint](verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md).
Broad dirty-state and query/barrier work remains queued. STACK-01 now has a
finite parent-cycle control and a private guard candidate; the
[Layer fallback decision](decisions/TRANSIENT-PARENT-CYCLE-PROPOSAL.md) precedes
production activation. Native release CI and physical coverage remain separate.

2026-10-07 stacking/query batch: both invoked-window cycle choices are approved
and activated, with finite hint validation and highest-override query reduction.
Eight focused Linux checks pass. The exact 1301-file combined checkpoint is
locally qualified (1144 standard entries, 120 installed identities, 72 default BSD
X invocations, optional 148/128 and 27 scoped sanitizer checks); earlier atom/expose evidence remains scoped to its own source. Reverse
closure, deep acyclic recursion and further barrier/query work remain queued.

The first DEAD-01 inactive-function batch is implemented on 2026-10-07: six listed
functions plus unused ResyncFvwmStackRing are removed after reference and active
preprocessing checks. Focused compilation and full qualification now pass
in the [three-story traversal checkpoint](verification/VERIFICATION-2026-10-07-STACK-TRAVERSAL.md).
Template/fallback reviews remain queued.
