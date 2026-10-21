# Shared string copy ownership

Status: composed local checkpoint passes; ready for PR review. Owners: DATA-01
#6 / CLEAN-01 #34. Base: merged PR136, including the protocol inventory from PR137.

## Changes and contract

Three related shared-library operations form this checkpoint:

- `CopyString` uses `size_t`, a first-line length and checked tail indices.
  Empty and newline-only inputs no longer form a pointer before their source.
  It copies its owned prefix and writes the terminator explicitly.
- `stripcpy` trims the whole string with checked indices, including newlines,
  without narrowing its length to `int`. Empty results still own an allocation;
  NULL still returns NULL.
- `CopyStringWithQuotes` reuses the private first-line helper's returned length,
  removing a separate full `strlen` scan and integer narrowing. Only initial
  ASCII spaces admit the opening quote. A preceding tab retains quotes, the
  first newline terminates the copy, and whitespace inside a closing quote stays.

The public signatures and existing configuration bytes remain unchanged. Other
quote helpers, parser ordering, X/error barriers, module protocols, mandatory
dependencies and build defaults are unchanged. This batch needs no configuration
migration or new man-page option. The before-source pointer constructions are
source findings, not reproduced exploits or sanitizer findings.

The independent streaming oracle tracks the last significant output byte rather
than using production's backward trim. Each locale executes 9,602 inputs through
three operations (28,806 output comparisons), plus NULL and ownership controls.
It includes all non-NUL byte values, an 8,191-byte input, literal quote/space/tab/
newline differences and all length-zero-through-five strings over six bytes.
Observed locales are recorded in raw logs; this is not universal locale coverage.

## Work and size observations

An unchanged baseline and the repaired Linux ARM64 driver agree on output. With
compiler `strlen` builtins disabled and actual calls wrapped, the quoted branch
changes from one owned-string `strlen` call to zero. This demonstrates removal of
one rescan; it does not measure libc internals or establish desktop latency.
`libs/Strings.c` loses 28 physical lines; tests add coverage. No whole-repository
or executable-size reduction is claimed. Consumer matching-line inventories have
61 `CopyString`, ten quoted-copy and eighteen `stripcpy` sites; those are source
matches, not a complete call graph.

The PR132 performance reference remains at four merged implementation checkpoints
(PR133–136). This unmerged checkpoint does not increment that count. Its merge
will reach five and make the next paired comparison due. No routine latency/CPU/
RSS/executable benchmark is repeated here.

## Source and continuation identity

Runtime commit: `99db08fd64f0bd8bbeec32b6fef7a3d1ce2825c7`.
Tree: `69d56efef53f2619effbe34fe87d8ea70779c167`.
Frozen source archive SHA-256:
`9c2b8ca91524b5d7832f75051adbe19d6abce34695cea15891c8741bb4f53bc8`.
All 2,095 regular input bytes and modes bind to that Git tree. Five tracked
inputs, including the tracker, change from the merged base. Final guide/evidence upkeep is separate.

The final test-driver edit only wraps two long expressions and adjusts adjacent
blank lines. Its non-whitespace tokens and explicit assertion line numbers are
identical. A separate Linux ARM64 driver passes and preserves all 22 allocated
code/data sections. All production C/header inputs remain byte-exact to the
frozen runtime. No tested source tree or build was synchronized for this edit.

Existing Linux/FreeBSD incremental builds are reused. Their affected continuations
retain completed full results after complete source and no-op/output hash/time
checks. Copies of the same original test log are counted once.

OpenBSD uses fixed, verified 8.0 images on both architectures. A qualified private
PNG 1.6.59 prefix is reused after source-pin, all twelve installed-file hashes,
compiler identity and retained 36/36 upstream-test checks, with a fresh native
probe and actual consumer-loader validation. Package signatures remain enabled.
Full native WM tests run once per architecture. The original source archive is
bound by the QEMU stream and run recipe; AMD64 also retains its complete observed
source map. ARM64's original small checkpoint was outside the collector's scope;
its original full Meson results remain in the canonical build archive. Do not
claim a missing original ARM64 observed-source file was collected.

Post-test continuations restore the exact complete build/stage archive into a
fresh prepared 8.0 guest, verify every regular file's hash/size/mode and the
compiler, and verify the fresh 2,095-source map before and after checks. Host
comparison also proves every original ELF output and staged regular file remains
identical after post-checks. Only missing verification/display work is executed;
there is no repeat of the 270-entry native suites. ARM64's final PNG/display
remainder runs in its held post-check guest. Canonical original failures and
corrected payloads remain distinct.

## Qualification

| Target/profile | Standard | Affected optional |
| --- | ---: | ---: |
| fvwm-x-linux-arm64-svg | gcc: 282, clang: 282 | software: 18, minimal: 10 |
| linux-amd64 | gcc: 282, clang: 282 | — |
| freebsd-arm64 | default: 270 | — |
| freebsd-amd64 | default: 270 | — |
| openbsd-arm64 | default: 270 | — |
| openbsd-amd64 | default: 270 | — |

