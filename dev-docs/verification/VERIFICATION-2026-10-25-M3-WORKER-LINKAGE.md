# M3.1: image worker dependency ownership

Cam explicitly accepts M2's session evidence and documented limits on
2026-10-25, authorizes merge/cleanup of PR203 and starts M3. Canonical merge
`9b395cb49f14b460c5db80ebe9ae9576d69a3e5f` retains verified signature, author DCO
and exact PR tree; its completed branch is retired. M2's owner gate is complete.
Previously documented native/hardware/release limits remain separately owned.

The private image worker and direct decoder test now name their actual
dependencies instead of `all_found_deps`. Neither links `libfvwm3`; this is a
complete small owner slice, not a declaration that all M3 linkage is resolved.

| Owner | Compile requirement | Runtime link requirement |
| --- | --- | --- |
| Decoder/scale sources and direct renderer test | X11/XRandR headers for X types/scale metadata; enabled SVG headers | Full enabled librsvg and existing selected backend package dependencies |
| Worker software effects | Same headers plus X image APIs | Renderer dependencies plus real X11 calls |
| Native capture | Existing explicit X11/PNG declarations | Unchanged; retained comparator |
| Core/modules | Existing shared archive and dependency bags | Unchanged ownership pending object/archive mapping |

`Fsvg.h` no longer imports the unrelated X/Picture umbrella headers. Its
consumers own those types themselves; `image_svg.c` explicitly imports `Xmd.h`
for `CARD32`. This removes accidental Xft/FreeType and other desktop header
requirements from the worker, including SVG-disabled effects. Full librsvg and
selected backend dependency objects retain GLib/GIO/GObject/Cairo and package-
declared static/transitive requirements. Backend choices, version floors,
global math/system policy, feature flags, worker limits and rendering code stay
unchanged. There is no package/toolkit replacement or M4 backend retirement.

## Measured link ownership

The [machine report](VERIFICATION-2026-10-25-M3-WORKER-LINKAGE.json) binds the
five-file overlay, before/after compiler and linker commands/maps, object
undefined symbols, dynamic entries, recursive runtime paths, target sizes,
package/version/license metadata and source/profile identities. Raw maps and
captures remain under `builddir/codex-2026-10-25/m3-target-linkage/`.

Library operands below count the preserved global `-lm` as well as dependency
library paths. They are linker inputs, not loaded-library/package counts.

| Target/profile | Before → after link-library operands | Before → after bytes |
| --- | ---: | ---: |
| Linux AMD64 default worker | 23 → 8 | 103000 → 103000 |
| Linux AMD64 default direct decoder test | 23 → 7 | 63528 → 63528 |
| Linux AMD64 SVG-disabled worker | 14 → 2 | 56848 → 56848 |
| FreeBSD ARM64 default worker | 24 → 9 | 102848 → 102816 |
| FreeBSD ARM64 default direct decoder test | 24 → 8 | 66440 → 66408 |
| FreeBSD ARM64 SVG-disabled worker | 15 → 2 | 64888 → 64888 |

Direct `DT_NEEDED` sets and resolved recursive runtime library paths remain
unchanged in every measured case, both with the qualified private PNG provider
environment and without its `LD_LIBRARY_PATH`. Link order and redundant runpath
entries may change. Cairo/librsvg still retain their real transitive libraries;
the complete project still needs dependencies used by other targets. No runtime
package removal, meaningful size/speed improvement or licensing reduction is
claimed. Existing package/license metadata is an inventory, not a new legal or
vulnerability assessment. No dependency version or third-party payload changes.
`pkg-config --static` closure is recorded; a static librsvg binary is not tested.

## Focused verification and remaining gates

One frozen combined gate passes on Linux AMD64 (Debian13/QEMU TCG) and early
FreeBSD 15.1 ARM64, using preserved owner-controlled default/reduced builds:

- Reconfigure only non-installed live-X fixtures, collect baseline records, then
  apply the five-file overlay and compile affected core/root/library consumers,
  worker, direct decoder and existing image probes. Generated config hashes stay
  exact; SVG/PNG/NLS and other product features do not change within a profile.
- Five default and four reduced pure tests per OS pass: SVG protocol/client,
  effect arithmetic/protocol, and enabled direct SVG rendering. No test is
  skipped or relabeled as a pass.
- Each OS passes default software-effect X, default SVG isolation/recovery/
  cancellation X and SVG-disabled software-effect X. The two default SVG runs
  each retain seven passed checks. These use exact worker copies staged at the
  introspected Meson install path; copied target hashes match tested binaries.
- One additional `Fsvg.h` test consumer, `picture-cache`, compiles/links/runs on
  the existing Linux ARM64 lab with an isolated new-header/source overlay and
  its unchanged native archive. The dependency file confirms the new header.
  This is one header-consumer check, not third-target worker-graph qualification.

All ten other `Fsvg.h` consumers are source-reviewed for their own X/Picture
imports. Production consumers and the direct image-input probe compile through
the two main gates; the additional cache unit covers the remaining test source.
The passed group contains eighteen main pure cases, six real-X invocations and
the additional cache unit. No UI/layout feature changes, repeated theme capture,
Go race run or sanitizer matrix is selected for a header/link ownership change.
Full native sanitizer and physical/release gates retain their existing scope.

These are exact target copies using Meson's install mapping, not a fresh complete
`meson install` or offline install qualification. Full Linux/FreeBSD/OpenBSD
AMD64/ARM64 default/reduced/optional and install acceptance remains required at
the combined M3/#184 closure. #184/#49 stay open. Next map shared archive object
extraction and target consumers before narrowing core/module dependency bags;
moving their target lists alone would retain hidden archive propagation.

## Review, integrity and cleanup

Separate analysis, implementation and source/evidence review preserve the finite
worker scope. Requested routing remains unverified because observed model/client
metadata is unavailable; agent review is not independent human approval.
Repository check/lint and working/staged whitespace checks pass. No executed
implementation file changes after the frozen gate; docs/status additions follow.

Both archives pass full gzip CRC, retaining 126 stored payloads. All 124
manifest-listed payload hash/size/mode comparisons pass. Retire only 130
verified completed capture/upload/archive duplicates
(1,947,169 bytes), preserving source, cached builds, canonical install stages,
VM disks, other owners and canonical host evidence. Captured target copies remain
in the host archives. Both owned VMs power off normally with QEMU status zero.
The FreeBSD boot initially waited for read-only port/disk checks; the successful
start verifies its new owned PID controls the listener before guest control.

The production cadence is 4/5 after PR200 and the evidence-only M2 checkpoints.
This implementation advances it to 5/5 on merge; the next periodic comparison
is then due. No performance reference is reset by the current link inventory.
