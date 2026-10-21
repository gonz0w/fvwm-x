# OpenBSD 8.0 release baseline — 2026-10-20

Status: composed local checkpoint passes; ready for PR review.
Baseline selected October 20; qualification completed October 21, 2026.
Owners: CI-01 #17, COMPAT-01 #3, IMAGE-01 #8 and LIB-07 #13;
related release verification remains with VERIFY-01 #5.
Base: merged PR #134, `3bb05ac2e79edfac9ae4bb0cec5ed36d2baf1f7e`.
PR #135 remains independently open.

The [accepted baseline](../decisions/OPENBSD-8.0-BASELINE-2026-10-20.md)
selects **OpenBSD 8.0 only**, on AMD64 and ARM64, retaining Linux and FreeBSD.
The [official release](https://www.openbsd.org/80.html) dates 8.0 to October 20.
The previously missing CI asset, SVG isolation and legacy-property entries run
and pass on both architectures, without omissions. Full configured installs,
staged geometry and authenticated default Xenocara pass. Earlier 7.9 failures
remain historical; these results do not isolate their cause or prove a repair.

## Changes

- Pin `anyvm-org/openbsd-builder` v2.1.1 8.0 images/profiles/observations and
  bootstrap identities. Independently verify publisher/download SHA256 and
  sizes. Ten non-OpenBSD asset/bootstrap records remain exact; retain old locks.
- Admit the selected release/architecture before guest preparation, and verify
  release kernel identity before native build qualification. An `8.0` uname
  does not admit an `8.0-current` or `8.0-stable` kernel. Update both gate mocks.
- Record VESA on AMD64 and wsfb on ARM64. Accept syspatch's documented status 2
  when no additional patches install, but require the signed pending list to
  be empty, including after an updater self-update. Other errors stay fatal.
- Reserve an uncontended Meson slot for the command and xlock CLI checks. They
  assert real elapsed-time limits; other Go tests must not consume their timing
  budget. Test commands/assertions and client five-second deadlines are unchanged.

No C/header, Go, Perl, module-protocol or supported configuration changes.
No runtime speedup or executable-size reduction is claimed.

## Combined qualification

[Machine-readable results](VERIFICATION-2026-10-20-OPENBSD-80-RESULTS.json)
retain source, archive, kernel, driver, patch, install and correction identities.
Counts below are Meson entries or named driver invocations, not every assertion
inside each driver.

| Gate | ARM64 | AMD64 |
| --- | --- | --- |
| Standard suite | 269 composed passes | 269 original passes |
| Optional Go suite | 17 composed passes | 17 composed passes |
| Full install | 15 code/RPATH + 70 data identities | 15 code/RPATH + 70 data identities |
| Installed Go utilities | 7 exact identities + direct CLI checks | 7 exact identities + direct CLI checks |
| PNG loader identities | 4 standard + 2 Go consumers | 4 standard + 2 Go consumers |
| Staged Xvfb geometry | 1x and 1.5x | 1x and 1.5x |
| Server-only Xenocara startups | 16/16 authenticated | 16/16 authenticated |
| Default-X staged interactive geometry | 1x and 1.5x | 1x and 1.5x |
| Default-X Go clients | 5 sessions / 6 client fixtures | 5 sessions / 6 client fixtures |

This is 538 standard and 34 optional entries, 44 authenticated default-server
invocations, four staged and four default-server scale invocations. Each
server rejects a client without its cookie; startup/readiness, application
outcomes and teardown are separate records.

One full standard and one full optional suite ran per architecture. Preserve
these original failures and compose only their affected corrections:

1. ARM64 standard: 268/269 pass. `verification-profile` omitted the release and
   kernel identities now required by the real helper. Fix its recording stub;
   three affected profile/gate/QEMU entries pass. The full suite is not repeated.
2. AMD64 optional: 15/17 pass. Command silent-peer and xlock producer-deadline
   cases exceed their outer process limits while other Go tests run. One
   unchanged isolated control passes both. After adding the two scheduling
   fences, both cases pass with the normal Meson scheduler on both architectures.
   Commands, timeouts, prerequisite sets and compiled executables are preserved.
   Retain 30 unaffected optional passes and use four fresh affected passes.
   Complete the previously blocked install and default-X steps afterward.

Both initial ARM64 preparation attempts remain recorded before any WM tests:
VESA-only inventory and no-new-errata exit handling. Local verification-tool
adjustments for Meson 1.12's RPATH API, Ninja log compaction and unordered
prerequisite metadata are retained separately from application outcomes.
No deadline, assertion, package signature, TLS check or hardening is weakened.

## Inputs and evidence

The final code/test/lock snapshot is 2086 regular inputs:
`ceccf853531842a19024efc2ce2307be6f62c79304290062becf59f50e9dbb36`.
ARM64's original standard run uses candidate4
`ba43b589f572e1947c9d8dc032f8654ce2ccdf3abfa3f2dce9a733425353471a`;
its gate-fixture correction and the other original runs use candidate5
`8fdbbbb5d52f1328cbdec5475dcb0becc131fa5caa80f89835c7fa6f9b895108`.
The candidate6 change adds only two scheduling flags, comments and guide upkeep.
Native before/after executable hashes and test-metadata comparisons bind the
retained runtime results. Final upkeep adds guides/evidence only.

The fresh guests use signed 8.0 packages and applicable day-one errata, followed
by a verified changed boot identity. ARM64 runs GENERIC.MP #1 with 001/002/004;
AMD64 runs GENERIC.MP #2 with 001/002/003/004. Package indexes and guest records
identify Meson 1.12.1v0, Python 3.14.7p0, PNG 1.6.59 and librsvg 2.63.2v0.
Private PNG 1.6.59 runs its 36 upstream checks per architecture; built/staged
consumers load the verified private library. Optional Go uses pinned 1.27.2,
local toolchain selection and vendored inputs. NLS auto disables because
`msgfmt` is unavailable: zero catalogs, and no translated-UI qualification.

Host evidence is under
`builddir/codex-2026-10-20/openbsd80-release-qualification/` in the shared
workspace. It includes the exact source archives/maps, executed scripts,
original ARM full-suite JSON, original optional failures and isolated controls,
package/update/reboot records and canonical captures. Full gzip CRC and every
capture node's path/type/mode/hash are checked. Build captures independently
verify all 85 configured installed outputs, seven Go utilities and six PNG
consumers per architecture against their receipts.

| Canonical archive | SHA256 |
| --- | --- |
| ARM64 test capture | `2980f00ccd3200f3e1d492ef6480e6dbbd2341b7d5ff33897f9bc9b54a680419` |
| ARM64 scheduling supplement | `787444b412469e28f21010b7925a46905ee46ad7c3f028e37e0ffb3ba5c6beab` |
| AMD64 test capture | `6a9c39aff87eb2ea2dbccbd7e6b5772008f01ecfd30e4c779eeba4e88ee2f535` |
| ARM64 reusable build capture | `74c56cefe163d0a62de651753b3014d6d7e80a55096432448a9f66cf8714afcd` |
| AMD64 reusable build capture | `8a88843bce7086e3f0652db84c0b1825d0d7a1c058e794857de6ea3988177722` |

Verified guest test-data duplicates are retired with exact manifest/hash guards.
Source trees, incremental builds, install stages, VM disks and canonical host
evidence remain. Both owned VMs exit zero and generated authentication is removed;
other guests/disks remain untouched.

Portable foundation originally records 168 passes and 11 native-capability skips;
42 later affected cases pass, including the additional gate case and the two
profile-fixture cases. The composed foundation covers 180 registrations
(169 pass / 11 skips), plus those two profile cases. Repository/diff checks pass.
No full matrix repeats for PR creation.

## Remaining acceptance

These are OpenBSD kernels and native binaries under QEMU HVF on ARM64 and TCG
on AMD64, hosted on macOS. They do not establish native AMD64 CPU, physical GPU/
input/hotplug, toolkit/compositor, native Linux sanitizer, standards/theme or
wider release acceptance. Keep those owners open. Linux/FreeBSD retain their
unchanged source-scoped runtime evidence; no WM matrix repeats there for the
release selection and test-dispatch metadata change. The old 7.9 VESA cause
investigation (#93) stays historical/deferred; it is not a reproduced 8.0 gap.

Performance reference PR132 remains at 2/5 merged implementation checkpoints
(PR133/134). This PR is unmerged and no regular comparison is due.
