# Style matching work — 2026-10-19

STYLE-01 #38 / CLEAN-01 #34. Base: merged PR #124,
`b8dc5048da1b1233572d7a335d5b7b6c5f1748d3`. Three related runtime improvements retain existing FVWM
configuration, matching precedence and update/grab/error barriers.

## Result and implementation

Three related style preparation/predicate/candidate-merge improvements pass one
combined checkpoint: 2029/2029 standard results (2021 fresh / eight retained by
exact source and ten binary identities), plus 447/447 optional entries across
six targets/ten profiles. Instrumented escape scans fall eight to zero for
twelve preparations. 180 actual-core merge cases and 132 preparation cases
pass. Four scoped guest sanitizers, nine fresh full stages and four
authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its
full-install/staged-geometry gap remain without retries.

| Operation | Predecessor | Candidate | Evidence |
| --- | --- | --- | --- |
| Prepare wildcard metadata | Find prefix, scan for escapes, rescan full pattern | Reuse verified prefix and classify only the remainder | Twelve preparations: eight wrapped escape-scan calls become zero; independent legacy matcher/language tests |
| Match explicit conjunctions | Visit universal explicit predicates | Omit their already-known true results | NULL/star and mixed-selector legacy differential tests; controlled matcher timing |
| Merge necessary-key candidates | Search every active head for each rule | Operation-local bounded ordinal heap | Exact visit sequence and complete merged styles versus independent linear traversal; grouped/interleaved/collision/retirement/append cases |

Preparation still computes the immutable fingerprint and any necessary-key
hash. This is a reduction in metadata-classification scanning, not a claim of
one total pass over the selector. Quoted tokens consume one decoded byte;
trailing escapes retain their impossible literal slot. Raw suffix offsets,
decoded widths, universal/literal masks and lookup domains keep their contracts.
No cached record/subject layout grows (184-byte metadata / 120-byte subjects on
the tested targets). Ordinary matching is not capped by overlap-proof budgets.

Universal explicit predicates already accept NULL subjects. Removing them
preserves the separate compatibility OR and the order of every remaining
literal/wildcard conjunction. No selector syntax or configuration migration
is introduced.

The heap borrows the same array of at most nine chain heads. Each rule has
one owner, subject buckets are deduplicated, and chain ordinals retain exact
priority/list order. Creation, incremental append, retirement, simplification
moves and SIZE_MAX invalidation retain their existing owners. Push/pop use
bounded parent/child arithmetic and never allocate. Replacing a minimum only
repairs its heap path. At nine active heads the source bounds a pop to at most
six ordinal comparisons after heap setup, versus eight in a complete head
scan. Setup has its own comparisons; smaller populations can have equal or
extra work. This is source-structural evidence, not an instrumented comparison
count or an assertion that every workload is faster.

No pointer survives command/event dispatch. The heap is finished before the
unchanged `EWMH_GetStyle` call, which still owns its existing side effects.
No style preparation is moved before grabs and no persistent match cache,
new state authority or scheduling/barrier change is added.

## Independent probes and baseline

The existing pure matcher test retains its legacy wildcard/NULL/escape/byte
language and overlap witnesses, mixed cold/warm fields and existing work counts.
A scoped `strchr` wrapper measures twelve preparations: literals, universal
values, stars/questions, quoted wildcards/backslashes and a trailing escape.
Both control and candidate compile with the same no-builtin/wrapper options.
Eight escape scans in the predecessor become zero in the candidate.

A separate actual-core merge probe runs in the existing style fixture. It
creates 160 distinct matching conjunctions across four explicit key domains
and fallback in one priority list. It compares exact live rule-ID pointer
visits and the complete initialized merged record against index-disabled
linear traversal with the original wildcard predicate. Border/title values
also have independent expected results. All comparisons finish before commands
retire their borrowed IDs.

Four shapes cover grouped, interleaved, forced-collision and synthetic nine-slot
partitions. The synthetic case changes only test-derived bucket assignment;
it does not claim that an ordinary configuration naturally needs nine chains.
Each shape checks initial precedence, retirement of head/middle/tail plus a
whole owner chain, then append after ordinal gaps. All twelve cases clean up
their rules and restore the control style. Existing maintenance/retention,
command-ownership, reference/collision and window-update probes remain.

The first unchanged-production core probe incorrectly expected repeated
trailing stars to match an empty suffix. Both linear/indexed results agreed
on `front-4`, while the new expected result incorrectly named `front-159`.
A focused diagnostic retained that observation. The original wildcard owner
explicitly preserves its exhaustion rule: repeated stars do not consume empty
text like a single final star. Literal tails correct the workload; no production
fix, deadline change or broader retry was made. Original source/logs and the
diagnostic remain. The corrected actual-core baseline passes twelve cases.
The first baseline's unit suite already passed, and its exact binary identity
is retained for the corrected core baseline without repeating that unit suite.

Candidate passes eight focused native entries: pure matcher/filter, style
profiles, update work, command ownership, index maintenance/retention and
window updates. Baseline/candidate executable probe logic is unchanged; one
probe comment is shortened to the C column limit. Source identities and that
exact comment-only difference are recorded in the receipt.

## Controlled timing observations

