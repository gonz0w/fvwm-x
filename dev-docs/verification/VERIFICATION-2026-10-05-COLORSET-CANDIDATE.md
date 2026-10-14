# Sparse colorset candidate — 2026-10-05

Status: historical focused candidate checks. The user approved 4096 distinct
sets per process, a build-time override up to 65536, and copying set zero for
undefined module IDs on 2026-10-05. The [final report](VERIFICATION-2026-10-05-COLORSETS.md) records completed
Linux/BSD/default-X and installation gates. This is a partial verification
record, not a completed release or an interpreter-safety claim.

The prior [MiniScroll release pass](VERIFICATION-2026-10-05-SCRIPT-RELEASE.md)
is complete and retains its full Linux 70/70 and BSD 59/59 evidence.

## Candidate implementation

A common standard-C store keeps separately allocated records and a sorted ID
index. Lookup is binary search; insertions move bounded index entries without
moving record addresses. A single exact-hit cache serves consecutive rendering
field reads; missing-ID fallbacks are not cached. No library is added.

Core and bundled module dense indexing is replaced with nonallocating lookup.
Core allocation initializes only the requested record with its existing
black/gray or ID-dependent monochrome defaults. Module allocation clones zero
for a newly referenced undefined ID. Startup packets enumerate allocated IDs
in ascending order, beginning with zero. Root transparency and button redraw
iteration use real IDs rather than assuming dense indices. The binary packet
fields and numeric IDs are unchanged; the Perl tracker already uses a hash.

New-ID refusal occurs before committing colorset assignments or freeing prior
colors in migrated callers. Existing sets remain editable at capacity.
Core style, menu, decoration and expansion paths and bundled native modules
check admission. Negative disabling sentinels are preserved in options that
support them. Checked int32 conversion avoids narrowing oversized IDs. The
shared build option is colorset-limit, default 4096, supported range 1–65536.
This bounds records, not image, gradient, X-server or total process memory.

Supported compiler flags now reject implicit function declarations and
incompatible pointer types, so missing headers cannot silently truncate a
returned pointer on LP64. This is not a blanket warnings-as-errors setting.

## Completed checks

| Scope | Result |
| --- | --- |
| Linux ARM64 GCC focused core/modules and tests | 7/7 |
| Actual-X colorset fixture | 6/6 outcomes |
| Prior actual-X widget outcomes in focused run | 16/16 |
| Linux ARM64 GCC and Clang pure storage policy | passed |
| Linux AMD64 GCC pure storage policy | passed |
| FreeBSD ARM64 and AMD64 native C pure storage policy | passed |
| OpenBSD ARM64 and AMD64 native C pure storage policy | passed |
| Local macOS pure storage policy | passed |

Pure coverage checks sparse IDs through INT_MAX, reverse insertion/growth,
stable pointers, ordered iteration, refused creation without existing-state
loss, existing records at capacity, int32 parsing/overflow, zero/invalid capacity
and 4096/65536 configured counts. The module API test checks initialized zero,
cloning, Dump/Load, invalid packet IDs, count refusal and existing updates.

Actual-X checks verify isolated ID 100000 and INT_MAX without gap broadcasts,
live script colorset updates, actual core/module table saturation, overflow and
count refusal, unchanged visible managed-window styling, preserved script
assignment, existing updates/repainting at capacity and diagnostics followed by
WM availability. Startup packet reports record the complete allocated ID lists.
The fixture avoids enormous gap allocations and oversized images. A build
limit below three skips only this high-ID palette fixture; pure/API tests remain
enabled. The skip path is checked without opening a display.

The focused archive matches 156 frozen source fingerprints. The overlay
inherits the previous verified snapshot and adds owned colorset changes.
Unrelated desktop, launcher and session source/target changes in the shared
checkout are excluded. tests/meson.build is projected from the verified baseline
plus colorset targets, and both projection hashes are recorded. Permission
entries number 1155; full installation/source-mode checks are pending the full
matrix. Focused production hashes are recorded, not claimed to be installed.
Native Linux ASan/UBSan CI remains unrun. Temporary BSD snapshots are shut down;
unrelated desktop guests and Linux containers are preserved.

## Corrections and compatibility finding

Compilation caught a misplaced style cleanup label and misplaced guards in
Pager and Script color-changing paths; these are corrected. Missing Parse/log
headers in the new shared API then caused two actual-X failures. Required
headers are added, compiler checks are strengthened and focused checks pass.
Failed logs and selected candidate source archives are retained.

The initial new frame observer queried before its window became viewable and
received BadMatch. It now waits for viewability before capturing pixels. The
first capacity fixture used unchanged default colors, which populated the core
but sent no change broadcasts to the running script. It now sets a changed
foreground so both process tables actually reach capacity. These fixture
failures are retained. Initial source freezing also included unrelated current
desktop sources; the corrected overlay reads unchanged files from the verified
archive rather than the shared checkout.

A separate real-X comparison uses the previously verified installed core and
module and the current candidate. With zero red, set 100 defined and a widget
using undefined set 50, the old widget is gray (bebebe) and the sparse widget is
red (ff0000). Before/after hashes protect both variants. The first comparison
script used an invalid literal filename; it is corrected to a filename
variable. The observed failure is recorded in correction metadata; the raw
helper log was overwritten by the final successful comparison.

The [proposal](../decisions/COLORSET-STORAGE-PROPOSAL.md) recommends consistently cloning
zero for undefined module IDs. Preserving legacy implicit-gap gray behavior
would need additional compatibility handling. The count limit is approved;
the appearance decision is now approved under AGENTS.md's requirement for an
explicit decision on a specific compatibility break. No final full-matrix or
completed full-matrix colorset claim is made until the final gates finish.

Local evidence: builddir/codex-2026-10-05/colorset-storage/, including
focused-evidence.json, linux-arm64-gcc-focused-evidence.tgz,
candidate-before-zero-approval.tgz, candidate-before-zero-approval-fingerprints.json,
owned-overlay-files.json, build-definition-projection.json,
bsd-pure-policy.json, per-target pure-policy logs and gap-default-control.json.
