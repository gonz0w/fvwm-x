# Fragmented XCB replies and installed package closures

G15 continues [#211](https://github.com/gonz0w/fvwm-x/issues/211) and
[#212](https://github.com/gonz0w/fvwm-x/issues/212) after
[PR364](https://github.com/gonz0w/fvwm-x/pull/364). Its source base is
`b076e3f309be463435bc70cccb3154e515a93726`. The selected batch qualifies
private partial-read observation, interprets installed Linux package graphs
and prepares strict OpenBSD graph validation. No production renderer, library,
configuration or installed runtime changes follow.

## Fragmented-packet qualification

The private observer uses ABI 2 with separate optional atomic acknowledgment
pointers for logical and partial-read wait hooks. Both pointers are borrowed
only within an active scope and cleared at scope end. Partial acknowledgment
requires the matching connection and an active reply context. The unit covers
inactive, wrong-connection, absent-context and completed-scope cases alongside
the existing invalid/overflow and record-limit checks.

`fragment_probe.c` uses an owned Unix socket pair and one peer thread per
control. It never forwards display traffic. The peer validates the exact
unauthenticated X11 setup and one 24-byte GetProperty request. A 32-byte reply
header declares a known 16-byte payload. The fragmented control waits for the
logical hook before sending the header, then waits for the actual partial-read
hook before sending the payload. Each reply's sequence, type, format, length,
contents and completed observation record must match. The peer remains open
until the caller acknowledges reply consumption and scope completion.

The complete-reply negative queues all 48 bytes before observation. Its partial
counter must be zero. Socket availability does not establish an XCB-buffered
reply, so this control does not require zero logical waits; the retained
buffered control supplies that separate expectation. Peer I/O and atomic waits
have three-second bounds, and the process has a 30-second alarm. Missing hook
acknowledgments release the withheld body and fail the control.

The single native Linux ARM64 run passed:

| Control | Reply contexts | Logical waits | Wait calls | Partial-read calls |
| --- | ---: | ---: | ---: | ---: |
| Fragmented property | 1 | 1 | 1 | 1 |
| Complete property | 1 | 1 | 1 | 0 |
| Asynchronous request | 0 | 0 | 0 | 0 |
| XCB-buffered reply | 1 | 0 | 0 | 0 |
| Outstanding direct XCB | 1 | 1 | 1 | 0 |
| Outstanding Xlib | 2 | 1 | 1 | 0 |
| Checked void | 1 | 1 | 1 | 0 |
| Other connection | 0 | 0 | 0 | 0 |

The private library and both probes compiled, and the assertion-enabled native
observer unit passed. The original six controls ran against owned Xvfb; the
fragment fixture needed no display server. The probes verified their API and
reply implementation resolve to the same private library, with Xlib's dependency
also checked by the existing probe. Protocol source/header generation completed
before private library compilation, preserving the qualified build ordering.
There were no native failures or retries in this ARM64 checkpoint.

The observation boundary remains one calling thread and connection. Partial
waits count entries into the instrumented EAGAIN branch while processing a
reply; they are separate from logical waits and do not establish kernel sleep
or whole-process multithread coverage. The fixture qualifies the private seam;
FVWM draw-operation integration and complete implicit waits remain open.

## Installed Linux package graphs

The new Debian adapter requires nine fields from a complete installed snapshot:
`binary:Package`, `Version`, `Installed-Size`, `Depends`, `Pre-Depends`,
`Provides`, `Status`, `Architecture` and `Multi-Arch`. Native and foreign
architectures are captured separately. The current adapter accepts one native
architecture and architecture-independent packages. Historical seven-field
captures are refused; architecture facts are not inferred from names.

For each Depends/Pre-Depends group, the adapter selects the first satisfiable
declared alternative, preferring an installed real package over one unique
installed provider. Versioned dependencies and versioned Provides are checked.
It retains resolution provenance and rejects missing dependencies, ambiguous
providers, unsupported architecture restrictions and malformed or oversized
metadata. This is a reproducible installed satisfier closure. It does not claim
to recover the package manager's original or only possible installation plan.
Recommends and Suggests are outside this declared dependency scope.

Package bytes are Installed-Size KiB converted to bytes. The existing closure
calculator deduplicates packages across exact library-owner roots. All eight
comparison executables and their libraries match the retained G6 identities.
Owners are matched through the installed package file lists and canonical paths,
including merged `/usr` aliases. Package metadata and file-list identities match
before and after capture. The exact private PNG path is accounted separately.
No package was installed, removed or upgraded.

Linux ARM64 has 529 installed packages and 1,714 resolved dependency edges.
All 26 independent native `dpkg --compare-versions` cases agree with the adapter,
including reverse comparisons, epochs, tildes, numeric portions and equal forms.

| Linux ARM64 closure | Native bytes | Candidate bytes | Added bytes |
| --- | ---: | ---: | ---: |
| Buttons drawing | 36,754,432 | 39,057,408 | 2,302,976 |
| Form drawing | 36,370,432 | 38,673,408 | 2,302,976 |
| Buttons layout | 36,754,432 | 36,754,432 | 0 |
| Buttons drawing with recorded librsvg package in both root sets | 106,106,880 | 106,106,880 | 0 |

The drawing increment comprises Cairo, Pixman, XCB Render and XCB SHM packages.
Requiring the recorded librsvg package in both root sets already includes those
packages. This conditional root-set comparison does not imply zero file, RSS,
startup or maintenance cost. Package boundaries and declared dependencies vary
by OS; the retained FreeBSD totals keep their original scope.

## Linux AMD64 capture correction

The initial data-only capture completed its installed metadata, exact owners,
library identities and version oracle. Its wrapper then failed an incorrect
500-package expectation before the final executable recheck. The original G6
raw manifest independently contains the same 668 package names and versions
as the fresh capture. Its SHA-256 is
`107997b3c7d089dd0f9fd417f99b52ed86e57959ffa0eafd8d71bbf82a796dcb`.
The selected correction pins that provenance and uses the correct count,
without changing the collector, graph expectations or library identities.
The first run's 165 diagnostic files and archive match their hashes. All 82
metadata command groups retired, QEMU exited normally with status zero and
the full backing chain retained its admission identities.

A predispatch review also found that the reused wrapper carried a BSD-only
shutdown option. The prior Linux G6 receipt showed its rejection. The Linux
command was corrected before dispatch; neither G15 capture used a forced stop.
No native attempt was spent on that predispatch correction.

The reviewed correction passed its one data-only retry on Linux AMD64 under
TCG. It captured all 668 packages and 2,171 resolved dependency edges. The
same 26 version cases agree with native dpkg. All eight executable identities
were rechecked after capture, along with libraries and package/file-list data.

| Linux AMD64 closure | Native bytes | Candidate bytes | Added bytes |
| --- | ---: | ---: | ---: |
| Buttons drawing | 27,462,656 | 29,934,592 | 2,471,936 |
| Form drawing | 27,206,656 | 29,678,592 | 2,471,936 |
| Buttons layout | 27,462,656 | 27,462,656 | 0 |
| Buttons drawing with recorded librsvg package in both root sets | 66,712,576 | 66,712,576 | 0 |

The increment contains the same four package names as ARM64, with the AMD64
package sizes. This is metadata collection in an emulated CPU profile, not a
WM runtime or timing comparison. All 165 exported files and the archive verify;
all 82 metadata command groups retired. QEMU exited normally with status zero,
with no forced stop, and the full five-image backing chain and read-only seed
retained their admission identities. The qualified child remains the current
capture. The superseded failed child was retired after verifying its diagnostics,
closed-file state and absence from the replacement chain, freeing 34,865,152
allocated bytes. Compact failure evidence remains.

AMD64 initial plan SHA-256
`4b895543011522e0613d763afe76ef4b18c3f904383f96695385410d0d0d44de`.
Successful correction plan SHA-256
`e81dc93e0819470f7cc2a5b43ac0760ae41358fcc3cff3b34c2da5386eb47c21`.
Successful dispatch receipt SHA-256
`d59d67b29def38f721d9bd3b681892f84fe6c97d0987078e15efb7bfb4642e62`.
Package snapshot SHA-256
`585c8388b81bd3d6ced6e3dfb6133c5d60bd5eaaa592a1b72a74271b7f9a2d37`.
Comparison result SHA-256
`58b84756ff2344ac69c7079af44c8a8abbd1c3c79497f0ffa54c5f37af013e38`.
Artifacts are in `platform-packages/repair01/linux-amd64-run01` under this
checkpoint's artifact root. The failed run remains in
`platform-packages/linux-amd64-run01`, with only its retired child image absent.

## OpenBSD capture boundary

The OpenBSD adapter consumes captured `+CONTENTS`, `+REQUIRING`,
`+REQUIRED_BY` and one `pkg_info -q -s` size estimate for every installed
package. It uses the package database's resolved names rather than interpreting
package-spec patterns itself. Forward and reverse edges must agree across the
whole snapshot, every target must exist and packing-list identities must match.
Missing dependency files remain explicit absent values; a missing size never
becomes zero. Packing-list instructions remain data and are never executed.
The byte scope is the package tool's estimate, not measured disk allocation.
See the [package manual](https://man.openbsd.org/package.5),
[pkg_info manual](https://man.openbsd.org/pkg_info.1) and
[installed dependency-list implementation](https://github.com/openbsd/src/blob/master/usr.sbin/pkg_add/OpenBSD/RequiredBy.pm)
for the planning references. These references do not substitute for native
source and snapshot qualification.

Seven host policy tests pass, including shared closures, explicit zero sizes,
identity/size refusal, missing and inconsistent edges, duplicate names, bounds
and inert packing-list instructions. No OpenBSD package graph was captured in
this checkpoint. Retained ARM64 and AMD64 guests have a concrete reuse gap:
their earlier canonical jobs replaced guest authentication with temporary job
keys, and cleanup retired those host keys. Their current status records and
missing-key observations are preserved. Credentials and boot configuration were
not changed to reuse them. The prepared data-only collector remains unexecuted.
A later bounded checkpoint can select fresh owned profiles through the existing
canonical qualification route. Both OpenBSD closure calculations remain open.

## Verification and retained artifacts

Nine Debian adapter host tests and seven OpenBSD adapter host tests pass. The
host macOS observer unit passed before the native run; native Linux ARM64
separately establishes the current observer unit and both probes. Unchanged
source patch-installer checks retain their prior source-bound evidence.
No WM, install, broad platform matrix, theme or periodic whole-WM benchmark
was repeated for these private fixtures and package metadata. There is no new
user-visible UI. The whole-WM performance ledger remains at 4/5.

Artifacts live under `builddir/codex-2026-11-07/r1-xcb-fragments-packages`.
Linux ARM64's frozen plan SHA-256 is
`5cfb1ec83599eca410684cd9242f31dbd25c29ab8f7a18cbb8ed3fd12eb7b04c`.
Its dispatch receipt SHA-256 is
`deb2d2f246b5a28da0f90f12454dd17e3644eeb327ddd7b7e4814298fd3b148e`.
All 201 exported files and the archive match their hashes. All 16 outer command
groups and 82 metadata command groups retired. Container and machine shutdown
were confirmed. The package snapshot SHA-256 is
`45eac7327a75e0e047bfe8a71a1785cb4342356a4f3a16b9967d321ec7e7a203`.
The comparison result SHA-256 is
`06db272abf63b7763c832b9587b9a500570b507c9493d6ea24f202669c3a0ee4`.
The stopped container retains the latest private build at
`/tmp/fvwm-appearance-a4-20261102-01/g15-xcbwait01`.

Separate source and exact frozen-job reviews preceded dispatch. The attempt
budget was one run plus one diagnosed, separately reviewed correction per
selected profile. Shared native work remained serial, one owned guest at a time,
with the existing time/export limits. Source integration, broader consumers,
complete implicit waits, OpenBSD costs and release acceptance remain separate.
