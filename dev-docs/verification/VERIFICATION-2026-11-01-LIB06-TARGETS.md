# LIB-06 supported-profile comparison

Owner Cam; #48, over merged PR331 (`dc88d52`). **Retain patched cJSON for
R1; defer yyjson integration.** The finite comparison is complete; no production
serializer, parser, dependency or installed byte changes. See the
[decision](../decisions/STATUS-SERIALIZER-COMPARISON-2026-11-01.md) and
[exact receipts](VERIFICATION-2026-11-01-LIB06-TARGETS.json).

## Scope and outcome

One assembled pure-C comparison covers Linux, FreeBSD and OpenBSD on AMD64 and
ARM64. Each profile compiled four objects, two serializer probes and one private
supervisor, then exported 307 chronologically ordered native invocations. The
host independent oracle validated every invocation and hash without executing
exported ELF programs on macOS. Three native samples of 1,000 iterations per
backend/case measure construction, write and disposal, with alternating backend
order. No whole-WM/install/default-X/theme matrix was rerun.

All profiles retain the same outcomes: 33 mode/cases; baseline completes 11,
yyjson copied/borrowed each complete nine and refuse the two legacy key/value
cases under the frozen literal-Unicode flags. Semantic numeric parity differs
from lexical parity. All observed small-case allocation ordinals and 1/64-byte
budgets leave zero tracked live bytes. Output caps include newline and remain
strict; occupied-queue admission is a model, not actual transport qualification.
Candidate refusals are observations, not parity passes.

The actual unchanged MFL binary passes ordinary/Unicode/legacy byte parity
against both independent expected bytes and the baseline probe, 256 separate
256-byte-name events, and unterminated-name refusal followed by a valid sentinel.
Subscription readiness is observed on the same client's forwarded module command
pipe. The private socket is retired and the module is reaped with exit 0. This
is isolated production-module output with synthetic native packets, without
core/X; its separate records do not pretend to be the probe's stress array.

Retained October 21 Linux AMD64 GCC `status-json` unit and actual status-stream
receipts remain applicable to eight exact unchanged builder/policy/test files.
The actual stream parses ten records, up to 77,056 bytes, and checks complete
JSON after backpressure plus latest-state recovery. Their archive/member hashes
and source binding are retained. This reuses component evidence, not a fresh
full-core, current-install or release qualification.

## CPU and elapsed work

Values are median [minimum–maximum] nanoseconds **per iteration**, in ordinary /
64 KiB string / 256-record array order. They compare one target's compiler and
execution environment; do not rank OSes or extrapolate TCG to physical CPUs.

