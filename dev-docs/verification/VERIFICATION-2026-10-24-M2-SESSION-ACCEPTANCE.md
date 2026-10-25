# M2.8: session observation integrity and installed acceptance

Cam authorized merge, cleanup and continuation of PR200. Canonical commit
`fdd396dd0b446ac60384583fc96f092e19026f8c` retains verified signature/DCO and
PR200's exact tree. GitHub already removed its remote branch; the local branch
is retired. All 1,185 runtime/build inputs and installed/native identities stay
exact. This PR changes test observers and documentation, with no runtime rebuild.

The old sampler began after startup, missed short-lived workers and counted only
thread leaders' context switches. The new bounded observer begins just after
launch, matches PID/task start identities, validates ancestry before retention,
reads all available thread counters and keeps partial/unknown values distinct.
Raw snapshots, phase boundaries, first-sample delay and maximum gap remain.
Twelve seconds of idle covers normal five-second status acquisitions; activity
opens/closes three settings editors and makes twelve settings queries.

Optional fresh cgroup-v2 accounting captures joined descendants' CPU and charged
memory high-water, including short-lived workers. A private existing tracefs
instance attributes fork/exit/wakeup events by task lifetime, refusing complete
counts on loss, truncation, unknown format or ambiguous root birth. No mount,
controller, global tracing or privilege change is made. The observer and X server
stay outside the measured group. Atomic prelaunch/running ownership journals
record boot/process and exact resource creation identities; normal cleanup runs
after session reaping. A narrow expected-BadWindow handler covers borrowed name,
tree and attribute reads, keeps unexpected errors until cleanup and remains
installed through display closure. It does not promise cleanup after XIO or a
native crash.

## Focused evidence

[Machine receipts and raw sample summary](VERIFICATION-2026-10-24-M2-SESSION-ACCEPTANCE.json)
bind four original phase archives, source scopes, commands, identities and limits.
Canonical host captures are under `builddir/codex-2026-10-24/m2-session-acceptance/`.
Full gzip CRC/payload hash/size/mode verification precedes guest retirement.
Source-bound composition retains passed checks without relabeling failed phases.

- Pure proc/identity/coverage, kernel trace/accounting, GC and ownership/X error
  fixtures cover reused or vanished parents, thread counters, unknown memory,
  counter regression, limits, cancellation, loss, narrow cleanup and callback
  lifetime. The final portable suite contains 63 focused cases and runs through
  `make verify`; native affected fixtures also pass.
- A fresh current installed root contains no Python/PyPy interpreter. Default
  desktop/worker/native capture, offline checker and Command/directory/full-menu/
  browser paths pass. Safe backend peers preserve the same held locker across
  controller restart and core logout, then release/reap it. The root has an
  actual character null device, with no procfs; it proves runtime availability,
  not hostile-code isolation. Installed hardlink identities stay exact.
- Three requested notice/font/palette views are captured and inspected for
  readable text/controls. Names identify requested inputs; effective appearance
  and a fully settled bar are not asserted. Earlier source-bound full theme
  rendering evidence keeps its original scope; no new product layout is added.
- One native destroyed-window probe records two exact expected retirements and
  one deliberately unexpected error, then closes the display/owned server.
  Three alternating reference/current session pairs and a separate longer GC
  run pass. Every valid pair has complete kernel accounting/trace-loss checks,
  zero unexpected observer X errors and successful journaled resource cleanup.

## New AMD64 session observations

The preserved PR194 Python source and its receipt-matched AMD64 Go worker run
against the same current C core/modules/data as PR200's Go resident. The worker
was originally cross-build-qualified; this checkpoint executes it. This Debian13
Linux6.12.113 QEMU environment establishes a new reference; do not compare its
numbers with prior ARM64 samples. No reference is rebuilt or tuned.

| Median of three samples | Python reference | Go current |
| --- | ---: | ---: |
| Launch to settings reply/bar title, ms | 2753.858 | 2114.932 |
| Retained idle RSS sum, KiB | 49136 | 32052 |
| Retained idle PSS sum, KiB | 25116 | 16973 |
| Sampled startup RSS sum peak, KiB | 61728 | 44640 |
| Launch/ready-boundary charged peak, bytes | 33267712 | 23871488 |
| Twelve-second idle CPU, ms | 114.083 | 120.341 |
| Idle task wakeup events/second | 18.179 | 35.922 |
| Synthetic key-to-core Echo, ms | 2.773 | 2.460 |

Go startup and retained memory are lower in these samples. Go task wakeups are
consistently higher here; idle CPU ranges overlap (reference 105.552–168.563 ms,
current 111.002–123.828 ms). Do not infer power or general speed from those
counts. Keep the higher wakeup observation for later tuning; no production
change follows this evidence-only checkpoint. RSS/PSS are sums across each poll,
not simultaneous physical memory. Charged peak includes kernel/cache and excludes
pages charged elsewhere. Polling still misses some transient identities; kernel
accounting supplies a different, explicitly scoped measure. Startup includes
observer/IPC/title readiness, not login or completed painting. Key timing is
synthetic, with raw ranges retained. No threshold turns these ratios into passes.

The separate `GODEBUG=gctrace=1` run uses 24 editor cycles/96 queries and forces
no collections. No resident trace cycles are observed, so pause latency remains
unknown; worker stderr is outside that scope. This is not a zero-pause claim.

## Failures, review and continuation

Preserve the first reference's missing executable mode, the unsupported workload
command and the real observer destruction race as failures. Correct fixture mode
and use the existing `edit-settings all` command. The C-level BadWindow exit
bypasses Python cleanup: exact private resource/process diagnostics and trace are
preserved before normal owned-VM poweroff. The corrected X observer is then
qualified after restarting the same unchanged build/profile. None of these
failed attempts is a performance sample. No deadlines or product assertions are
relaxed, and passed policy/interpreter-free checks are retained.

Separate review fixes direct fixture imports, ancestry adoption during detected
parent reuse, runtime-before-kernel cleanup ordering and bounded GC log reads.
The X trap and ownership journal receive additional pure/native checks. A final
post-run helper docstring clarification changes no executed logic. Requested
routed analyst/implementer/reviewer models remain unverified because this surface
returns no observed model/client metadata; no routing cost/speed claim follows.
Agent review is not independent human approval.

Retire 157 verified guest capture/upload/archive duplicates (94,186,188 bytes),
preserving canonical host evidence, source/build/cache/stages/disks and other
owners. Seven successful kernel journals confirm resources removed. Both owned
VM boots end with normal poweroff and QEMU status zero.

GitHub #23 was manually closed on 2026-10-24; preserve that issue state without
inferring completion of the remaining tracker acceptance. #7/#15 remain open.
Native-CPU/full-session owner acceptance, unobserved
GC pauses and physical/hardware/toolkit/compositor/assistive checks remain
explicit. No feature, tuning, M3 or M6/M7 migration is selected. PR194's independent
metadata-scan cadence remains 4/5 production checkpoints after PR200; this
observer-only change leaves installed binaries unchanged and does not trigger
another scan or reset that reference.
