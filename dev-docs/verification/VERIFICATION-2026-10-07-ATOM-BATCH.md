# XIO-01: independent startup atom batches

Status: locally qualified in the [combined atom/expose/icon checkpoint](VERIFICATION-2026-10-07-ATOM-EXPOSE.md).
The focused evidence below remains retained. This change is outside the earlier
accepted frozen JSON checkpoint.

Core startup now interns 28 names together, ICCCM setup interns eight (including
the dynamic WM_S screen name), and EWMH setup interns UTF8_STRING plus all nine
existing tables in one 64-name batch. Names, destinations, EWMH types/list order,
selection ownership and publication barriers remain unchanged. No XCB,
connection/event-owner change or mandatory dependency is introduced.

`libs/atom_batch.c` initializes output to None, calls XInternAtoms and retries
only missing names once on partial failure. Successful entries are retained;
a terminal missing result remains None with a diagnostic, matching the old
single-name result. Empty tables complete without an X request, negative counts
are refused. All production tables have static finite capacity. EWMH uses
bounded local arrays sized from its static tables; core and ICCCM keep their
existing destination owners.

The [Xlib specification](https://www.x.org/releases/current/doc/libX11/libX11/libX11.pdf)
describes XInternAtoms as equivalent to interning individual names while reducing
round-trip exchanges. That is the API contract; the measured local evidence is
call counts and correctness, not a desktop latency benchmark.

## Actual baseline and focused checks

The same actual-core mapping/selection/property assertions pass on the unchanged
frozen JSON baseline: 100 single-name initialization calls plus one independent
colorset _XROOTPMAP_ID call. The new actual-core fixture observes three batches
(28/8/64), retaining the colorset call. Its normal and controlled partial-failure
cases validate every core/ICCCM/EWMH atom name, the ICCCM owner, and exact ordered
_NET_SUPPORTED contents. Partial failure clears one result per batch; only those
three names are retried, with the same final mapping.

Linux unit, actual-core, stack/lifetime and module selection checks pass. Native
FreeBSD/OpenBSD ARM64 isolated objects pass both actual-core cases and pure
failure/count coverage. Private native source copies apply only the owned atom
function changes to the accepted JSON snapshot; they preserve the guest WM build.
The Linux development tree includes shared work and is not claimed as a whole
accepted source checkpoint.

Scoped Clang ASan/UBSan passes the pure atom test with leak checking enabled and
the actual-core fixture with leaks disabled because Capture exits before normal
teardown. This is local emulated evidence, separate from native Linux release CI.

Retained fixture/setup corrections: an initial requested test name was absent;
Motif declarations were missing from the new fixture; its first call-count
assertion omitted the unchanged colorset lookup. Private native includes and the
OpenBSD Xvfb PATH were corrected. A scoped pure-unit link placed its helper after
the static-library group; correcting object order completed that check. These
are recorded fixture/setup failures; no WM behavior change was needed for them.

Sources, logs, baseline/final/native/sanitizer archives and hashes are under
`builddir/codex-2026-10-07/atom-batch/`. Collected archives are checked through EOF.
`working-source-fingerprints.json` identifies the shared development files;
native archive metadata identifies the narrower tested source copies. Full
AMD64/compiler/optional/install/default-server qualification passes in the linked
1291-file combined checkpoint; do not carry JSON-only qualification forward.