| Profile / execution | Baseline CPU | yyjson copied CPU | yyjson borrowed CPU |
| --- | --- | --- | --- |
| linux-amd64, 6.12.111+deb13-cloud-amd64, Debian 13, QEMU TCG | 5,709 [5,585–5,722] / 880,487 [846,817–957,674] / 2,521,136 [2,508,715–2,532,022] | 2,349 [2,336–2,508] / 367,414 [362,911–384,601] / 901,038 [897,513–907,351] | 1,995 [1,884–2,042] / 359,142 [349,546–369,177] / 591,382 [586,565–604,350] |
| linux-arm64, 6.18.10-200.fc43.aarch64, Ubuntu 24.04 container, Fedora 43 kernel, AppleHV | 311 [310–322] / 71,261 [68,155–76,727] / 137,159 [134,346–137,207] | 86 [85–91] / 11,843 [11,800–11,847] / 34,006 [33,717–34,040] | 76 [67–77] / 10,839 [10,833–11,021] / 24,128 [23,981–24,231] |
| freebsd-amd64, 15.1-RELEASE-p3, QEMU TCG | 4,971 [4,820–5,015] / 650,690 [650,290–664,870] / 1,594,074 [1,589,490–1,595,922] | 2,147 [2,094–2,526] / 323,824 [321,794–325,800] / 594,184 [590,242–596,707] | 1,711 [1,698–1,727] / 361,954 [361,389–363,363] / 519,844 [519,360–522,907] |
| freebsd-arm64, 15.1-RELEASE-p3, AppleHV | 223 [216–227] / 58,019 [55,618–58,948] / 101,252 [88,437–109,045] | 71 [68–72] / 14,079 [14,040–14,250] / 22,699 [20,435–27,273] | 61 [56–72] / 12,546 [12,175–14,989] / 20,772 [17,147–22,385] |
| openbsd-amd64, 8.0, QEMU TCG | 21,042 [20,349–21,044] / 616,013 [614,612–616,497] / 4,056,056 [3,974,281–4,075,882] | 9,852 [9,813–9,884] / 322,847 [319,114–405,090] / 1,355,335 [1,344,276–1,370,427] | 7,327 [7,299–7,567] / 297,969 [295,131–384,030] / 1,253,482 [1,224,467–1,257,863] |
| openbsd-arm64, 8.0, AppleHV | 4,338 [4,155–4,405] / 108,523 [106,053–111,728] / 321,465 [316,954–321,908] | 1,846 [1,755–1,850] / 34,724 [34,704–34,896] / 106,559 [106,343–107,032] | 1,119 [1,102–1,163] / 31,038 [30,383–31,058] / 95,749 [94,738–96,354] |

| Profile | Baseline elapsed | yyjson copied elapsed | yyjson borrowed elapsed |
| --- | --- | --- | --- |
| linux-amd64 | 5,710 [5,586–5,813] / 882,251 [847,464–958,243] / 2,521,991 [2,510,573–2,532,845] | 2,426 [2,338–2,511] / 367,992 [363,360–384,783] / 903,353 [899,943–907,594] | 2,037 [1,888–2,043] / 359,257 [349,912–369,935] / 592,526 [586,999–605,047] |
| linux-arm64 | 312 [312–326] / 71,443 [68,429–76,910] / 137,474 [134,814–137,474] | 86 [85–91] / 11,863 [11,823–11,881] / 34,100 [33,795–34,137] | 76 [67–77] / 10,860 [10,848–11,053] / 24,199 [24,066–24,330] |
| freebsd-amd64 | 4,974 [4,823–5,018] / 651,057 [650,818–676,993] / 1,595,289 [1,590,537–1,596,867] | 2,150 [2,096–2,529] / 323,949 [321,983–326,054] / 594,853 [590,492–598,652] | 1,713 [1,699–1,730] / 362,085 [361,590–363,563] / 520,140 [519,818–523,120] |
| freebsd-arm64 | 223 [216–228] / 58,017 [55,615–58,947] / 101,252 [88,436–109,058] | 71 [68–72] / 14,079 [14,040–14,250] / 22,698 [20,435–27,272] | 61 [56–72] / 12,546 [12,174–14,988] / 20,772 [17,147–22,385] |
| openbsd-amd64 | 21,042 [20,349–21,044] / 616,121 [615,041–616,531] / 4,057,458 [3,981,668–4,082,112] | 9,852 [9,814–9,884] / 322,849 [319,151–405,124] / 1,357,624 [1,344,651–1,370,531] | 7,329 [7,299–7,567] / 298,047 [295,161–384,380] / 1,256,456 [1,225,129–1,260,639] |
| openbsd-arm64 | 4,339 [4,155–4,405] / 108,553 [106,053–111,727] / 321,473 [316,969–321,919] | 1,846 [1,755–1,851] / 34,724 [34,704–34,896] / 106,563 [106,342–107,038] | 1,120 [1,102–1,163] / 31,038 [30,382–31,061] / 95,749 [94,738–96,354] |

