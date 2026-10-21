# Status and diagnostic stream ownership

2026-10-21 — FD-01 / PROC-01 / CLEAN-01, with DATA-01 metadata and
LOOP-01 continuation coverage. Composed local checkpoint passes; PR #141
remains for maintainer review. Broad audit owners and release acceptance stay
open. [Machine receipt](VERIFICATION-2026-10-21-STREAM-OWNERSHIP.json).

## Four repairs and their contracts

1. **Transactional Status construction and teardown.** `status_pipe.c` owns
   a newly created mode-0600 FIFO, its identity, private close-on-exec descriptor
   and output continuation. The existing `$TMPDIR/fvwm3.pipe` name, with `/tmp`
   fallback, remains. Existing paths, including stale FIFOs, are refused rather
   than unlinked. Open/type/identity/flag failures roll back the owned descriptor
   and matching FIFO. Off, normal shutdown and restart retire the owner and
   matching published environment value. Removing the former `fdopen` step also
   removes its unclosed-descriptor failure path.
2. **Bounded Status output with readiness continuation.** Preserve partial JSON
   lines across EAGAIN/EINTR, cap one pending record at 1 MiB including newline,
   and make at most four writes per flush. The existing readiness snapshot
   watches POLLOUT and resumes output without another command. One dirty bit
   coalesces subsequent snapshots; a completed pending line precedes the latest
   snapshot. No window pointers survive in the transport owner. This bounds
   queued output, not the temporary JSON serializer's total allocation.
3. **Transactional, repeatable log adoption.** One FILE owns the inherited
   descriptor; adopting it again is a no-op. Failed adoption or path replacement
   preserves the working sink. Switching to stderr closes the prior owned file;
   stderr itself stays borrowed. Buttons, Ident, Backer and IconMan check adoption
   failures and use the existing checked decimal parser for LoggingFD metadata,
   including empty and overflowing values. Deliberate module inheritance remains.
4. **Diagnostic failure containment.** Empty formatted messages avoid an access
   before the buffer. Returned write/flush failures retire the diagnostic sink
   instead of exiting the WM/module; explicit reopening recovers logging.
   Allocation-failure behavior, SIGPIPE policy and potentially blocking log-file
   I/O are unchanged. This is not an asynchronous logging redesign.

Supported configuration names/arguments and JSON version 4 remain usable.
The man page documents collision refusal, permissions, output coalescing/cap,
retry/cleanup and log failure/recovery. No configuration migration, new package
or portable module wire format is introduced.

## Qualification and composition

Frozen signed implementation `e32061f5139264ab6ee20af6c2728ace6c3e7d55`, based
on merged PR140 `40b5bfeac2bb831bdef0d385bc3de860450cefc1`, has 2,107 regular
source inputs. Its archive SHA-256 is
`5eea34c4e178f8a769d0cd7f2b6f2ec8187720b6313833a2dc779bd4752b52fc`.
The full standard matrix ran once; later work continued only affected checks.

| Target | Standard profiles and accepted entries | Completion |
| --- | --- | --- |
| Linux ARM64 | GCC 286; Clang 286 | Original full runs, fresh installs and staged X checks pass |
| Linux AMD64 | GCC 286; Clang 286 composed | Clang retains 285 original passes plus the corrected fault probe; withheld install/X checks then pass |
| FreeBSD 15.1 ARM64 | Default 273 | Full suite, install, staged X and authenticated default Xorg pass |
| FreeBSD 15.1 AMD64 | Default 273 | Full suite, install, staged X and authenticated default Xorg pass |
| OpenBSD 8.0 ARM64 | Default 273 | Original full suite/install retained; corrected audit and staged/default Xenocara continuation pass |
| OpenBSD 8.0 AMD64 | Default 273 | Original full suite/install retained; corrected audit and staged/default Xenocara continuation pass |

Totals: **2,236 standard entries** (2,235 original passes plus one corrected
probe), 58 affected optional entries (software 39 / minimal 19), four scoped
ASan/UBSan entries with leak detection enabled, ten install stages, 150 staged
ELF identities, 77 catalogs and 700 data-file identities. The installed profiles
pass ten Status, twenty 1x/1.5x geometry and ten style fixture invocations.
Four private-authority default BSD servers pass five fixtures each: Status,
actual-core readiness, real session and geometry at both scales. Unauthenticated
clients are refused and all four servers exit cleanly. OpenBSD post captures
also verify two built and two staged PNG consumers per architecture.

New units exercise real 1 MiB pipe backpressure, exact partial-write bytes,
collision/symlink refusal, construction rollback, descriptor flags, exec
noninheritance, replacement preservation, four-interruption service bounds,
EIO/zero writes, repeated log adoption, failed fdopen/replacement, intentional
log inheritance, borrowed stderr, empty messages and error/reopen recovery.
The live fixture fills the pipe with a 77,056-byte JSON snapshot, verifies
continuation/latest-state recovery without more commands, and checks refusal,
Off/On and Quit cleanup. Linux `/dev/full` also verifies actual WM survival.
Buttons' packet-only fixture includes malformed LoggingFD metadata. Focused
development checks passed four Linux and three FreeBSD ARM64 entries before
the matrix. Preserved PR138 runtime binaries reproduce existing-path deletion
and failed-log startup exit in two expected-negative controls; PR139/140 do not
change their runtime C/header inputs.

