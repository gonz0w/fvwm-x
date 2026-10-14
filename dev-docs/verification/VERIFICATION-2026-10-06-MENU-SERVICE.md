# INPUT-01 active menu interaction — 2026-10-06

Status: locally accepted. Eight compiler profiles pass 984 registered entries,
120 installed code identities and 12 default-BSD-X driver invocations. Optional
profiles pass 128/108 entries; eight scoped ASan/UBSan checks pass. The separate
[viewport checkpoint](VERIFICATION-2026-10-06-VIEWPORT-SERVICE.md) is accepted.

The user approved the [menu contract](../decisions/MENU-INTERACTION-SERVICE-PROPOSAL.md).
Active menus now reuse audited command/timer/transport servicing, finite 100-ms
readiness and termination cancellation. Motion coalescing has a 32-event budget.
A pure monotonic 10-ms tick policy preserves submenu delays despite early module
wakeups. General lifecycle dispatch, topology changes, image publication and
unsafe command/resource mutation remain behind outer unwind. Physical release
still gates surrounding function continuation; no new library or X authority.

Focused actual-core checks cover producer order, safe/unsafe timers, independent
module progress under motion, image completion/deadlines, Escape and held release,
submenu opening/closing delays under early wakeups, keyboard selection, dynamic
actions, deferred MapRequest, termination during submenu waits, mapped image
stability and persistent tear-offs. Existing release and viewport checks pass.
Scoped sanitizers cover one pure policy check with leak detection and seven
actual-core checks without leak detection. No performance benchmark is claimed.

Fixture repairs retain the observed failures. Popup's sticky release behavior
requires Escape before the intended final held wait. The new lifecycle probe
explicitly declares automatic placement, avoiding a separate interactive
placement wait. Selected submenus can retain mapped ancestors through nested
physical-release waits, so the selection check asserts independent progress and
no action before release, then action/unmapping after release. It does not require
all menu windows to disappear while the physical button remains down. A viewport
active-service handshake distinguishes its passive root grab from active Scroll.

Frozen final source: 1267 files, SHA-256
`f111b3804be679ac88911d428a70e972fd7b6b307653034f50bb48a7a38f3fac`.
Evidence is under `builddir/codex-2026-10-06/menu-service/`. Production code is
unchanged between the initial full compiler gates and the final source; only the
menu selection fixture changed. The affected fixture runs on every compiler
profile; registered rows, initial attempts and replacements retain provenance.
Stacking/FIFO changes in the independent development source and unrelated shared
checkout work are excluded. Native Linux release CI and physical toolkit/
compositor checks remain separate unrun requirements.


## Combined checkpoint

`qualification-summary.json` validates all seven archived payloads, registered
names, exact final source hashes/modes and installed/tested code after only
specified RPATH fixups. Linux AMD64 and ARM64 each pass GCC and Clang (129 entries
per profile), and all four BSD Clang profiles pass 117 entries. Each default
BSD X server passes servicing/producer order, submenu/selection/lifecycle behavior
and active-menu image/deadline/termination boundaries. Core/module installation
includes 15 code identities per compiler profile. Go and documentation builds
are disabled for this runtime-only checkpoint.

The original full compiler runs use source archive SHA-256
`4ca5d5b9db8da5fe4da0e17d74d9d04a239dd4b3080f3df76a49b4cab51eeba6`.
Only `menu_interaction_live.py` differs in the final archive. Every guest verifies
that all other source files are identical before applying that fixture correction;
every compiler profile reruns it. Linux AMD64 Clang's initial fixture missed its
pre-release unmapping assertion; its corrected check passes. OpenBSD AMD64 passed
116 initial entries and missed the common release-grab predicate. That unchanged
check and the corrected menu fixture pass the affected rerun. Initial registered
logs, affected replacements and composed final rows are archived and validated;
no full suite was repeated for these fixture changes. The first FreeBSD ARM64
default run also hit the pre-release selection assertion, and the final corrected
driver passes. Its per-guest log was overwritten by the rerun; this report records
the failure rather than claiming an archived original default-driver log.

The software fallback profile passes 128 entries; SVG-disabled passes 108 (the
SVG-dependent menu-image driver is excluded). Scoped sanitizer policy/core split
is 1/7; the independent FIFO/output sanitizer source was reused only after this
menu checkpoint's evidence was collected. None of its later changes are included.
Native Linux release CI and physical toolkit/compositor qualification remain unrun.