These are development Linux ARM GCC guest observations with the same controls;
there is no elapsed-ratio acceptance threshold or portable/native-CPU hardware
latency claim. Preparation and universal rows are medians of three samples in
one run. Merge rows cover the combined candidate and do not isolate heap cost.

| Unit workload | Baseline ms | Candidate ms |
| --- | --- | --- |
| 20,000 preparations of a 2,008-byte selector | 100.138 | 67.121 |
| 500,000 all-universal explicit matches | 5.402 | 1.091 |

| Actual-core workload, 512 lookups of 160 rules | Baseline ms | Candidate ms |
| --- | --- | --- |
| Grouped | 79.688 | 34.350 |
| Interleaved | 85.166 | 35.595 |
| Forced single-key collision | 80.172 | 40.287 |
| Synthetic nine slots | 82.979 | 40.166 |

## Combined checkpoint

- Six targets/ten profiles reuse the owning incremental builds: Linux ARM
  GCC/Clang/software/minimal, Linux AMD GCC/Clang and default FreeBSD/OpenBSD
  on both architectures. At most two heavy guests and two Ninja jobs each run.
- Standard scope passes 2029/2029: 2021
  fresh results and eight retained focused results. Optional scope passes
  447/447 fresh. Three required omissions leave
  2032 registered standard entries; no test registration changes.
- ARM GCC retains exactly eight successful results only after all frozen
  non-Markdown inputs and ten full binary hashes match. Six retained fixture
  evidence sets remain exact. Fresh/retained raw rows are separate and their
  unique-name composition is validated; publication causes no ordinary repeats.
- 15 actual-core drivers verify 180 merge cases.
  11 pure matcher invocations verify 132
  preparations with zero measured escape scans. Existing legacy/policy/reference
  metrics, EWMH state, ownership/index and window-update barriers all pass.
- Four scoped guest ASan/UBSan entries pass: pure matcher (leak checks enabled),
  style profiles, window updates and EWMH state (WM lifetime leaks disabled).
  These guest results do not establish separate native-CPU Linux acceptance.
- 9 fresh full stages validate 135 code and
  88 catalog identities after normal RPATH adjustment.
  Comparisons use PR124's source-bound installed identities, with no promotion
  of its missing OpenBSD AMD64 full installation.
- Four authenticated BSD displays pass 38 fixtures, refuse
  unauthenticated clients and exit cleanly. 18 staged
  geometry invocations verify 126 cases; 10
  real-client invocations retain existing wildcard/shared-condition checks.
- All 2041 frozen content hashes and canonical file modes pass
  native postchecks. Git executable/non-executable modes bind the archive;
  `git archive` tar.umask group-write bits are not tracked file modes. Every
  capture member, whole archive, executed runner, compile/test option and binary
  identity is validated. Core fixtures keep 75 assertion-enabled sources;
  style profile links its separate probe and Linux fault wrappers keep their
  two-source compilation contract. Changed-line diagnostic review has no findings.
- Portable `make verify` runs once: 166 passes / 11 native-Meson skips. Final
  documentation uses foundation/link/JSON/whitespace/source checks only.

This gate requests Meson's prerequisite builds before fresh tests and installs
with `--no-rebuild` after successful compilation/testing. PR124's report is
corrected to match its actual captured command: that earlier gate used
`--no-rebuild` tests after compilation. No historical result is relabelled or
rerun to make that documentation correction.

The ARM container initially fell below its 1 GiB owner-space reserve. Two
completed PR122/PR124 capture directories and the duplicate PR124 archive were
evacuated only after every file/size/SHA256 and whole archive matched preserved
canonical host evidence (605921205 duplicate bytes).
Source/build/stage trees, VM disks and other owners' work remain. The 1 GiB owner
and 256 MiB temporary-space floors remain; large inputs use owner filesystems.

## Source and remaining limits

Signed author-DCO runtime: `0661c346a001f1495bba3bb9d7ac11cee2d6bc5f`;
tree `74cd3c4b6e0bcacf392d113a748b1feb42c073b2`. Frozen archive SHA256:
`600c25980a5ad7c6bcd12cec54056045d39f493144bd51ffd26c9c18b4dcecaf` (2041 regular inputs).
Final upkeep changes only Markdown and the machine receipt.

[Machine receipt](VERIFICATION-2026-10-19-STYLE-MATCH-WORK-COMBINED.json) records source, diagnostics, fresh/retained
composition, archive/capture/runner/installed identities and measurement limits.
Full source snapshots, failed/diagnostic/corrected baseline attempts, development
measurements and native archives remain under
`builddir/codex-2026-10-19/style-match-work` in the owning workspace.

Three required OpenBSD AMD64 omissions retain their exact predecessor scope:
CI-01's worker timeout, IMAGE-01's SVG/MFL startup refusal and PR120's legacy
property first-connection refusal. The latter's previously completed unchanged
control remains with its original source/deadline and unproved cause; no retry
is made here. Full fresh installation and two staged geometry probes remain
unperformed on that target. [Prior runtime scope](VERIFICATION-2026-10-19-EWMH-TRAY.md).

Linux execution uses macOS-hosted environments; BSD kernels run under QEMU.
Native-CPU sanitizers, physical hardware, actual toolkits/compositors and release
acceptance remain separate. Actions remains disabled; VESA #93 stays deferred.
Broader STYLE-01, STATE-01, CLEAN-01 and maintenance owners remain open.
