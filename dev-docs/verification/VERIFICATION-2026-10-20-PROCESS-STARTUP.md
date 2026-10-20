# Process and session startup transactions

Status: composed local checkpoint passes; ready for PR review. Owners: FD-01 / PROC-01 / DATA-01; main base is merged PR132.

Three related runtime improvements:

- Core Exec checks fork, opens and duplicates /dev/null before launching the
  shell, and uses _exit on child setup/exec failure. It uses fork's owned copy
  of the command instead of allocating a duplicate string.
- Session authentication obtains every network string and cookie before writing
  or publishing credentials; failed construction cleans both temporary files
  and all prepared entries. File/helper failure also precedes publication.
- Session startup checks composed network identifiers and setenv, publishes
  only after watch admission, and removes watches/resources after failure.
  Selectable listener count bounds precede authentication-size arithmetic.

No command names, arguments, configured shell text, module packet layouts,
logging inheritance, X ordering barriers or dependencies change. Pure controls
wrap real production APIs and actual core objects. Existing real-X session and
compatibility drivers cover successful use and descriptor inheritance.

The initial Linux probe compile failed on a missing type header; corrected
fixture evidence is retained separately. Runtime sources were unchanged by the
fixture correction. Completed combined qualification and scoped sanitizers are
recorded below. PR132's accepted performance reference is at 0/5 subsequent merged
implementation checkpoints; this constructor batch adds no benchmark run.

## Baseline and focused observations

Six isolated unchanged-main controls use original builtins/fsm implementations,
compiled with the owning native flags and linked with the new probes. Core Exec
continues to exec after injected open refusal (assertion). Session network-name
refusal reaches incomplete data (assertion), cookie refusal faults with SIGSEGV,
network-list refusal and environment-publication refusal incorrectly report
success (assertions), and iceauth refusal follows prior credential publication
(assertion). These are local constructor/failure observations, not a demonstrated
remote compromise. Canonical baseline logs are retained separately.

The candidate passes 12 focused entries on Linux ARM64 GCC and 12 on FreeBSD
ARM64: core Exec's seven observations, seven separate session constructor cases
with successful retry after each refusal, and compatibility/session/Buttons/
Script real-X integration. Eight binary identities per target bind retained
results. The initial missing-header compile is preserved, not counted as a pass.

Linux guest capacity initially failed the unchanged 1-GiB owner guard. Four
completed duplicate checkpoint captures were retired only after full canonical
host gzip/CRC and exact native payload/hash comparison (941665141 duplicate
bytes). Canonical host archives, source/build/stage trees and VM disks remain.

The initial environment baseline wrapped only setenv, so it did not inject
failure into the old putenv call. That observation is excluded and replaced by
one isolated control that wraps putenv, records the actual refusal, and verifies
that unchanged main ignores it. The six accepted controls are the other five
original observations plus this corrected refusal; all attempts are retained.
Candidate sources and matrix inputs were unchanged.

## Corrected compiler-specific fixture

The original Linux AMD64 Clang Exec probe failed because it wrapped open/open64
but the production object calls fortified __open64_2. Symbol/disassembly evidence
shows the production negative-fd branch is present. The corrected fixture wraps
both fortified open entry points, with hardening flags retained. Its seven
observations pass on all eleven compiler/sanitizer profiles; the early corrected
Clang control is retained by unchanged source and production-binary identities.
The other 273 original Clang entries remain valid. Clang's withheld fresh install
and four staged style/session/geometry checks then pass with no production
rebuild or matrix rerun. The original failed row, capture and diagnostics remain.
This is a composed qualification, not an originally all-green full run.

## Combined acceptance

Three process/session constructor repairs pass one composed checkpoint after a single full matrix: 2141 composed standard results (2116 original fresh passes, 24 retained after unchanged-input/eight-binary checks per focused target, and one corrected Exec probe), plus 468 optional results across six targets/ten profiles. Eleven actual-core Exec drivers verify 77 observations; seventy session fault/retry entries and nineteen real session integrations pass. Nine scoped guest ASan/UBSan checks, nine fresh stages and four authenticated default BSD displays pass. Three inherited required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. Performance reference PR132 is accepted, at 0/5 subsequent merged implementation checkpoints; no comparison is due.

