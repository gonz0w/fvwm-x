# Library evaluation comparisons

Date: 2026-10-25. Owner: Cam (@gonz0w).
Status: issue scope approved; evaluations and adoption decisions pending.

After reviewing candidate libraries and existing plans, the user requested
issue updates that reuse existing owners and compare simplicity, performance
and dependency costs across Linux, FreeBSD and OpenBSD. Nine existing owners
cover the candidates; BUILD-PROFILES-01 #226 coordinates their shared evidence.
No new umbrella or duplicate evaluation issue is needed.

The [tracker](../PROJECT-TRACKER.md#execution-order) owns execution. Existing
issue status, priority, stage, milestone, Project Work batch and native
relationships remain unchanged. Preserve the selected maintenance/optimization
queue and feature gates; planning does not select every prototype now, reopen
accepted work or turn all evaluations into prerequisites to current work.

## Existing owner map

Each linked issue contains the bounded comparison and acceptance details.

| Existing owner | Candidates and baseline | Decision focus |
| --- | --- | --- |
| LIB-06 [#48](https://github.com/gonz0w/fvwm-x/issues/48) | Maintained cJSON versus yyjson on current bounded status/query workloads. | Document-pool ownership, copied/borrowed strings, allocations, failure paths and response/request costs; retain current UTF-8/numeric/schema/identity contracts. |
| TEXT-01 [#54](https://github.com/gonz0w/fvwm-x/issues/54) | Current checked Unicode helpers/Go x/text versus utf8proc and narrower libgrapheme; current font stack versus direct HarfBuzz and Pango/PangoCairo. | Compare capabilities separately: normalization/case folding/boundaries and shaping/layout/fallback. Count application code removed versus GLib/GObject/C++/binding costs and remaining input/widget responsibilities. |
| GRAPHICS-02 [#211](https://github.com/gonz0w/fvwm-x/issues/211) | Qualified native/XRender baseline versus bounded direct Cairo module drawing. | Paths/gradients/clipping/alpha, X requests, redraw/input latency, ownership and fallback; SVG linkage does not qualify a general UI renderer. |
| LIB-04 [#46](https://github.com/gonz0w/fvwm-x/issues/46) | Qualified librsvg/Cairo worker versus bounded LunaSVG worker. | Actual SVG/text/filter/resource fidelity, cold/warm render/transport and C++/PlutoVG/full closure; preserve accepted M4 consolidation and worker limits/leases. |
| LAYOUT-01 [#212](https://github.com/gonz0w/fvwm-x/issues/212) | Native layout baseline first, then optional bounded Clay prototype. | Layout policy removed versus render/text/input/focus/accessibility glue, arena limits and real panel/collection costs; no mandatory engine. |
| FILE-ADVANCED-01 [#81](https://github.com/gonz0w/fvwm-x/issues/81) | Isolated libarchive prototype versus relevant Go tar/zip and qualified utility/process baselines for selected formats. | Archive list/create/extract code and format/codec costs; traversal/link/expansion safety, limits, conflict/cancel/partial outcomes. Coordinate FILE-JOBS-01 #68; no live mutation or archive UI adoption. |
| FILE-DATA-01 [#66](https://github.com/gonz0w/fvwm-x/issues/66) | Current bounded in-memory/snapshot/data-file approaches versus SQLite/FTS5 for named non-sensitive metadata/search workloads. | Search/update/recovery simplicity versus schema/index/locking/migration/binding costs. Current browsing/IDs remain database-independent; coordinate #74/#81. |
| STORE-01 [#74](https://github.com/gonz0w/fvwm-x/issues/74) | Relevant C/base-OS and Go standard crypto facilities versus libsodium protected-memory/authenticated-encryption facilities. | Actual copy/locking/wiping/dump/refusal guarantees, key/version/recovery burden, guarded-memory and crypto overhead; protected memory first, no custom crypto or plaintext fallback. |
| SESSION-02 [#77](https://github.com/gonz0w/fvwm-x/issues/77) | Current no-bus baseline versus godbus/dbus and a narrowly justified native binding alternative if useful. | One finite optional consumer, bounded connection/signal/call lifetime, native/Go integration costs and truthful per-OS service availability; login/locking/WM remain usable without a bus. |
| BUILD-PROFILES-01 [#226](https://github.com/gonz0w/fvwm-x/issues/226) | Shared reproducible manifests and actual target/runtime/build inventories. | Consistent evidence tables and incremental/complete costs; #11/#13 retain patch/advisory review and #235 retains artifact-hardening verification. |

The alternatives are not assumed equivalent. HarfBuzz shapes text runs; Pango
supplies more layout infrastructure. Unicode normalization and segmentation
have different policy requirements. Clay does not supply complete native
widgets. A library present through isolated SVG can still add loading,
initialization and memory costs to another process.

## Required comparison rubric

Freeze a named current/no-new-dependency baseline, candidate versions and finite
FVWM-X workload before prototyping. Preserve configuration behavior, native
pixel/scale/cache generations, single-threaded authoritative X state and owned
process/transport boundaries. Existing accepted evidence remains source-scoped.

| Dimension | Required evidence |
| --- | --- |
| Simplicity | Application code/branches removed, integration/adapter/FFI code added, ownership/error/unwind/teardown paths, tests retained or changed, upgrade burden and remaining application policy. Lines of code alone do not establish a simpler maintained system. |
| Performance | Relevant end-to-end latency/throughput, startup, CPU, allocation counts, RSS or available OS memory metric, and idle/wakeups where applicable. Record datasets, repeated runs, variability, toolchain/features and non-applicable metrics with reasons. Upstream benchmark claims are not FVWM-X measurements. |
| Dependencies | Direct/static/transitive/build/runtime closure; incremental versus complete installed bytes; per-process loaded/initialized cost; compiler/interpreter/C++/cgo/FFI needs; licenses, package/backport availability, offline/source-release and optional/disabled profiles; maintenance/security-update burden. Do not count installed shared libraries as zero-cost process reuse. |
| Correctness and containment | Feature/fidelity parity plus malformed/boundary/failure/refusal/retry/teardown controls; resource budgets and cancellation; trusted-X and intentional configured-shell boundaries; required filesystem, sensitive-content and no-bus guarantees. |
| Decision | Adopt, defer or reject with the evidence, gaps, justified exceptions, smallest integration/rollback scope and exact supported profiles. No default or dependency replacement is authorized by issue planning. |

Each decision report must contain a comparable six-target table:

| Target | Evidence to record |
| --- | --- |
| Linux AMD64 | Exact release/kernel/libc/compiler/linker/library/profile, build/install and applicable workload results/costs. |
| Linux ARM64 | Same fields, preserving architecture and native/emulation identity. |
| FreeBSD AMD64 | Exact release/compiler/linker/packages/library/profile, build/install and applicable workload results/costs. |
| FreeBSD ARM64 | Same fields, preserving architecture and native/emulation identity. |
| OpenBSD AMD64 | Current supported release/compiler/linker/packages/library/profile, build/install and applicable workload results/costs. |
| OpenBSD ARM64 | Same fields, preserving architecture and native/emulation identity. |

Record passed, failed, unavailable and deferred cases explicitly. Use comparable
workloads within each platform; do not infer a cross-OS speedup from different
machines or equate unlike memory metrics. Library/protocol portability does not
guarantee every OS provides the same session services. Final adoption across
supported targets needs the corresponding evidence or an explicit scoped
platform decision, not silent support reduction.

Use focused incremental prototypes first, early affected BSD/API/build checks,
then the relevant cross-platform comparison at the decision checkpoint. Do not
run a full matrix after each small edit. Retain separate native-CPU, physical,
toolkit/compositor and release gates. Rejection or justified deferral is a valid
evaluation result; never manufacture an integration to fill a milestone.

## Scope limits

No candidate has been benchmarked or adopted by this planning checkpoint.
SQLite is not a prerequisite for live browsing or the base palette, does not
replace domain identities with row IDs, and does not authorize sensitive
plaintext indexing/persistence. Key/unlock/retention/recovery and default
decisions remain with STORE-01. Ordinary configured symlinks, filenames and
shell behavior keep their existing contracts.

No whole-core XCB/event-loop/renderer rewrite, second decoder or state authority,
mandatory GUI toolkit/bus/GPU, implicit service startup, build-backend change,
hosted checks or release publication is selected. Coordinate the existing
[hardening follow-ups](PORTABLE-OPENBSD-HARDENING-2026-10-25.md) when worker
authority, dependencies or produced binaries change. This record adds no new
blanket blocker to the current cleanup/optimization handoff.

## Upstream references

- [yyjson API](https://ibireme.github.io/yyjson/doc/doxygen/html/api.html)
- [utf8proc](https://juliastrings.github.io/utf8proc/)
- [libgrapheme](https://libs.suckless.org/libgrapheme/)
- [HarfBuzz scope](https://harfbuzz.github.io/what-harfbuzz-doesnt-do.html)
- [Pango layout](https://docs.gtk.org/Pango/class.Layout.html)
- [Cairo](https://www.cairographics.org/)
- [LunaSVG](https://github.com/sammycage/lunasvg)
- [librsvg feature support](https://gnome.pages.gitlab.gnome.org/librsvg/devel-docs/features.html)
- [Clay](https://github.com/nicbarker/clay)
- [libarchive](https://www.libarchive.org/)
- [SQLite self-contained design](https://www.sqlite.org/selfcontained.html)
- [SQLite FTS5](https://sqlite.org/fts5.html)
- [libsodium secure memory](https://doc.libsodium.org/memory_management)
- [libsodium authenticated streams](https://doc.libsodium.org/secret-key_cryptography/secretstream)
- [godbus/dbus](https://github.com/godbus/dbus)
