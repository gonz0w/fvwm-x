# M3.2: core and archive dependency ownership

Merged PR204 (`b53609df8f67c37c358f2effe5a4a3d0f620290c`) retains verified
canonical signature, author DCO and exact candidate tree; its branch is retired.
This checkpoint gives the existing shared archive compile-only requirements and
makes runtime linkage explicit for the core and four related native modules.
M2's owner acceptance remains complete. M3/#184/#49 remain open.

| Consumer | Runtime dependency owner |
| --- | --- |
| Core | Full X11/XRandR/Xext, Xft/Fontconfig and enabled text/NLS/session/image dependency objects, including both SM/ICE and legacy SVG initialization |
| FvwmAuto / FvwmEvent | Extracted libc-only pipe/parser/logging helpers; existing global math policy |
| FvwmMFL | Full libevent dependency plus shared system policy |
| FvwmRearrange | Full X11/XRandR dependency objects for extracted FScreen calls |
| Other archive consumers | Existing explicit all_found_deps, pending their own owner groups |

`all_compile_deps` projects existing include directories and compile arguments.
It preserves legacy umbrella-header visibility while preventing the static
archive from silently propagating every package's runtime link requirements.
There is still one archive with ordinary `link_with`. Full runtime dependency
objects preserve package-declared static/transitive requirements. Core retains
librsvg/backend dependencies for supported older `rsvg_init`/`g_type_init` paths,
even when current GLib makes initialization a no-op. Xft's package closure
retains its actual FreeType transitives. No new library/toolchain/version,
backend retirement, feature or configuration migration is introduced.

## Measured ownership

The [machine report](VERIFICATION-2026-10-25-M3-ARCHIVE-LINKAGE.json) binds the
six-file overlay, archive receipts, package versions, selected target sizes,
link operands, extracted archive objects, direct/recursive runtime closure and
install mappings. Full compiler/link argv, maps, undefined symbols and logs are
preserved in the two canonical local archives under
`builddir/codex-2026-10-25/m3-archive-linkage/`.

| Target | Linux default / reduced library operands | FreeBSD default / reduced library operands |
| --- | ---: | ---: |
| Core | 23→21 / 14→12 | 24→21 / 15→12 |
| Auto and Event, each | 23→1 / 14→1 | 24→1 / 15→1 |
| MFL | 23→2 / 14→2 | 24→2 / 15→2 |
| Rearrange | 23→3 / 14→3 | 24→3 / 15→3 |
| Three archive-only tests, each | 23→1 / 14→1 | 24→1 / 15→1 |

Operands include existing `-lm`; they are linker inputs, not loaded libraries.
Across the four profiles, every compiler command, generated config, all ninety
archive object identities and the archive hash remain exact. The default /
reduced archive consumer inventories contain 138/128 Linux and 136/126 FreeBSD
targets. Exactly eight link commands change in each profile: core, four modules
and the three archive-only tests. The other 130/120 and 128/118 consumer commands
remain exact. The unchanged Rearrange probe is built/executed for its selected
X checks; other unchanged consumers retain prior coverage. This retention uses
unchanged source/commands and archive inputs; direct object/binary hashes for
those retained consumers were not captured.

Every selected target extracts the same archive object set before/after and has
the same direct `DT_NEEDED` and resolved recursive library sets/paths. Core's
SM/ICE ordering changes without removing either owner. Clean-loader records and
Meson's unchanged install mapping are captured separately; no installed target
copy or full `meson install` is claimed. Linux file sizes remain unchanged.
FreeBSD default Auto/Event/MFL/Rearrange shrink by 144/160/48/32 bytes;
reduced Auto/Event shrink by 112 each. Core size remains exact. The machine
report retains test sizes and binary hashes; equal size does not imply equal
binary bytes. No meaningful runtime-size, package-removal or speed benefit is
claimed. Package/license inventories from PR204 remain retained; fresh system
package versions and static linker metadata accompany this group. The version
supplement uses ambient pkg-config paths; it does not requalify the private PNG
provider, whose actual prefix is bound by the build environment/maps/closure. No package payload
changes, legal reassessment or vulnerability assessment follows.

## Focused acceptance

One frozen combined checkpoint passes on Linux AMD64 (Debian 13, QEMU TCG) and
early FreeBSD 15.1 ARM64, reusing default/reduced owner-controlled caches:

- Reconfigure, capture baseline objects/commands/maps, apply one six-file overlay,
  then rebuild the eight changed consumers and existing Rearrange probe.
- Nine registered entries per profile pass: event-loop policy, image effect,
  image-effect protocol, Auto/Event transport, MFL request/stream X and Rearrange
  signal/monitor X. Total: 36 OK, zero failed/skipped; sixteen real-X invocations.
- Core/MFL actual-X paths execute the changed production binaries. Rearrange's
  X checks execute its existing broad-dependency probe; production minimal
  linkage is established by successful link, extraction and loader evidence,
  not direct execution in those probes. No C/runtime/test source changes.

No new UI/layout contract changes, so no theme/screenshot repeat is selected.
Existing tests cover behavior; no test merely asserts Meson source text.
Full Linux/FreeBSD/OpenBSD AMD64/ARM64 default/reduced/optional, offline install
and remaining static/package acceptance stays at combined M3/#184 closure.
Native Linux sanitizers and physical/toolkit/compositor/release gates retain
their separate scope. This focused checkpoint does not close #184/#49.

The [due PR194→PR204 comparison](PERFORMANCE-2026-10-25-PR204.md) uses existing
ARM64 browser binaries with five alternating pairs, no rebuild. Overlapping
ranges establish no consistent speed change; current browser file growth is
explicit. PR204 becomes reference 0/5; this implementation becomes 1/5 on merge.

## Review and cleanup

Separate routed analysis, six-file implementation and source/evidence review
find no actionable defect. Requested routing remains unverified because observed
model/client metadata is unavailable; agent review is not human approval.
Repository check/lint and working/staged whitespace checks pass. Production
Meson hashes remain exact after native freeze; only reports/status follow.

Both canonical archives pass gzip CRC to EOF, all 204 manifest-listed payload
hash/size/mode checks and two additional manifest-file hashes (206 payloads).
Only 210 completed guest capture/upload/archive duplicates retire, totaling
16,978,140 bytes. Source, cached builds, stages, disks and canonical host archives
remain. Both owned VMs power off normally with QEMU status zero. The separate
performance run retires two exact host/guest-verified input duplicates (5,622,994
bytes) and its temporary fixture; the shared ARM64 lab keeps running.

Next: batch remaining module/runtime owners by extracted archive requirements,
then perform the required accumulated M3 platform/install closure once. Keep
M4 backend retirement and unrelated source/tooling owners outside this batch.