Frozen signed runtime commit: `a91fe79b7a28a5373af8879442be7fdfb3fa6fce`; tree
`c3a881b6728a69c38c4d60f38a01d69b28710d48`; base `e64a64947d47b8ca025caa36fe83ca91d83c2138`.
The original 2077 regular inputs bind source bytes/modes to archive
`919f3824b64d9806f00d69d895ad4258a7c0a54da9622a30164607217cd9726c`. Final line wraps/comments and Exec-probe argument/fortified-hook corrections are
separately qualified: 33 production
translation units retain identical preprocessed tokens across eleven compiler/
sanitizer profiles. Seventy final session-probe entries and eleven final Exec
entries pass in isolation (81 affected entries total). All
other runtime/native-test/build inputs and all seven original gate runners remain
exact. Five final source paths are explicitly bound by the correction receipts.
Frozen native sources and production binaries remain unchanged; the full matrix
is retained. Install recovery adds only the withheld stage and its logs.

The result is 2609 passing standard/optional entries out
of 2612 registrations; the three inherited OpenBSD AMD64 omissions
are required gaps, not passes. They remain CI asset-worker timeout, SVG startup
refusal and PR120 legacy-property startup refusal/unchanged control. No retries
or VESA cause work are included. That profile receives no full install and no
two staged geometry invocations. Nine other fresh stages verify
135 code identities and 88 catalogs;
27 change and 196 match PR132.
Every native staged byte comparison and install RPATH identity is checked.

Four authenticated default BSD displays pass 66 commands,
including real authenticated session registration, literal iceauth pathnames,
no descriptor leaks, repeated cleanup and shutdown. Unauthenticated X clients
are refused; owned servers exit cleanly. Linux staged session use also passes.
Eighteen staged geometry invocations retain 126 cases.

Nine original guest Clang ASan/UBSan entries cover core Exec and all seven session
constructor controls with leak detection, plus real session integration with
leak detection disabled for external X/library scope. Eight affected final probe entries are also checked under the same guest
instrumentation. This emulated local execution does not establish native-CPU
sanitizer acceptance.

Portable verification runs once: 166 passes and
11 native-tool capability skips. All 26
man pages render once without diagnostics from 32 exact
inputs. The initial post-render text assertion selected fvwm3.1 instead of the
command manuals; existing fvwm3commands.1/fvwm3all.1 outputs were validated
without rerendering. 11 native compile logs retain inherited
warnings; none is on a changed C/header line. Exact counts/hashes are in the
machine receipt, and complete diagnostics remain in canonical captures.

Production C/headers add 84 lines and remove 67 (net 17).
Exec removes one duplicate-string allocation per launch by relying on fork's
owned copy. This source observation is not a measured desktop speedup or
executable-size claim. The session checks add explicit failure/cleanup paths.
No periodic benchmark is due: accepted reference PR132, zero subsequent merged
implementation checkpoints; the next regular comparison follows five merges.

[Machine-readable combined receipt](VERIFICATION-2026-10-20-PROCESS-STARTUP-COMBINED.json). Canonical complete capture
archives are retained under this checkpoint's ignored `qualification/` directory,
with source maps, executed runners, commands, failures, stdout, binary identities,
install comparisons and teardown evidence. Six gzip streams pass full EOF/CRC
and every capture payload hash. Original baseline/focused/manual/portable and
capacity evidence remains in the owning work directory. Two completed duplicate captures (165502106 and 163957066 bytes) were retired
before isolated final checks, after canonical host CRC and every native payload hash matched. This
retains the capacity floor and all canonical archives, source/build/stage trees
and VM disks.

Local source/caller/ownership review passes. The shipped manager constructors
run in isolated Buttons/Script modules before accepting their first ICE
connection; the separate core owns its own SM connection. Watch-removal rollback
uses that startup-order invariant, and this checkpoint does not extend manager
construction around preexisting ICE connections. See the
[X.Org ICE watch contract](https://xorg.freedesktop.org/archive/X11R7.7/doc/libICE/ICElib.html).
No external reviewer was used.
Native CPU, physical hardware, toolkits/compositors and release acceptance remain
separate. Only the affected probe entries are repeated for final line wrapping and the
probe-only argument/fortified-entry corrections; no full
matrix, default-X or performance run is repeated for this correction or
publication.
