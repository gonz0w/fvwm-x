# Shared font-shadow work — 2026-10-06

Owner: SCRIPT-01 / shared rendering and resource lifetime. Status: implemented and
qualified on 2026-10-06. The user approved a default of 4096 positions
per draw and a finite build-time override up to 65536.
[Decision](../decisions/FONT-SHADOW-WORK-PROPOSAL.md).

## Implementation scope

The shared font loader checks directional and Center shadow work before loading
or caching a new font. `font-shadow-limit` accepts 1–65536 and defaults to 4096;
zero cannot disable the limit. Refused fonts produce a diagnostic. Center
counting uses checked wide arithmetic, and iteration visits its border directly
in the legacy row-major order instead of scanning the excluded interior.
Directional positioning and origin additions use wide arithmetic before checked
conversion. Ordinary admitted shadow geometry, rotations and position order
remain covered by production iterator comparisons.

Core title/icon font replacement prepares the new owned font before releasing
the previous one. Pager label, window and balloon setters preserve usable fonts
on refusal; explicit WindowFont none retains its disabling behavior. Form
reuses the previous draw-table font, or its existing startup default, with an
owned reference. Startup failure guards cover consumers without a usable
fallback. Existing menu/default setters, Buttons inheritance, Ident/IconMan
failure and IconMan tooltip fallback are audited and exercised. Empty font
names now remain on the default-font path instead of advancing past their NUL
through the quote check. This does not establish a whole-font-cache budget or
repair all inherited process teardown allocations.

No new mandatory library, core thread or recursive event dispatch is added.

## Source and environment

Evidence: `builddir/codex-2026-10-06/font-shadow/`. The final snapshot contains
1209 source hashes/modes, based on the accepted 1205-file Script geometry
checkpoint. Seventeen existing files change and four are added. Twelve root-owned
files match the frozen bytes; build graph/manual additions are scoped onto the
accepted baseline. A startup-size wait is added only to the frozen desktop
baseline fixture; concurrent root desktop/tooling/default changes are excluded.

The inherited private profiles are Linux ARM64 GCC 13 and Clang 18 in an
init-backed Podman guest, full-system Linux AMD64 QEMU GCC 14, and native
FreeBSD/OpenBSD ARM64 and AMD64 Clang 19 QEMU guests. Patched PNG provenance and
runtime paths are inherited from the accepted SVG checkpoint. Original user
VMs/shared containers remain untouched.

## Qualification

Final archive validation passes all seven compiler legs:

| Platform | Compiler | Full suite | Default BSD X |
| --- | --- | --- | --- |
| Linux ARM64 | GCC 13 | 92/92 | — |
| Linux ARM64 | Clang 18 | 92/92 | — |
| Linux AMD64 | GCC 14 | 92/92 | — |
| FreeBSD ARM64 | Clang 19 | 81/81 | scfb, 24-bit |
| FreeBSD AMD64 | Clang 19 | 81/81 | scfb, 24-bit |
| OpenBSD ARM64 | Clang 19 | 81/81 | wsfb, 24-bit |
| OpenBSD AMD64 | Clang 19 | 81/81 | vesa, 24-bit |

All 600 full-suite executions and 105 staged executable identities pass.
Each final platform archive matches all 1209 source hashes/modes; installed
bytes match tested output after only Meson's expected RPATH transformation.
The final source is also checked in sanitizer and override archives. `validate.py` verifies these contracts and writes accepted-results.json.

The registered live controller covers eleven font consumer outcomes per leg:
core default/window title/menu refusal and recovery, Script shadow rendering,
Pager replacement, Form previous/startup fallback, Buttons inheritance,
Ident and IconMan failure/recovery, and subsequent core command service.
Separate installed checks pass 33 Linux and 44 default-BSD-X shadow outcomes,
63 Script geometry outcomes, and 224 prior widget/text-list outcomes. An
additional controller passes 21 installed core default/title/icon pixel-state
outcomes across all seven legs.

Five scoped local Clang ASan/UBSan checks pass and cover two pure policies,
the production font probe, and the actual Script geometry/shared consumer controllers. Leak
detection is enabled for pure/font probes; actual core/module runs disable it
because inherited teardown retains allocations. The production font probe
releases Fontconfig's process cache. Native Linux release sanitizer CI remains
configured but unrun; this virtualized local gate does not establish that result.

Separate builds at limits 1 and 65536 each run both pure policies and the actual
production font loader/iterator probe, six focused checks total. Builds at limits
0 and 65537 refuse configuration. Option values are checked in actual Meson
metadata, rather than inferred from command arguments.

The actual core and Script Asciidoctor roff is rendered with native FreeBSD
mandoc. Both baseline/candidate retain two macro errors each; warning counts
remain 33 for the core and 21 for Script. Style diagnostics change 26→29 and
9→11 respectively, with lint exit 3. The rendered text contains the limits and
replacement contract. This is not clean lint or a manual rewrite qualification.

## Bounded performance comparison

A production-iterator-only comparison uses Center size 1, offset 511: 4096
positions per draw. Twenty draws across five runs produce matching checksums.
The accepted baseline scans a 1050625-position square per draw; the candidate
visits only the 4096 border positions. Median time changes from 6.82 ms to
0.62 ms for twenty draws, approximately 11.1× faster. Compiler/link arguments
and production library identities are retained in profile-results.json.
This virtualized ARM64 sample measures position generation only, under local
host load; it makes no whole-render or input-latency claim.

## Retained failures and corrections

Initial qualification exposed fixture/module setup mistakes: missing diagnostic
includes, missing frozen Meson executable references, Pager alias argument
order, Form title quoting, menu-window discovery assumptions and artifact copy
after temporary-directory cleanup. These were corrected before final qualification.

Slow guests exposed partial initial painting and non-viewable XGetImage reads.
The fixture now waits for viewability and repeated stable pixel observations.
It also waits for retired clients to leave the managed-client list before taking
a new Pager baseline. Pixel equality and recovery assertions remain intact.

Later full/installed gates exposed Script reports/main windows appearing before
all widget children, and a Buttons window appearing at its initial 1×1 size
before configured layout. Geometry, text/list and font consumer fixtures now
wait for their complete expected children; the frozen desktop fixture waits for
the asserted configured size. Earlier before-viewability and before-startup-
barrier evidence is retained separately and is not accepted evidence. Driver
source-digest checks were corrected after a quoting setup error; final installed drivers
require the exact source digest and successful full suite before launch.

## Remaining work

The position limit does not bound glyph count, Unicode/font-set scans, font
matching/cache population, individual library calls, total pixel area or X
server storage. Modal servicing and wider owner/descriptor cleanup remain open.

The next concrete responsiveness decision is
[asynchronous core SVG completion](../decisions/SVG-ASYNC-REPLACEMENT-PROPOSAL.md).
The accepted decoder has finite limits, but its parent still waits synchronously.
Returning before pixels/final dimensions are ready changes observable theme
completion and needs an explicit compatibility decision. Retaining previous
usable pictures, single-threaded commits and stale-owner/generation checks are
part of the proposal. Primary source I/O, other formats, wider storage/permission
isolation, package review, physical hardware/compositor/toolkit qualification
and native Linux CI remain open. The earlier unexplained FreeBSD ARM64
interaction timing observation remains in VERIFY-01.
