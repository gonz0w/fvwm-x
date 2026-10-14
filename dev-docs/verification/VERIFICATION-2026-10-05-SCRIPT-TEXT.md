# FvwmScript text and list maintenance — 2026-10-05

Status: runtime maintenance verified on all six targets and the Linux Clang leg. The interactive
text-size policy was subsequently approved and verified in the separate
[limit checkpoint](VERIFICATION-2026-10-05-SCRIPT-TEXT-LIMIT.md).

## Change and practical effect

TextField offset walks now use the available byte length once per walk, bound
negative and excessive positions, and clamp incomplete final character strides.
They retain the existing locale callback and encoding behavior. Edits validate
ranges and lengths, allocate before publishing, and retain the original title on
refusal; insertion may safely alias the previous title. Deletion uses checked
in-place movement. Selection copies own a terminated bounded interval.

The shared mouse hit-test and selection-copy paths replace duplicated indexing
and ownership logic. Legacy Delete still acts as Backspace, and legacy typing
keeps its existing selection behavior. The already opt-in keyboard policy keeps
Ctrl-A, Home/End, forward Delete and selection replacement. Replacement is
atomic. TextField.c drops from 786 to 541 lines in this checkpoint; new policy
helpers and tests account for part of the code moved out.

List drawing uses checked wide arithmetic for row/viewport/scroll-thumb and
pointer calculations. Normal count-based rounding is retained. The first visible
row is normalized to the current list, and rendering uses bounded X dimensions.
Narrow runtime sizes omit contents that cannot fit rather than narrowing a
negative width into a large unsigned rectangle. Declared dimensions remain
available to scripts. Each draw scans once to the first option, then walks
visible options sequentially. Skipped expose rows no longer allocate a title,
and List destruction releases its loaded font.

This does not establish a total module memory budget, bound every font-width
operation, or finish Unicode modernization. Flocale's legacy stride callback
still lacks complete four-byte UTF-8 handling. Repeated prefix-width queries can
still be expensive for a long TextField. Text selection's modal event loop,
List's legacy descriptor waits, broader font/geometry arithmetic and resource
budgets remain in SCRIPT-01/INPUT/FD work.

## Verification scope

The candidate is based on the accepted tooling-adoption snapshot and overlays
only the owned widget/helper/test/manual changes. It excludes concurrent desktop,
launcher and theme work. The frozen archive has 1173 source files with hashes and
permission metadata. The runners synchronize changed bytes and refresh their
mtimes, select the source directory explicitly, and use isolated build trees.

This is one C runtime profile per target with Go disabled, PNG/Xcursor/XRender
enabled, debugoptimized builds and real-X tests enabled. Other optional features
are auto-detected and recorded. It does not repeat the earlier three tooling
feature matrices or qualify the unchanged Go component. Linux AMD64's earlier
Go compiler limitation under QEMU user emulation remains separate.

| Target | Full suite | Default BSD X / installed code |
| --- | --- | --- |
| Linux ARM64 GCC | 77/77 passed | installed-code check passed |
| Linux ARM64 Clang | 77/77 passed | installed-code check passed |
| Linux AMD64 GCC | 77/77 passed | installed-code check passed |
| FreeBSD ARM64 | 66/66 passed | 24 installed outcomes passed on default Xorg |
| OpenBSD ARM64 | 66/66 passed | 24 installed outcomes passed on default wsfb |
| FreeBSD AMD64 | 66/66 passed | 24 installed outcomes passed on default Xorg |
| OpenBSD AMD64 | 66/66 passed | 24 installed outcomes passed on default Xenocara |

Native pure checks cover negative/extreme offsets, bounded truncated strides,
linear callback counts, aliased replacement, exact/excess limits, invalid ranges,
SIZE_MAX refusal, deletion, empty text and menu option semantics. List math
coverage includes extreme values, ordinary legacy formulas and viewport/thumb
invariants. The two pure suites also passed host Clang UBSan with strict warnings.
This is not native Linux sanitizer CI, which remains unrun.

The new real-module fixture checks eight outcomes: legacy insertion/arrows/
Backspace/Delete; mouse selection/cut buffers/middle paste; list paging and row
selection; opt-in editing; truncated multibyte clipboard text; invalid text/list
positions and degenerate-size recovery in both modes; partial exposes and core
recovery. It derives visible rows from each server's font geometry. A separate
valid-input control passes the same ordinary editing assertions against the
previously accepted and candidate modules on Linux ARM64.

Fresh installs are checked against the tested binaries after applying Meson's
recorded install-time RPATH transformation to a private copy. Whole-file hashes
are recorded before and after that transformation; an arbitrary byte difference
is not accepted. Installed `fvwm3 --version` is also checked. Default-server
follow-ups use staged core and module executables for the new fixture and prior
widget fixture.

## Verification corrections retained

Early fixture runs assumed a report's last line was data, a specific bitmap-font
height and a non-existent harness collection method. Those assumptions were
repaired. Window-tree scans now tolerate only BadWindow from name/tree requests
while scanning: a child can disappear after module shutdown. Unexpected X errors
still fail the fixture. Failed private-session logs were preserved and only its
three identified leftover processes were stopped.

A reconfigure attempt initially selected the container's unrelated working
source directory. Its outputs are saved as a registration attempt and do not
count as candidate evidence; the corrected build uses explicit source paths.

The first BSD install comparison required byte identity with a build-tree file.
Meson removes the build-time library search path at install, so that comparison
failed despite successful full tests. The corrected verifier computes the exact
expected Meson transformation and compares every resulting byte. Initial failed
logs remain available. These were verification repairs, not runtime-source
exceptions or relaxed test assertions.

## Evidence and remaining decision

Evidence is under `builddir/codex-2026-10-05/script-text-list/`: frozen source,
source fingerprints/modes, remote runners, platform logs, full Meson logs, live
artifacts, install expectations and source checks. `accepted-results.json` records six accepted archives covering seven full-suite
runs, all 1173 source hashes/modes per target, tested/staged binary hashes and
96 installed default-server outcomes. The previous and candidate normal-input
controls each pass five outcomes. QEMU shutdown/release/exit records confirm
that the four isolated snapshots are gone; other guests were preserved.

The current runtime only applies the existing integer representability bound
to interactive edits. Applying the approved 1 MiB ValueLimit to widget titles
would refuse formerly accepted large edits; the approved storage scope excludes
widget allocations. The [proposal](../decisions/SCRIPT-TEXT-LIMIT-PROPOSAL.md) recommends
reusing ValueLimit with atomic refusal and continued smaller edits. Its default
is pending a user decision, not silently enabled by this maintenance batch.
