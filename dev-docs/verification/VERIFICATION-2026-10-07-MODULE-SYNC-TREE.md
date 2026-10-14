# Synchronous module deadline and child-list ownership checkpoint

Status: combined checkpoint accepted locally on 2026-10-07. Native release CI
and physical/toolkit/compositor coverage remain separate.
Owners: OUTPUT-02, DATA-01; OUTPUT-03's startup decision is separate.

The user approved one total monotonic `ModuleTimeout` deadline through synchronous
packet delivery and UNLOCK on 2026-10-07. The existing 30-second default and
positive configurable override remain. Partial reads/writes, commands, readiness
and EINTR cannot refresh it. Nested operations have their own deadline while the
parent continues to elapse. Expiry/invalid clocks retire the producer through its
existing lifecycle. One executing command cannot be preempted.

The synchronous frame now pins its module record across callbacks and death-row
cleanup. Checked nonblocking reads preserve framing and test the same deadline
before retrying. Retirement is idempotent. A final continuation=0 command still
executes before the wait unwinds. Ordinary asynchronous service limits remain
64 KiB, 32 completed packets and 128 attempts per pass. The manual documents the
approved timing contract.

The independent `GetEqualSizeChildren` repair initializes failed output and
validates parent depth/visual/colormap before acquiring the Xlib child list.
FvwmButtons remains its consumer. The previous focused report retains the
allocation/stale-output baseline and failure/empty/filter coverage.

## Frozen scope and focused evidence

Archive: `builddir/codex-2026-10-07/module-sync-tree/source-module-sync-tree.tgz`.
1274 files; SHA-256
`a418ebfce724c79f5af085c47bb7f74a4b1564ddc9cd88d75a8fbf1ac4f0fdee`.
This extends the accepted FIFO/stack/output archive only with the thirteen owned
source/manual/build entries in `owned-files.json`; other desktop/theme/tool
changes in the shared checkout are outside this checkpoint. Changed-mtime sync
checks the guest's accepted baseline before editing any bytes.

Focused Linux development policy/runtime, actual-core synchronous and existing
asynchronous fixture entries pass. The synchronous fixture has eighteen groups:
timely UNLOCK; idle/EINTR readiness; partial/interrupted/blocked writes;
partial/interrupted reads; shared output/unlock duration; activity without refresh;
nested deadlines; callback and final-command retirement pins; terminal readiness
errors; disconnect; termination; clock failure/regression; and real-clock activity.
The real-clock one-second limit returns at approximately one second, compared
with the retained 2.049-second baseline. These are deadline observations, not
end-to-end desktop latency measurements.

Isolated affected-object checks pass on native FreeBSD/OpenBSD ARM64 and scoped
Clang ASan/UBSan. The pure policy/runtime probes enable leak checking; actual-core
startup fixtures disable leak checking because they exit without full core
teardown. Relinking uses unchanged accepted objects for unaffected translation
units; full qualification rebuilds changed header consumers. Evidence:
`builddir/codex-2026-10-06/module-sync-deadline/focused-summary.json` and the three
focused archives. Initial isolated probe builds selected old root-relative
headers and failed compilation; corrected include precedence passes. Gate logs
retain that runner failure. The independent child-list leak-enabled and native
BSD focused evidence remains in the preceding tree-query report.

## Combined gates and remaining limits

| Gate | Result |
| --- | --- |
| Linux ARM64/AMD64 GCC and Clang | 134 entries per profile, all pass |
| FreeBSD/OpenBSD ARM64/AMD64 native compiler | 122 entries per profile, all pass |
| Eight standard profiles | 1024 entries, all pass |
| Staged binary identity | 120 identities; tested bytes match installed bytes after expected Meson RPATH repair |
| Four default BSD X servers | 28 driver invocations: Move, Resize, Scroll, Menu, transient stack/lifecycle, asynchronous output and synchronous deadline on each server |
| Software rendering with XRender disabled | 133 entries, all pass |
| SVG disabled | 113 entries, all pass |
| Scoped Clang ASan/UBSan | 12 entries: five pure/leak-enabled and seven actual-core/leaks-disabled |
| Archived source/integrity | Seven archives; all 1274 SHA-256 hashes/modes match; every payload reads to EOF |

`qualification-summary.json` and `validation.log` under the checkpoint directory
validate source identity, all registered test names/results, installed hashes,
optional/sanitizer counts and default-server evidence. The full standard and
combined sanitizer gates passed on their initial frozen run; no production or
fixture repair was needed during this checkpoint. The earlier focused probe's
include-path compile failure remains historical runner evidence, separately from
these passing full runs. No result from the preceding checkpoint substitutes for
coverage of these new bytes.

The subsequently implemented client-context/transient lookup is outside this
frozen archive and has its own focused report. OUTPUT-03's proposed default
startup deadline also remains separate and unimplemented pending approval.

Native Linux release CI/sanitizers, physical hardware, real toolkit sessions and
external compositors remain separate unrun release checks. No Git remote exists.
Local container/QEMU results are not evidence of those native CI or hardware gates.