Candidate synthetic loops are faster in this corpus. That does not establish
production response latency or a WM speed improvement. Full end-to-end candidate
MFL/core integration was not built; corresponding latency, installed bytes and
idle/loading deltas are unavailable, with deferral recorded rather than a pass.
Native process-wall samples include fork/exec/wait and requested 1 ms watchdog
polling plus scheduler delay; they are retained as observed, not isolated startup.
The short-lived probe has no persistent idle loop; idle wakeups are not applicable.

## Memory and binary costs

Requested tracked allocation peaks (bytes), ordinary / large / stress, are
identical across the six profiles:

| Mode | Requested peak bytes |
| --- | --- |
| Patched cJSON | 631 / 262,220 / 296,376 |
| yyjson copied | 952 / 459,277 / 361,704 |
| yyjson borrowed | 696 / 393,724 / 230,888 |

Tracked copy-realloc includes old/new overlap and excludes allocator headers,
rounding, stack and separately reported workspace. cJSON's custom hooks disable
its normal realloc fast path; use native trials for performance. Borrowed-mode
savings require caller inputs to outlive writing/disposal; the mutation controls
prove copied independence and borrowed observation, not real asynchronous-owner
suitability.

Whole-process maximum RSS is reported in KiB on these Linux/BSD profiles and
includes loader/input/probe overhead. Values are median [min–max], ordinary /
large / stress. FreeBSD reports some zeros for short-lived children; preserve
those OS observations without treating them as zero memory consumption or a
reliable comparison. Allocator-request counts remain separate.

| Profile | Baseline RSS | yyjson copied RSS | yyjson borrowed RSS |
| --- | --- | --- | --- |
| linux-amd64 | 1,460 [1,324–1,476] / 1,588 [1,472–1,604] / 1,588 [1,588–1,588] | 1,512 [1,404–1,564] / 1,604 [1,560–1,708] / 1,684 [1,656–1,748] | 1,404 [1,380–1,492] / 1,536 [1,528–1,556] / 1,608 [1,452–1,672] |
| linux-arm64 | 1,280 [1,280–1,280] / 1,280 [1,276–1,280] / 1,280 [1,280–1,280] | 1,284 [1,284–1,284] / 1,540 [1,536–1,540] / 1,540 [1,532–1,540] | 1,284 [1,284–1,284] / 1,412 [1,400–1,412] / 1,412 [1,404–1,412] |
| freebsd-amd64 | 2,628 [2,424–2,632] / 2,812 [2,808–2,948] / 3,092 [3,088–3,092] | 0 [0–2,668] / 3,064 [3,064–3,072] / 3,112 [3,104–3,116] | 2,676 [0–2,864] / 3,000 [2,996–3,000] / 3,004 [3,004–3,008] |
| freebsd-arm64 | 0 [0–0] / 2,700 [2,700–2,700] / 2,980 [2,980–2,984] | 0 [0–0] / 2,896 [2,896–2,896] / 2,952 [2,940–2,952] | 0 [0–0] / 2,828 [2,828–2,832] / 2,844 [2,840–2,852] |
| openbsd-amd64 | 1,472 [1,468–1,480] / 1,672 [1,656–1,680] / 2,844 [2,832–2,916] | 1,508 [1,492–1,508] / 1,684 [1,680–1,720] / 2,468 [2,420–2,492] | 1,492 [1,488–1,508] / 1,628 [1,612–1,632] / 2,332 [2,312–2,384] |
| openbsd-arm64 | 1,296 [1,292–1,300] / 1,456 [1,456–1,464] / 2,596 [2,580–2,672] | 1,296 [1,292–1,296] / 1,492 [1,480–1,492] / 2,152 [2,148–2,284] | 1,292 [1,288–1,292] / 1,420 [1,420–1,424] / 2,072 [2,064–2,132] |

