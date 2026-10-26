# R1 reference profile checkpoint

Owners: #209/#29/#226. Three desktop and three instrumented WM runs pass on
Linux ARM64, using existing observers and one normal GCC cache. Numeric
envelopes are **empirical proposals**, pending review; no complete resource
budget or R1 release acceptance is claimed. The
[structured report](VERIFICATION-2026-10-26-R1-REFERENCE-PROFILES.json) includes
normalized receipts, raw-key mappings, all run commands, ELF sizes and costs.

## Source and execution identity

PR256 merged as `b263edf8fdf744fe545fa9af6d9685cf2b705375`, with reviewed
tree `4f2f1bcfedbd449e7e2173a25b35b8c741027325`. PR261 merged as
`1748e2f41199d996c97f47f84e047249fdec3394`, with reviewed tree
`cf98af06d06f19ab4a2cd63138254506d821537b`, closing #208. Canonical
signatures and author DCOs were verified. Both PRs/Project entries are Done;
merged local/remote branches are retired after roadmap links were pinned to
the accepted commit. #209/#29/#226 are In progress.

All 1,329 selected runtime/config/observer file hashes match the owned Linux
source before and after observation. Parked-file checks compare identity only;
no parked source inspection, implementation or runtime test was selected. The
normal incremental build relinked only FvwmMFL; other selected workload targets
were current. `meson install --no-rebuild` prepared a fresh **measurement
prefix** containing cached installed outputs. It is not a clean source/install
qualification. Meson's install fixups mean staged hashes, rather than build-path
hashes, identify executed artifacts.

Environment: ARM64 macOS host; ARM64 AppleHV Linux VM/container; Linux
6.18.10-200.fc43.aarch64, GCC 13.3.0, Go 1.27.2, Meson 1.7.0, Ninja 1.11.1.
Native ARM64 instructions inside a VM are distinct from physical hardware tests.
Normal debugoptimized binaries have no sanitizer/LTO/PGO instrumentation.
Desktop uses Xvfb 1440×900×24; instrumented WM uses 1280×720×24.

## Available observations

| Metric | Three-run observation | Proposed envelope |
| --- | --- | --- |
| Desktop settings/bar ready | 108.330–118.267 ms | 128.205137 ms |
| Desktop final synthetic key/Echo median | 0.317355–0.367772 ms | 0.418189 ms |
| Instrumented idle core command median | 0.297043–0.366731 ms | 0.436418 ms |
| Instrumented resize core command median | 0.264272–0.274314 ms | 0.284357 ms |
| Core process lifetime HWM through cases | 7,216–7,236 KiB | 7,256 KiB |

Raw precision and all idle/title/resize CPU/command/timer rows are in the
[WM receipt](R1-BASELINE-2026-10-26-MINIMAL.json); desktop results are in the
[desktop receipt](R1-BASELINE-2026-10-26-DESKTOP.json). Core HWM is the kernel
per-process lifetime high-water mark, not a sampled whole-session peak. Command
and XTEST key/Echo latency do not measure physical input-to-photon or redraw.
Scheduled-command timing includes the requested 10 ms delay. CPU uses 100 Hz
ticks; RSS HWM uses 4 KiB pages. No arbitrary percentage cap is introduced.

Each desktop run exercised 12 seconds idle, three settings-editor cycles and
twelve settings queries, plus the existing fixed application/search/files/input
workload. Each WM run exercised idle/title/resize with 128 samples per case and
requested load 10,000/s. Actual achieved rates and elapsed times are preserved.
The six runs took about 68 seconds in total, excluding preparation/archival.
No all-OS/compiler/install/theme matrix was run. There is no new product UI;
the existing observer's menu captures remain in raw evidence.

## Cost population and unavailable rows

The common measurement install has 100 regular files totaling 61,114,438 bytes,
including data/debug information. File sizes are separate from ELF text/data/bss.
The all-installed-ELF resolved `ldd` file union has 59 distinct objects totaling
63,469,736 bytes. This includes dependencies of unstarted programs; it is not
actual loaded mappings, RAM or all build-tool dependencies. Configured C
dependency versions and per-artifact sections are retained. No optional service
increment or minimal-build cost is invented from this common install.

Idle proc coverage is complete in run 1 (206 samples), partial in all 208/207
samples in runs 2/3. The normalized whole-tree RSS/PSS/CPU rows are therefore
conservatively partial and have **no envelope**. Unseen ephemeral descendants
remain outside sampling even when an individual snapshot reports complete.
PID/start identities are recorded; the observer does not emit an executable
map for each PID. Launch binaries and installed configuration have separate exact
identities. No combined core-plus-module cap is inferred from core-only HWM.

Existing cgroup/trace endpoints were not requested; true wakeups and charged
memory peak are unavailable, never zero. Context switches are not wakeups;
charged peak is not RSS/PSS. Complete session process accounting and remaining
geometry/redraw metrics belong to #29; complete target/profile/dependency and
archive/install/offline costs belong to #226. Physical qualification belongs
to #5. #209 remains open for the accepted foundation handoff.

Historical M2 observations, PR253's periodic component reference and M4 installs
retain their original source/platform scope; none are pooled into these repeats.
The PR253 counter is 3/5 after PR256; planning-only PR261 does not increment it.
This reference campaign claims no before/after performance improvement.

## Retained evidence and review

Canonical host directory:
`builddir/codex-2026-10-26/r1-foundation/` under the shared outer workspace.
`native-reference.tgz` is 32,542,618 bytes with SHA256
`dcc88b0d1ca194ddfd91c9b0482b51bc693986043b3c30b55c146b06ad680ecf`.
All 157 regular-file identities in its inventory were verified on the host,
including the complete measurement prefix. Raw receipts, commands, process
cleanup journals, input hashes, build provenance and screenshots are preserved.
The source/build caches and older accepted stages remain intact.
After host verification, this batch's guest payload/prefix and duplicate archive
were removed (99,288,590 regular-file bytes). The owned lab was restored stopped.

Sol/high implemented the read-only helper and ten focused tests; a separate
Sol/high review found no actionable code defects and independently passed those
tests. Requested routing metadata is recorded, not independently attested model
execution. Final independent documentation/receipt review verified all numbers,
1,329 source hashes, artifact identities and archive entries. Its one finding
was missing published context-hash preimages; the structured report now includes
the exact inputs and canonicalization. A repeat read-only review reproduced all
digests from checked-in data and found no remaining actionable defects.

Ten helper tests, CLI processing of both actual receipts, `make check` and
working/staged whitespace checks pass. An initial README check exceeded the
250-word limit; shortening the link text fixed it, with failure retained in the
host evidence. This documentation correction did not rerun the native campaign.
The initial implementation-agent capacity failure produced no source edits;
a retry on the same approved model succeeded. No additional OS test campaign
was needed for this developer-only helper and reference documentation.
