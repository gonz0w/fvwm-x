# Ordered XDG menu allocation — 2026-10-12

Owners: RUNTIME-GO-01 / UTIL-01, coordinated with XDG-01 / COMPAT-02.
Status: **three-story pure policy checkpoint locally qualified**
at frozen `be8e`. Full provider/frontend integration remains open.

## Scope

The new [allocation policy](../../bin/FvwmPrompt/internal/xdgmenu/allocation.go)
consumes an owned `MenuPlan` produced by the future merger/resolver. It does not
parse a menu file, acquire directories, execute an application or render UI.
No production consumer imports this package yet.

1. Ordered Include/Exclude operations reuse the existing boolean rule evaluator.
   Inclusion marks a source allocated even if a later exclusion removes it.
   Exclusion alone does not allocate it; later inclusion can restore it. Results
   retain literal source records and use deterministic desktop-ID order before
   the later layout/localized-label sort.
2. Allocation runs regular menus first and `OnlyUnallocated` menus second, with
   children before parents and siblings in input order. The output tree retains
   input order. Each menu snapshots its eligibility before applying ordered
   rules, so its own inclusion mark cannot skip a subsequent exclusion/reinclude.
   Later unallocated menus see the earlier menu's allocation marks. Source paths
   identify shared files: a child override with the same flattened ID is a
   distinct source, while the same source exposed under a different ID remains
   allocated. Caller acquisition must supply consistent, clean absolute paths;
   the policy does not resolve filesystem aliases or symbolic links.
3. One operation owns allocation marks, category caches and whole-job budgets.
   Categories decode once per immutable desktop object shared by menus. String
   slots count toward category storage, including empty list values. Aggregate
   items/bytes and rule-node/category/iteration work are bounded across all menus,
   preventing individually bounded rules from multiplying into unlimited work.