| Profile | Baseline executable bytes | Candidate executable bytes | Compiler |
| --- | --- | --- | --- |
| linux-amd64 | 128,216 | 1,609,536 | cc (Debian 14.2.0-19) 14.2.0 |
| linux-arm64 | 174,904 | 1,882,592 | cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| freebsd-amd64 | 153,280 | 1,379,768 | FreeBSD clang version 19.1.7 (https://github.com/llvm/llvm-project.git llvmorg-19.1.7-0-gcd708029e0b2) |
| freebsd-arm64 | 146,208 | 1,226,176 | FreeBSD clang version 19.1.7 (https://github.com/llvm/llvm-project.git llvmorg-19.1.7-0-gcd708029e0b2) |
| openbsd-amd64 | 145,768 | 1,271,408 | OpenBSD clang version 22.1.6 |
| openbsd-arm64 | 151,440 | 1,265,952 | OpenBSD clang version 22.1.6 |

These `-O2 -g` unstripped standalone sizes include the probe and debug data;
they are not installed deltas. Exact native text/data/bss and dependency output,
object sizes and binaries are retained per target in JSON. Both probes use only
base C/math facilities: Linux libc/loader, FreeBSD libc/libm/libsys, OpenBSD
libc/libm/loader; no new runtime GUI/toolkit/Go/C++/FFI dependency. The candidate
adds a private C implementation/header and MIT notice during comparison; actual
production integration would still retain current cJSON request parsers. See the
decision for source, license, offline-build and upgrade/ownership costs.

Linux ARM uses Ubuntu 24.04 userspace/GCC under the existing Fedora-kernel
AppleHV Podman VM. Linux AMD64 is Debian 13.7/GCC under QEMU TCG; this comparison
is not Ubuntu 24.04 AMD64 qualification. BSD ARM runs architecture-native guest
code under AppleHV; AMD64 uses TCG on ARM hardware. None is physical/native-host
qualification, and no external X/font libraries were instrumented.

## Controls, failures and cleanup

The private direct-child monotonic supervisor retains a 30-second deadline and
one-second TERM grace. Six host controls cover normal exit, exec failure,
deadline, KILL grace, first-cancel latching and confirmed late completion.
Validation requires confirmed wait, wrapper/child agreement, no timeout/cancel,
and a measured supervised duration below deadline. Requested 1 ms polling does
not guarantee scheduling precision. Only the frozen non-forking probes use it.

The capture oracle has eight passing pure tests; the BSD runner's 41 tests pass,
including same-connection collection, preserved exit 23, default separate
collection and nonarchive refusal. New optional `--inline-evidence` leaves strict
SSH trust, default collection and owning content validation unchanged. Same-
connection archives are capped at 2 GiB, stderr at 64 MiB; gzip-header validation
alone does not certify contents. This finite #17 prerequisite is complete, not
the whole CI-01 issue.

Retained first failures: FreeBSD ARM script exited 0 but separate evidence SSH
banner exchange timed out; empty archive qualifies zero observations. Fresh
run02 used the reviewed same-connection path. Linux ARM first compiled all
objects but lacked descriptive `file`; optional metadata now reports absence,
while exact binary identities and native `size` remain checked. Linux AMD64
first lacked `/usr/bin/time` before its first object; fresh run02 records compile
timing unavailable without adding a package. Native loop timing and compile
success expectations were unchanged. No network root cause or candidate defect
is inferred from these harness boundaries. Review also corrected private
supervisor races, generated setup and cancellation/transfer paths before their
execution. No failed result is relabeled as a pass.

Owned VM/container cleanup receipts distinguish SIGTERM/reaped QEMU from normal
guest poweroff. Canonical images, reusable builds and current compact evidence
are retained; stopped disposable clone/input retirement is recorded separately.
No disk was expanded, no packages installed, and no trust/credentials changed.

Per-PR checks comprise separate source/evidence review, `make check`, whitespace
checks and the affected Python tests above. This comparison changes no X-visible
UI; screenshots/themes are not applicable. Full support/install/default-X,
native-host sanitizers and milestone/release acceptance remain with their owners.
