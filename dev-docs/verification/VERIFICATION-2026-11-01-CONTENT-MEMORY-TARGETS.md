# Protected content memory — assembled comparison

Owner Cam; STORE-01 #74 with CONTENT-01 #63 and RETENTION-01 #71. Base PR334
`00e8c5799ca37e2f061f073263358f548f825375`. Six target profiles pass the
same private comparison: **150 native invocations, 4,320 normal lifecycle
rows and 48 observed access/lock controls**. All normal preparations succeed;
no control is unavailable in these selected profiles. This completes the
protected-memory candidate comparison, not the broader issues or R1.

[Exact results and source/artifact/command identities](VERIFICATION-2026-11-01-CONTENT-MEMORY-TARGETS.json)
and the [contract](../decisions/CONTENT-MEMORY-EVALUATION-2026-11-01.md)
bind these observations. Retain the base-system allocator direction; defer
libsodium guarded-allocation integration. See the
[bounded decision](../decisions/CONTENT-ALLOCATOR-COMPARISON-2026-11-01.md).

## Observed profiles

| Profile | Native release | Compiler | Execution | Captures / controls |
| --- | --- | --- | --- | --- |
| linux-arm64 | Linux 6.18.10-200.fc43.aarch64 | cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 | Ubuntu 24.04 container, AppleHV Linux VM | 25 / 8 |
| freebsd-arm64 | FreeBSD 15.1-RELEASE-p3 | FreeBSD clang version 19.1.7 (https://github.com/llvm/llvm-project.git llvmorg-19.1.7-0-gcd708029e0b2) | QEMU HVF | 25 / 8 |
| openbsd-arm64 | OpenBSD 8.0 | OpenBSD clang version 22.1.6 | QEMU HVF | 25 / 8 |
| linux-amd64 | Linux 6.12.111+deb13-cloud-amd64 | cc (Debian 14.2.0-19) 14.2.0 | Debian 13.7, QEMU TCG; not Ubuntu qualification | 25 / 8 |
| freebsd-amd64 | FreeBSD 15.1-RELEASE-p3 | FreeBSD clang version 19.1.7 (https://github.com/llvm/llvm-project.git llvmorg-19.1.7-0-gcd708029e0b2) | QEMU TCG | 25 / 8 |
| openbsd-amd64 | OpenBSD 8.0 | OpenBSD clang version 22.1.6 | QEMU TCG | 25 / 8 |

These are native target binaries inside the named guests. AMD64 uses CPU
emulation; neither guest execution nor AppleHV establishes physical hardware
or native Linux hardware sanitizer qualification. The Linux ARM userspace is
Ubuntu 24.04 on the existing Fedora AppleHV Podman machine; OS/kernel/package
identities remain in its environment receipt. Fixed offline asset/boot pins
bind fresh stock BSD images. The unchanged BSD runner uses disposable job
authentication and retires its host key; no persistent/shared trust changes.

## Comparable work and controls

Each candidate runs three alternating-order processes with 20 repetitions of
zero, one byte, page−1, page, page+1 and 64 KiB: 60 samples per size/candidate.
Each buffer reserves exact policy costs, prepares protection, writes a bounded
contiguous prefix while reserved, closes access, publishes actual lengths,
reads through an exact lease, retires and observes wiping before release.
Cancellation refuses later production/publication/reader access. Five explicit
allocation/lock/dump/access/cancellation injections check cleanup flow;
injected failures are not real OS exhaustion or enforcement evidence.

Both candidates separately complete a one-page positive read/write control,
produce actual native SIGSEGV in no-access and read-only write controls after
checked readiness, and genuinely refuse a checked lock call after lowering
only their own verified lock limits to zero. Positive/negative OS, candidate,
page, nonroot UID/EUID, binary and environment match. Linux verifies absence
of CAP_IPC_LOCK. Neither probe changes identities/capabilities. Exit 139 alone
cannot pass. All child waits confirm; no timeout/cancel is accepted.

Each non-forking child has a five-second deadline, one-second TERM grace and
five-second bounded reap. The separate runtime/launch/collection bound is 30 s;
the source-build/runtime job is bounded to 1,200 s. An unconfirmed wait fails
and stops the owned lab. Existing configured sources and builds are reused;
no full WM, install/default-X, theme or unrelated runtime matrix ran.

## Timing and footprint

The following medians use 60 matched one-page rows per candidate. Prepare
includes allocation/protection; finish includes policy, copies, reader,
retirement, wipe and release. They are separate phase medians, not end-to-end
WM latency or a sum-of-medians benchmark. Full six-size variability, main-entry
initialization, per-process CPU/raw RSS and supervised wall remain in JSON.

| Profile | Base prepare / finish µs | Sodium prepare / finish µs | Base / sodium executable bytes | Private staged SDK bytes |
| --- | --- | --- | --- | --- |
| linux-arm64 | 2.542 / 6.750 | 5.188 / 8.021 | 136,256 / 990,136 | 4,385,299 |
| freebsd-arm64 | 1.166 / 4.000 | 2.750 / 7.625 | 77,864 / 1,203,192 | 3,542,859 |
| openbsd-arm64 | 4.416 / 9.875 | 10.291 / 16.084 | 75,432 / 1,275,592 | 4,001,157 |
| linux-amd64 | 52.999 / 78.999 | 117.999 / 114.999 | 91,688 / 1,953,616 | 6,631,817 |
| freebsd-amd64 | 26.000 / 57.000 | 92.000 / 133.000 | 73,760 / 1,461,616 | 3,951,201 |
| openbsd-amd64 | 101.505 / 206.024 | 182.655 / 230.145 | 67,224 / 1,351,728 | 4,054,253 |

Binaries use the same strict C11/O2/debug profile; they are unstripped
comparison executables, not installed-product deltas. Sodium is explicitly
statically linked to the attested archive; native loader dependencies and
private link flags are recorded per artifact. The complete private SDK includes
headers, archive and metadata. Source/build/stage logical bytes, regular-file
counts, allocated KiB and native build-phase wall stamps are separate records.
No CPU or allocator/RSS cost is inferred from file sizes or page models.

Official libsodium 1.0.22 source: 2,008,529 archive bytes and 9,072,098 regular-file
bytes after expansion; ISC license. Archive SHA256
`adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349`.
HTTPS release acquisition and the independently recorded Homebrew source hash
bind it; no local upstream signature verification is claimed. Stable configure/
make uses existing base tools; no Autoconf regeneration, package install, cgo,
FFI or GUI dependency is introduced. Configured Makefile/DEFS, actual utility
compiler/preprocessor argv, unchanged object hashes and archive-member equality
bind the allocator. Prototype prefix/DESTDIR is private, never a system install.

## Protection and evidence limits

Linux children verify zero core limits and process nondumpability; BSD verifies
zero limits under its pinned stock-kernel/core-policy prerequisites. These are
process controls, not observed regional dump exclusion or universal collector
absence. Linux/FreeBSD use checked regional advice. OpenBSD baseline applies
its own MAP_CONCEAL mapping; sodium region-kind3 means its exact configured
utility mmap path passes MAP_CONCEAL. Both OpenBSD architectures attest this
from effective native definitions, object/archive and static link. No crash
dumps are collected and no system-wide core policy changes.

Page-rounded mapped/locked totals are source-derived reservation models, not
kernel mapping/locked-memory measurements. Normal optimized runs inspect zeros
while memory remains valid. Expected-fault children do not explicitly wipe or
acknowledge cleanup. Sodium void free exposes no individual unmap/unlock result;
its observation is wipe-and-return. Base cleanup checks those returns. Neither
proves deletion of prior stack/register/transport/decoder copies.

Initialization starts at main entry and includes common setup; loader startup
is not isolated. Supervised wall includes fork/exec and 1 ms polling overhead.
RSS is retained in native raw units, with no cross-OS normalization. Zero
readings in some FreeBSD ARM samples do not establish zero memory use. These
short-lived probes have no persistent idle service: product idle/wakeup and
clipboard end-to-end performance remain unmeasured. No runtime production
code was removed, no installed allocator was adopted and no general performance
cadence counter advances for this private comparison.

## Review, cleanup and continuation

Separate source/recipe/dispatch reviews fixed pre-execution configuration,
unavailable-result, import-guard, byte-count and owned-cleanup issues. Four
strict host compile/link commands, 14 pure fabricated-receipt oracle checks
and a no-child clock control are supporting evidence, not OS enforcement.
The six captured runs above supply the actual target observations. Prior
PR334 host unavailable evidence remains unchanged. All current native cases
pass; the late read-only Linux AMD status inspection returned SSH 255 while
its separately recorded attempt finished and shut down; it is not a test pass.

Keep current compact captures, static SDKs and canonical caches; retire only
verified stopped owned VM clones/duplicate inputs. The Linux warm base is
unchanged. Cleanup receipts distinguish direct QEMU reaping from guest shutdown.
No disk expansion. Source/object/config/archives verify before/after execution.

Next: implement the bounded native payload owner using the qualified base
facilities and shared content policy. Installed capacities and lock-time
capture/access remain explicit owner policies. Protected transport/UI/decoder
copies, actual capture/retention, Go crypto comparison, optional authenticated
persistence/key/recovery, screenshots and release-wide acceptance remain open.
No new UI; no screenshots or theme checks are applicable to this checkpoint.
Parked #257/#258 remain untouched.

Verified retirement reclaimed 16,845,316,096 allocated bytes from stopped BSD
clones/duplicate inputs and 21,823,488 from the disposable Linux COW child.
Preserved host exports contain the complete private static SDKs.