Ordered operations and inclusion-before-exclusion allocation follow the
[Menu 1.1 generation algorithm](https://specifications.freedesktop.org/menu/latest/query-algorithm.html).
Child-first traversal preserves the established provider's ordering. These
functions require already-consolidated child names and inherited/local AppDir
pools. They do not implement file merging, moves/deletion, layout/inline behavior,
directory-entry visibility, application visibility, icon lookup, CLI/menu
selection, startup notification, Form actions or whole-standard conformance.

The legacy PyXDG source was inspected as a secondary implementation reference:
its regular pass walks children first and records inclusion matches, while its
second pass skips already-matched entries during later rules. That second-pass
shortcut can skip ordered exclusion/reinclusion; it is not adopted as a required
parity behavior. Independent expected-result tests use the ordered-operation
contract, not Python eval or a snapshot re-recorded from this candidate.

## Ownership and boundaries

Input plans, rule nodes, pools and desktop objects remain caller-owned and
immutable during the operation. Returned trees/slices and the allocation ledger
belong to this call. Failure returns an explicit error and no partial output;
it does not leave allocation flags in shared desktop records. There is no new
global state, callback, file/process work or X state.

Allocation uses the input foundation's 128-level depth, 100000 total plan/pool/
rule-reference items and an 8 MiB name/source-identity/category allowance. Names/IDs/paths retain the 64 KiB
individual limit. Decoded category storage charges sixteen bytes per string
slot on the supported 64-bit targets plus actual text. A shared 10000000-step
allowance covers preparation, rule nodes, category comparisons and entry passes.
Sorting and bounded string comparison/parsing have additional byte/name costs; this is not
a clock deadline or a complete worker-memory limit. Acquisition, merging and
rendering still need their own whole-job work/deadline owner and representative
full-provider workload measurements before activation.

These are inactive external-input policies. No new limit, field name, argument,
binding, function or theme syntax is activated in existing FVWM configuration.
The installed Python provider/Perl frontend, all Go executable entry points,
module/vendor metadata and Meson/install selection remain unchanged.

## Frozen source and focused evidence

Source archive SHA-256:
`be8e79d0482e98527e3e86acdfe5c2a26e9f7266c89704c812bc5a132c69f938`.
It freezes 494 Go module files: two new implementation/test files and all 492
predecessor files unchanged from the qualified `9337` discovery/preference
checkpoint. The allocation policy remains standard-library-only with no new
dependency or Go-floor change.

Evidence: `builddir/codex-2026-10-12/xdg-allocation/` contains `source.tgz`,
`scope.json`, freeze/guest/dispatch/validation scripts, local test JSON and native
logs/proofs. Executed guest runner SHA-256:
`e5296c3bcffca80cb629358d0e4b9a68bd438364c460d2793f0b0f7efedd77aa`.
Private native source roots end in `fvwm-xdg-allocation-20261012`; OpenBSD evidence
lives under `/home`, with accepted predecessor trees/caches retained.

Local offline focused tests, all eight Go packages and vet pass. The policy suite
now contains 24 top-level functions and 265 named subtests: four new functions
and 35 additional named subtests. Coverage includes ordered boolean inclusion/
exclusion, excluded-but-allocated entries, two-pass and child/sibling ordering,
source identities/overrides, later re-inclusion, unchanged inputs, empty-pool
rule validation, malformed/cyclic plans, global Cartesian work/item budgets,
category slot storage and cache sharing across sixty menus.

All six targets pass the affected native checkpoint: eight Go packages per
target, vet/dependency checks and the inherited two input benchmarks. Both Linux
targets pass actual policy race instrumentation. Strict proof validation checks
exact source/runner/log composition, target architecture, test/package name sets
and the stdlib-only production graph; all current scoped source hashes match.
There are 48 ordinary package outcomes and two race package outcomes. The 265
named subtests execute on six ordinary and two race runs: 2120 subtest executions,
not 2120 different cases. Top-level results are separate. Four new functions
contain 35 new named subtests. No local/native/dispatch test failure occurred;
retained archive-extraction future-filter warnings are not failures, with member
validation already refusing traversal and non-regular files.

| Native target | Toolchain | Go package regressions | Policy top tests / named subtests | Vet / stdlib graph | Policy race |
| --- | --- | --- | --- | --- | --- |
| Linux ARM64 | 1.22.2 | 8 pass | 24 / 265 pass | pass | pass |
| Linux AMD64 | 1.27.0 | 8 pass | 24 / 265 pass | pass | pass |
| FreeBSD ARM64 | 1.27.0 | 8 pass | 24 / 265 pass | pass | not run |
| FreeBSD AMD64 | 1.27.0 | 8 pass | 24 / 265 pass | pass | not run |
| OpenBSD ARM64 | 1.27.0 | 8 pass | 24 / 265 pass | pass | not run |
| OpenBSD AMD64 | 1.27.0 | 8 pass | 24 / 265 pass | pass | not run |

A fresh direct Go 1.21 compiler check remains open. No provider speed/RSS/GC/
latency claim follows from these pure tests or inherited microbenchmarks; exact
benchmark lines are retained in `validation.json` and the native proofs.

Documentation upkeep validates 806 local links, owning whitespace and all 494
current frozen source hashes; `upkeep.json` records document identities.
Whole shared-checkout `git diff --check` retains unrelated generated Script
scanner/parser whitespace in `unrelated-diff-check.txt`. Those other-thread
files were preserved.

## Remaining integration

Continue ordered file acquisition/merging and plan construction, including
inherited/local pool precedence, defaults, moves/deletion and layout/localized
presentation. Route all application actions through the existing literal Exec
policy and shared command engine. Preserve supported CLI/menu selection and
old preferences. Complete the direct owned Form save/export boundary and qualify
paired consumer/actions, installed/manual references and actual native X before
activation. Python/Perl retirement remains separate.

The inactive pure package needs no new C/core/X/install matrix. Required future
provider/native-X, default/minimal/offline installs, real release CI/native C
sanitizers and physical checks remain open. See the
[tracker](../PROJECT-TRACKER.md#execution-order),
[discovery/preferences](VERIFICATION-2026-10-12-XDG-DISCOVERY.md),
[input foundation](VERIFICATION-2026-10-12-XDG-POLICY.md) and
[full-provider fixtures](VERIFICATION-2026-10-12-XDG-FIXTURES.md).