`make verify` completes all seven portable groups: 188 entries, 177 passes and
eleven capability skips. Its overall success is derived from the complete log;
the initial shell wrapper did not separately preserve make's raw exit receipt.
Repository/link and whitespace checks are run again for final documentation,
without repeating portable tests or the matrix solely for the PR.

## Retained failures and affected corrections

- Linux AMD64 Clang's sole original failure was the status fault-injection
  driver intercepting `open64` while fortification calls `__open64_2`. Add the
  matching two fortified entry-point forwarders and linker wraps, following the
  existing Exec probe. Production fortification stays enabled. Compile/link the
  corrected driver against the exact original helper/library, retain their
  hashes and flags, and run the probe on ten scoped profiles including ASan and
  OpenBSD ARM64. OpenBSD AMD64 retains its passing original unit: the added
  fortified hooks are unused there and the assertion body is unchanged.
  Linux AMD64 Clang's unchanged 802 ELF outputs and source identities permit
  only its withheld install/status/geometry/styles continuation.
- OpenBSD's original 273-entry suites and native installs both pass. The
  one-off installed-identity auditor used Meson's private `fix_rpath` API
  incorrectly: Meson 1.12.1 requires `target.system`. The
  [official caller](https://github.com/mesonbuild/meson/blob/1.12.1/mesonbuild/minstall.py)
  and [implementation](https://github.com/mesonbuild/meson/blob/1.12.1/mesonbuild/scripts/depfixer.py)
  establish the corrected call. This changes verification tooling, not project
  runtime/build tooling. Restore the exact current candidate build/stage, check
  every archived payload/mode and compiler/PNG identity, and finish the audit/X
  remainder. All 829 restored ELF outputs per OpenBSD profile remain unchanged;
  the original full-suite raw hashes match. No application rebuild or full-suite
  repeat is needed. The ARM continuation compiles only its isolated probe.
- OpenBSD AMD64's primary collector omitted the root checkpoint directory.
  Preserve this limitation: the original raw native build/test/install capture
  and nine newly built production translation units validate, while the post
  capture records all 2,107 observed source hashes/modes before and after use of
  the exact restored binaries. The ARM primary includes its checkpoint manifest.
- Linux ARM64's runtime/install/optional/sanitizer checks pass before a manual
  assertion incorrectly searches the main page for Status text. Status is in
  `fvwm3commands.1` and the combined page. Retain that failure and correct only
  the manual continuation. Earlier focused fixture/dispatcher mistakes and
  manual assertion/manifest checks remain in canonical logs; they are not
  production failures or reasons to rerun the matrix.

After freezing, seven production files receive only line wrapping/comments.
Nonwhitespace, noncomment tokens and explicit assertion/file/line anchors are
identical. Fourteen Linux ARM64 GCC/Clang object pairs preserve allocated
code/data and normalized relocations; original source/build outputs are not
mutated. This establishes the final formatting-only composition. The other
post-freeze changes are the test hook, manual paragraph and evidence/status
documentation. The machine receipt identifies final production hashes and the
exact corrected probe binaries/commands.

## Manuals, evidence retention and remaining work

All 26 manifest man pages render and match their staged bytes. Only the main
and combined pages change from the frozen export when the logging paragraph is
added; the other 24 match. Native FreeBSD mandoc renders the three affected
pages successfully. Lint remains nonclean: 69, 101 and 222 messages respectively,
the exact same messages/counts as the prebatch baseline, with lint exit 3 and
no added messages. Do not report this as a lint pass; wider formatter quality
stays with DOCS-01. The nested export manifest omitted from its producer's
per-file manifest receives an explicit host-side identity/content check.

Canonical evidence is under the owner-controlled
`builddir/codex-2026-10-21/stream-ownership-safety/`; the machine receipt records
archive paths, full SHA-256 values, payload/source checks, original errors and
composition. Gzip integrity, member/path guards and complete captured manifests
validate the normal checkpoint archives; AMD64 primary's stated collector gap
remains explicit. After validation, guarded exact-node/hash/mode retirement
removes completed duplicate guest captures/archives on the four persistent
Linux/FreeBSD guests. Canonical host evidence, source, reusable builds, stages,
performance references, VM disks and other owners' data remain. Four owned
OpenBSD jobs exit QEMU with status zero and their ephemeral keys are removed.

Disposition: the four repaired construction/output/adoption/failure contracts
are locally qualified. Numeric LoggingFD provenance in late configuration
broadcasts remains a **source-review concern without a live reproducer**:
the core advertises its current descriptor, whereas an already-forked module
inherited its earlier descriptor table. Track a bounded delayed-module/reopen
case under FD-01/PROC-01 at the selected input-boundary pickup; do not call it
a confirmed exploit or silently mark the entire stream review complete.
Then review the already-selected Module/Script boundaries and pending command
lifetimes. No open-ended rewrite or performance batch follows from this report.

PR138 remains the performance reference at 1/5 subsequent merged implementation
checkpoints after PR139; this unmerged PR does not increment it. No comparison
is due. Guest-native kernels/binaries and scoped sanitizer passes do not replace
native-CPU sanitizer, physical GPU/input/hotplug, toolkit/compositor or final
release acceptance. OpenBSD follows future releases; this evidence applies to
8.0, with 7.9 out of scope. This is a checkpoint, not a security certification.
