# Desktop root publication — 2026-10-21

EWMH-01 R07/R09/R10/R12, with DATA-01 / INPUT-01. Base: merged PR145,
`1af7d380a1e3457c2b317cf1378cccba132f0764`.

Three related repairs validate root pager indices/formats before conversion,
prepare accepted count changes before publication, and replace mismatched
256-entry viewport/work-area arrays with one bounded effective-count snapshot.
Count requests that would shrink past the current or populated desktop are
refused; no client action/relocation or configuration migration is introduced.

[EWMH 1.5 root properties](https://specifications.freedesktop.org/wm/1.5/ar01s03.html),
sections 3.3/3.5/3.6/3.9, define dependent lengths and allow refusing count
requests. Timestamp zero remains accepted. Pure policy validates each record
count against 1024 desktop records and the server's XChangeProperty header and
payload capacity before allocation/indexing. The bound applies to publication,
not configured FVWM desktop numbers; an unrepresentable state retains the
preceding root snapshot and reports refusal. No request becomes a multi-property
atomic transaction. Existing global geometry remains; reserved-edge/page/dock
semantics and multi-monitor root authority are not qualified by array lengths.

Focused Linux GCC ARM64 and FreeBSD ARM64 pass one pure policy and three
independent real-X pager groups. Preserved PR145 installed bytes fail count
adoption, valid root current-desktop requests and initial dependent-array
lengths independently. The first focused configure failed on a new Meson
include-variable typo; source/logs are retained and only affected checks rerun.

Three pager-index, count-adoption and bounded dependent-array repairs pass 2340
standard / 22 selected optional entries across six targets, four scoped
sanitizer entries, ten fresh installs and four authenticated BSD displays.
Original configure/manual-observer failures and independent old-core controls
remain explicit.

| Gate | Passed |
| --- | --- |
| Linux ARM64/AMD64, GCC/Clang | 299 standard entries per profile |
| FreeBSD/OpenBSD 8.0, ARM64/AMD64 | 286 standard entries per target |
| Linux software/minimal | 22 selected entries combined |
| Fresh staged identities | 150 ELF, 77 catalogs, 700 data files |
| Staged real-X | 30 desktop groups, 30 manager groups, 20 geometry and ten style groups |
| Default BSD Xorg/Xenocara | 44 groups on four authenticated displays, including twelve desktop groups |
| Scoped Linux guest ASan/UBSan | Four entries: pure desktop policy and three pager groups |
| Portable `make verify` | 177 passes / 14 capability skips, one run |

Each pager group uses an independent real X client. Count coverage includes
accepted growth/empty shrink, zero/negative/over-maximum/large/format refusal,
and refusal with the current or a client on a high desk. Current-desktop
coverage observes both the root property and a real newly managed client's
desktop, including timestamp zero and malformed/out-of-range refusal. Array
coverage checks startup, 301/401 desktops, actual-pixel page changes, oversized
configured publication refusal, continued commands and representable recovery.

All 2,128 frozen regular source inputs pass native hash/mode postchecks. Ten
profiles preserve generated configuration hashes. Canonical archives pass
EOF/CRC, full payload/result and source identity validation. Every staged native
consumer matches its tested bytes after Meson's platform RPATH adjustment.
Default displays refuse unauthorized clients and stop with status zero. Both
owned OpenBSD 8.0 jobs stop cleanly and remove their ephemeral keys.

The first focused configure failure retained a previous Meson test log in its
capture; those rows are explicitly marked unexecuted and never counted as new
results. Corrected focused runs and scoped ASan/UBSan each execute four passing
entries. Sanitizer exit codes distinguish findings from ordinary process refusal;
three sanitizer binaries retain their tested hashes. All focused non-document
inputs match the combined candidate; new manual/Markdown text adds no compiled
source change. The separate native-CPU sanitizer release gate stays open.

The final observer initially searched the main fvwm3 page for command-section
text, after every Linux ARM64 runtime/profile/install check had passed. Its
original error/archive remain preserved. The corrected observation reads the
same generated fvwm3commands and fvwm3all bytes; no generation or runtime suite
is repeated. All 26 pages verify, and both affected command/combined pages render
with native FreeBSD mandoc. Source review checks validate-before-narrowing,
request-header arithmetic, finite storage/indexing, one-buffer cleanup and
publication ordering. Added C/header lines meet 80 columns. The final evidence
updates change Markdown and this receipt only; no full matrix is repeated.

Implementation: `ef28786fbb4d4f09c5ab67b4f34fbb0fa12f558f`
(signed author-DCO). Frozen source SHA256:
`df32ca01635bcfa4691bde67e8010c0d6c2f46f1468d640e75ff4936b1945788`.
[Machine receipt](VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.json) binds native
commands/results, original configure failures, corrected focus, old-core
controls, sanitizer identities, installed outputs, default displays and manuals.
Canonical raw evidence is under
`builddir/codex-2026-10-21/desktop-publication-safety`. Exact completed capture/
archive duplicates on persistent Linux/FreeBSD guests and temporary host BSD
build-cache copies are retired only after canonical identity verification.
Sources, reusable builds, stages, performance references and VM disks remain.

The PR144 performance reference has one subsequent merged implementation
checkpoint (PR145); no comparison is due and this unmerged PR does not count.
Performance tuning remains deferred. Reserved-edge/dock work-area semantics,
other desktop geometry, multi-monitor root authority, broader EWMH/selection
roles, native-CPU/physical/toolkit/compositor and final release gates remain open.
Property writes preserve X request order, not multi-property atomicity. An
oversized configured state keeps its previous root snapshot; this checkpoint
makes no complete conformance claim for an unrepresentable state.
