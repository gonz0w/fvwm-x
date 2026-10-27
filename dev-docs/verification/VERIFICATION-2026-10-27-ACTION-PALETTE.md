# Shared action palette: ACTIONS-01 closeout

Owner: Cam. Base: merged PR269, `842758f682ad529651e2aea230c4c64fe55ebc6e`.
Candidate: `codex/r1-action-palette`. Target: the complete R1 Major #219 owner;
the issue closes on merge, after all five frozen criteria are qualified.

[Contract](../decisions/ACTION-PALETTE-2026-10-27.md),
[acceptance receipt](VERIFICATION-2026-10-27-ACTION-PALETTE.json),
[actual captures](../../doc/development-preview.md#shared-action-palette).
Optional file/notes adapters and broader file-manager, clipboard, theme, resource
and accessibility owners retain their separate acceptance. No criterion was
reduced or split to obtain closure.

## Frozen acceptance map

| Criterion | Implementation and observed result |
| --- | --- |
| Four baseline providers, opaque identity/generation, honest coverage | Existing resident FvwmDesktop combines copied core windows/desktops, committed application discovery and eight finite typed actions. Native initial inventory is 4/6/3/8. Core allocation/incarnation identity survives title changes and refuses stale/reused targets; desktop inventory follows six advertised desks in the fixture. Missing data is explicit. No file/notes backend or executable command-completion catalog. |
| Bounded/cancelled/filterable queries and supersession | Pure model caps 512 windows, 128 desktops, 500 applications, 16 registered actions, 1,156 combined rows and 128 UTF-8 query bytes. Core scans at most 4,096 managed records, pages at most 16 rows, retains at most four snapshots within a statically checked 1 MiB bound and allocates rows lazily. One active/latest pending refresh and native ACK batches preserve draft/selection/focus/scroll. Native sixteen-intent pressure starts two requests, publishes one and rejects one. Literal case-insensitive filtering, printable Unicode and stale callback refusal pass. |
| Literal typed actions, target revalidation, shared engine | Real native window focus/raise, application literal argv, Settings menu and desktop 5 activation pass. Core revalidates copied opaque targets before fixed command-engine operations, relooking up a window after recursive effects. App full-data identity and registered capabilities are checked again. No row label is a command. Destroyed targets, old app callbacks and old core incarnation actions are refused. |
| Accessible labels/type marks, focus and independent loss behavior | One optional native Form, Super+A, F3/F5/Tab/arrows/Space/Enter/Escape, textual provider/type labels and generic native marks. Super+P Run remains. Actual focus returns on Escape; explicit target activation disarms late Form focus return. Application loss retains disabled last-good data while other providers remain usable; refresh recovers. Closing/reloading/module teardown releases exact snapshot/lifetime. No daemon, database, extra resident or mandatory toolkit. |
| Source-bound policy/real-X, 2–3 themes/enlarged text and affected support | Combined Linux incremental core/Desktop build, four C policy/production-cache tests, five Go packages, two race packages, scoped vet/catalog, three leak-enabled C sanitizer tests and one three-profile native checkpoint. All 21 captures inspected independently; six published. Affected six-target scope and exact commands/identities are recorded below and in the receipt. |

## Actual focused verification

Linux ARM64 uses the owned Podman Linux guest, GCC and the retained Go SDK/cache;
the actual OS/ABI executes in a VM. It is not physical-hardware qualification.
Only affected incremental targets and tests ran. Exact environment, commands,
source/binary hashes and stdout are retained in the verified Linux archive.

- `meson test ... palette-policy action-palette-snapshots core-query-json module-runtime`:
  four pass. Snapshot tests use production storage with owned synthetic core
  records; cover scan/row/owner bounds, cookie release/replacement, allocation/XID
  reuse, skip/destroy flags, advertised desks/topology and fixed engine syntax.
- `go test` for `internal/actionpalette`, `internal/desktopcontroller`,
  `internal/collection`, `internal/viewbinding`, `cmd/fvwmdesktop`: five package
  outcomes pass by composition, preserving the initial environment failures.
- `go test -race` for `internal/actionpalette` and `internal/desktopcontroller`:
  two pass; scoped vet and command-catalog parity pass.
- Separate ASan/UBSan/leak-enabled affected C tests: `palette-policy`,
  `action-palette-snapshots`, `module-runtime`, three pass. This is scoped test
  qualification, not whole-core sanitizer execution or native-CPU assurance.
- `python3 tests/live_x/action_palette_live.py --build BUILD --artifacts-dir OUT
  --profiles slate-dark dune-light classic-large`: all three pass. Actual
  Xvfb/XTest, Slate dark, Dune light and DejaVu Sans Mono 18pt enlarged text.
- Affected `FvwmDesktop.adoc` renders through Asciidoctor manpage backend without
  diagnostics. No fresh mandoc lint or unrelated manuals/install runs are claimed.

Real native cases cover all four effects, focus return, repeated paged inventory,
localization/literal data, superseded filtering, app loss/recovery, stale callback,
explicit owner release, a real 520-client workload capped at exactly 512 windows,
refresh pressure and actual core restart with a surviving client. The test
observer alone permits 16,384 tree nodes/depth five for decorated synthetic
clients; product scan/row bounds remain 4,096/512.

Each profile observes 4,055 relevant native commands, maximum 167 bytes against
the existing 1,000-byte transport limit. Three-second open binding counters stay
unchanged; three seconds closed emit no unsolicited palette commands. Existing
Desktop polling/status sampling continues. These observations do not measure
kernel wakeups, CPU latency or a whole-desktop release resource budget.

The final before-restart fences record 16 module waits/four probe exits; final
fences record 17/five. Xvfb, initial core and restarted core exit zero. No final
unexpected observer X errors, cleanup failures or unreaped owned children.

## Affected platform scope

All six affected actual-OS targets pass. Linux ARM64 is the focused integration
owner above; the five additional default profiles each reconfigure an owned
incremental build, build core/Desktop plus four C probes, pass those four policies
and command-catalog parity, then run `go test ./internal/actionpalette
./internal/desktopcontroller ./cmd/fvwmdesktop` once.

| Target | Compiler | Result | Verified capture |
| --- | --- | --- | --- |
| Linux ARM64 | GCC, retained Linux owner profile | PASS | 43 Linux receipt files; 1,511 native payload files |
| Linux AMD64 | GCC 14.2.0 | PASS | 40 files |
| FreeBSD 15.1 ARM64 | Clang 19.1.7 | PASS | 40 files |
| FreeBSD 15.1 AMD64 | Clang 19.1.7 | PASS | 40 files |
| OpenBSD 8.0 ARM64 | Clang 22.1.6 | PASS | 42 files |
| OpenBSD 8.0 AMD64 | Clang 22.1.6 | PASS | 42 files |

Go 1.27.2 SDK hashes, actual OS/compiler/options, selected binary/probe hashes,
commands and archive identities are recorded in the receipt. Runtime C/Go files
match the tested canonical source archive
`ce3b0052398213571bedd946b01823f0f9cd75c058c76adde5cb37d5fd33fc58`.
Final documentation/gallery and native fixture changes remain separately recorded;
the final fixture was exercised by the lead's three-profile native run.

The first OpenBSD ARM64 attempt stopped before configure because a restored cache
retained obsolete `cairo=enabled`. Its failed archive is preserved. The approved
exact three-key retired-option whitelist removed only the present `cairo` key;
before/after SHA-256 and `other_options_equal=true` receipts preserve all other
bytes/options. Fresh unfinished OpenBSD gates then passed. No runtime dependency
choice, cache wipe or failing test retry was hidden. Three persistent VMs powered
off normally with zero status; all disposable QEMU runners exited zero, ephemeral
keys are absent and assigned ports/disks are unused. The lead Linux container and
VM are stopped too.

The new native query/action boundary justifies these scoped actual-OS/architecture
build/policy gates. Unchanged server effects remain with existing command-engine
qualification. No all-module/default build, optional-profile matrix, staged install,
BSD default-server or release matrix was repeated for this PR. Those checks are
not new passes. Existing release/native-CPU/physical evidence requirements remain.

## Review corrections and retained failures

Independent source review found that a rejected temporary model could complete
its binding ticket before validation. The implementation now validates a detached
candidate first; regression coverage requires Failed, unchanged Published and
incremented Rejected. Source review reconfirmed the fix before final qualification.
No runtime correction follows the frozen support/native candidate.

The original early build missing an include and standalone module-runtime fixture
missing the new cleanup observation stub are preserved. Both were repaired before
the combined C gate. The runtime fixture observes owner retirement; production
snapshot freeing has its own real-helper test.

Low guest disk space required build scratch in noexec `/dev/shm`. Initial Go
execution failed with permission denied; a private test runner copies one exact
binary to executable disk, records its hash, runs it and removes it. An existing
locker test then inherited its helper directory on noexec scratch. Giving the
child `TMPDIR=/tmp` and `GOTMPDIR=/tmp` fixed that environment; the complete
controller package and both race packages passed. No locker product behavior or
mount permissions changed. Original failures/diagnostics remain in the archive.

Five original native fixture failures are retained: independent-producer query
observation ordering; an incorrect full-metacharacter presentation expectation;
expecting Settings to open an editor instead of its existing menu; an observer
node cap too small for 520 decorated clients; and a receipt race after core leader
exit. Each correction changes the fixture observation only. Final evidence uses
exact-query/ACK quiescence, actual menu pixels and Escape, the private observer
bound, and actual bounded child-wait receipts. No failing run is relabeled passed.

## Visible limits and costs

Long result details and unavailable reasons can clip, especially with enlarged
text. The existing worker-failure notice may cover rows in loss/recovery captures.
Provider counts, primary state, controls and keyboard help remain readable.
Core truncation is aggregate and appears on both Windows and Desktops even when
only the window cap was reached. No omitted inventory is labeled complete.

Unicode display and literal filtering were observed; IME/shaping, screen readers,
physical display/toolkit/compositor behavior and all-theme accessibility were not
qualified. Real desktop activation here is single-screen/global mode. Other
monitor modes rely on explicit committed topology guards and existing shared
command-engine behavior; this run is not a new multimonitor qualification.
Actual XID reuse did not occur in the native sample; forced allocation/XID reuse
is covered by the separate production C fixture.

The retained Linux binaries grow from 5,479,448 to 5,521,504 bytes for core
(+42,056), and 5,580,640 to 5,739,035 bytes for Desktop (+158,395). These unstripped
file sizes are not RSS or performance measurements. No new installed executable,
dependency, persistent store or default resident is added. Snapshot storage has
an explicit lazy bound. PR269 reset the periodic comparison to 0/5; this runtime
merge makes 1/5, so no periodic performance run is due.

## Evidence and upkeep

Canonical private archives have full SHA-256, regular-file membership, sizes,
modes and gzip CRC verification; the compact public receipt records identities.
Only verified exact completed guest captures were retired, preserving source,
build caches, VM disks, prior performance references and canonical host evidence.
Original failures are retained compactly. The reviewer examined all 21 captures,
source identities and native receipts; this is separate agent review, not
independent human assurance. Requested Sol/high implementation/review and
Astra/high scope routing is recorded; actual model execution is not independently
attested. GitHub #219 and Project state mirror review only after delivery.
