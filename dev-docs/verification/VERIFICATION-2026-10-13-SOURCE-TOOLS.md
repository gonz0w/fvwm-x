# Source-tool maintenance checkpoint — 2026-10-13

MAINT-02 child issues [#84](https://github.com/gonz0w/fvwm-x/issues/84) and
[#85](https://github.com/gonz0w/fvwm-x/issues/85) form one review batch under
[parent #14](https://github.com/gonz0w/fvwm-x/issues/14). The foundation PR #83
was merged before pickup. This is stage 2 maintenance, not source-release adoption.

## Source and changes

Base: `5231ce3ce23216e0c6e2772b3488842f71e1b3df`.
Implementation: `168aebf62455d560971f0781f90354c0e88a4706`;
tree: `e2a2cbd984be63a7350d1bb146adce05e0ff9c37`.
The PR's later documentation commit retains these exact tested inputs.

Regular file and gzip hashing use 128 KiB reads. Git archive output goes to a
temporary disk spool owned by the archive operation. Subprocess success is
required before reading or publishing output. File-list/member metadata still scales
with entry count; temporary disk space is now required for the uncompressed
tar as well as the final gzip. Normalized archive contents, permissions,
exclusions, committed-source behavior and fresh-output ownership are preserved.
The manifest gains `bytes`; negative epochs fail in the programmatic API too.

Go manifest observations include input identities, module/Go/toolchain and
require/replace/exclude text with source lines. Optional go.sum and vendor
manifest identities are recorded; the existing flat map stays available.
This is text inventory, not Go validation, resolved package/symbol reachability
or advisory evidence. A missing pkg-config executable now has an explicit
availability flag and null package observations rather than losing the report.
No dependency update, config migration, runtime C/Go change or Meson dist
switch is included. The other chat's unpublished runtime changes are excluded.

| Tested input | SHA256 |
| --- | --- |
| `tools/maintenance/common.py` | `8b42f45cd946c1823b2cc9bbbad36c63e2c76ed7b34ee478b1ea884442964826` |
| `tools/maintenance/source.py` | `17e501d02caf3416ab53aa525e5922dbbd5b5e36c5e815ceeb0f29f8fedfbcfe` |
| `tools/maintenance/tests/test_source.py` | `5fa3b5fdf21a8ec52bd5c622af963b3927185f99f5c0008eff135e28444372c3` |

## Actual checks

- Existing macOS tooling baseline: 29/29 passes before edits.
- Final macOS source regression suite: 15/15 passes. Cases cover bounded reads,
  large payload parity, link-text hashing, dirty committed-source selection,
  existing output preservation, invalid commit/epoch and failed Git cleanup,
  manifest provenance/directives/companions and absent pkg-config.
- Local `make verify`: 111 passes, five native-Meson NLS skips. The unchanged
  NLS fixtures retain their separately accepted Linux result; this batch does
  not label those skips as a new native pass. Repository/whitespace and Python
  syntax checks pass. The invalid-ref fixture's Git diagnostic is expected.
- A check accidentally ran in the shared checkout and stopped on existing
  generated-file whitespace. Its failure is retained separately; no source was
  changed, and acceptance uses the subsequent isolated candidate run.
- Source-bound six-target affected run: 75 passes out of 90 executions;
  fifteen real-Git skips are explicit. Linux ARM64 passes all 15. Linux AMD64,
  FreeBSD ARM64/AMD64 and OpenBSD ARM64/AMD64 pass twelve policy cases each
  and skip the three real-Git cases because Git is absent. No guest packages,
  VM disks, active runtime sources or other chat's build directories changed.
- A 32 MiB regular-file fixture produces identical baseline/candidate inventory
  identities and gzip SHA256. Peak Python resident bytes measured on macOS
  fall from 95289344 (90.9 MiB) to
  25001984 (23.8 MiB), about 74% lower. This single
  tooling measurement excludes the Git child and makes no WM latency claim.
- Actual candidate inventory observes 1553 tracked/untracked working files and
  nine Go directives, plus checksum/vendor manifest identities. It is a
  working-tree observation, not an atomic release snapshot.

Receipts: [VERIFICATION-2026-10-13-SOURCE-TOOLS.json](VERIFICATION-2026-10-13-SOURCE-TOOLS.json).
Full logs, scripts and measurement identities remain in the ignored local
`builddir/codex-2026-10-13/source-tooling/`; the candidate's final commit/tree
and local signature evidence are recorded in the PR.

## Limits and remaining owner work

MAINT-02 remains partial. Source-release layout/build/install/license checks,
Meson dist adoption and predecessor retirement are still explicit separate
gates. This checkpoint does not claim whole-source archive acceptance on the
five guests without Git. LIB-03 still owns real package/symbol reachability
and advisory decisions. Unchanged runtime acceptance evidence is reused;
no native WM/X/full matrix was repeated for this developer-only change.
Physical/native release evidence and release publishing remain separate.