Standard totals are 2,208/2,208 across eight compiler profiles and six target
combinations. Optional software/minimal entries add 28/28 affected results; these
are not fresh full optional suites. The three operations have no feature-specific
policy, so those profiles select string/parser/module consumers before dispatch.
Go is disabled in these profiles; its independent qualification is not repeated.
The Linux ARM64 scoped ASan/UBSan driver passes with leak detection enabled. This
is guest evidence, not the separate native-CPU sanitizer release qualification.

Ten newly staged installs verify 150 ELF target identities, 77 catalogs where
NLS is available, and 700 data-file identities. RPATH-adjusted expected outputs
match the staged bytes. Each profile passes staged geometry at 1x and 1.5x and
real wildcard-style clients: twenty geometry and ten style invocations. The four
BSD targets also pass one authenticated default-X server each, cookie-less
connection refusal, seventeen fixtures per server and clean teardown. Those
68 invocations cover eight actual-core style/window-tree/state/update owners,
real styles/session, five image/resource consumers and staged geometry.

Portable verification runs once: 180 registered, 169 pass, eleven native-tool/
capability skips. Final documentation/formatting uses repository/link/whitespace
checks without repeating that suite or native qualification. Local review checks
NULL/empty ownership, byte classification, distinct newline policies, quote
admission and bounded indices against the original implementations and consumers.

## Original failures and harness corrections

No WM/native test failure is hidden by these continuations:

1. Initial host stub configuration omitted standard header declarations; the first
   standalone Linux command omitted the generated configuration's source include
   path. Corrected focused commands pass without product changes.
2. Two initial OpenBSD dispatches refuse scripts above the existing 64 KiB bound
   before job creation or boot. Bounded recipes keep the source archive binding;
   subsequent expected hashes are supplied as a separately pinned manifest.
3. The first FreeBSD ARM64 install verifier counts catalogs as executable targets.
   The corrected verifier distinguishes fifteen ELF files from eleven catalogs.
   Linux GCC result-name verification initially assumes colon-only Meson names;
   slash-style names are normalized. Their full tests already passed.
4. OpenBSD's private extraction umask yields source modes 600/700. The gate
   restores only the expected 644/755 modes before tests. Both PNG upstream suites
   had already passed; their verified installed prefixes are reused.
5. A PNG reuse diagnostic inherits `LD_BIND_NOW` into `ldd`, causing a diagnostic
   abort before WM compilation/tests. Immediate binding remains on the native
   probe; normal loader diagnostics follow the established bootstrap helper.
6. Both OpenBSD native suites pass, then install verification encounters Meson
   1.12's required `system` argument. The post-check supplies `openbsd`, retaining
   all RPATH/content assertions and the successful original install.
7. Two ARM64 build-restoration checks stop before post-tests while verifying BSD
   tar mode fields: extraction umask first, then full file-type bits in the archive
   mode. Exact permission bits are restored and checked after masking type bits;
   local regular-file/ELF controls verify this correction. A separate local control
   first names the wrong ELF path and is corrected to the actual archived target.
8. The first post gate receives the compact path/mode manifest without hashes;
   it refuses before verification. The corrected gate uses the complete separately
   SHA-pinned source manifest. An oversized generated manifest recipe is caught
   locally before dispatch and likewise moved to the bounded upload path.
9. ARM64's full consumer-loader verifier refuses to overwrite the original
   build-only loader logs. Those logs and consumer receipt are retained under
   `png-built-retained`; fresh logs validate both built and staged consumers.
   The remaining probes run in the same held guest. AMD64 uses that correction
   from its first post-test continuation.

Source/build identities and original errors remain recorded. No deadline,
assertion, supported release, dependency patch requirement or package-signature
policy is relaxed. No production code changes result from the harness repairs.
The avoidable setup/verification retries are overhead, not performance regressions
or additional native acceptance runs.

## Evidence, cleanup and limits

The [machine receipt](VERIFICATION-2026-10-21-STRING-COPIES.json) records canonical
archive paths and SHA-256, exact scope/counts, preserved native results, install/
default-X identities, formatting proof and guarded retirement receipts. The host
workspace evidence root is
`builddir/codex-2026-10-21/string-copy-ownership/`.
Full gzip integrity, bounded archive names/types and complete captured-payload
hashes are checked before accepting results. Linux/FreeBSD checkpoint source
hashes/modes compare with all 2,095 frozen inputs. OpenBSD restoration and final
source/output checks retain the distinctions described above.

Following the user's storage direction, completed Linux/FreeBSD guest capture
folders and duplicate archives are removed only after exact file/hash/mode/size
and path guards, freeing 14,962,760 duplicate bytes. Canonical host evidence,
source, incremental builds, install/performance stages, VM disks and other owners
remain. Ephemeral OpenBSD guests shut down and their job keys are removed; their
VM disks remain preserved. No VNC or monitoring job is left running.

This checkpoint covers VM/native-OS X11 and scoped pure ownership. It does not
close DATA-01/CLEAN-01's wider audits, physical GPU/monitor behavior, toolkit/
compositor/assistive-provider checks, native-CPU sanitizers or release acceptance.
OpenBSD follows new releases; 8.0 is this checkpoint's current tested baseline,
7.9 remains out of scope, and the historical VESA cause is deferred.
