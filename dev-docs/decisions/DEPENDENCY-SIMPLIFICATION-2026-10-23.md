# Dependency simplification

Accepted user direction, 2026-10-23. This records the requested issue updates
and additions. Implementation, dependency adoption and removals remain queued;
the actual build contract remains in `meson.build`, `meson.options` and
[INSTALL.md](../../INSTALL.md).

## Scope and scheduling

The compatibility baseline is FVWM 3.x configuration files: supported commands,
arguments, bindings, functions and themes remain useful. The user explicitly
approves removing Perl-specific facilities and pre-FVWM3 migration components.
Document their exact disposition and replacement examples without repeating
the same approval request. Other deliberate FVWM 3.x configuration breaks still
need a decision. Preserve user files and Git/verification history.

Keep modern XDG and directory menus, native FvwmScript, XPM and applicable
X11/ICCCM behavior. Age, a name containing "legacy", or a lexical search does
not prove functionality is obsolete. Retire obsolete implementations after
their supported behavior has a qualified owner.

Scheduling superseded by the later user request on 2026-10-23: the six children
are selected under the [maintenance-first batch register](MAINTENANCE-FIRST-BATCHES-2026-10-23.md),
in milestone 05 / P2 with stage 05 ownership. #179/#181 are Ready; the other
children stay Backlog until their batch/prerequisites are ready. New feature
pickup is paused. Existing accepted evidence remains accepted; the actual
build/install contract changes only when implementation is qualified and merged.

## Language and build direction

- Keep the C11 core, native UI and X/resource ownership. This is no authorization
  to rewrite working native modules.
- Make Go mandatory for building the applicable utilities and services, using
  the existing approved patched compiler policy and offline vendored inputs.
  Installed executables need neither a Go compiler nor Python/Perl interpreters
  for migrated functions. Remove disabled-Go install branches at their checkpoint.
- Remove all first-party Perl. Go owns utility/service replacements; Lua owns
  user scripting examples and policies where appropriate. Arbitrary Perl scripts
  will not be translated automatically. Runtime/install retirement and developer
  tooling retirement have separate owners and completion claims.
- Remove first-party Python runtime helpers, PyXDG and obsolete PIL/Pillow or
  ImageMagick converter fallbacks. Remaining owned build/docs/tests/maintenance
  helpers move in bounded parity groups, preserving verification coverage.
- Deliver optional isolated Lua scripting over shared typed primitives. Compare
  preferred Shopify/go-lua with official Lua in an isolated native module before
  choosing the engine, language version or coroutine contract. Qualify exact
  identifiers, reload/shutdown, callback/resource bounds and all six targets.
  Do not embed a VM or blocking script execution in the authoritative core loop.
- Keep Meson/Ninja operational. Evaluate Muon compatibility and a representative
  CMake/Ninja prototype before a separate adopt/defer/reject decision. Do not
  implement a bespoke build engine or a repository-wide port for the evaluation.

Distinguish three outcomes: an interpreter-free installed runtime, no owned
Python/Perl tooling, and no Python required for a complete build/test. Meson's
own interpreter and external documentation/test prerequisites prevent claiming
the last outcome merely by porting project helpers. Go being mandatory is an
approved future change, not a description of the present optional-Go build.

## Issue ownership and acceptance

| Owner / child | Bounded outcome |
| --- | --- |
| [RUNTIME-GO-01 #23](https://github.com/gonz0w/fvwm-x/issues/23) / [DEP-GO-01 #179](https://github.com/gonz0w/fvwm-x/issues/179) | Mandatory compiler checks, retired disabled-Go utility branches and qualified offline/default/reduced installs. Broader desktop, launcher/session and screenshot runtime ports remain with #23. Reuse accepted UTIL-01 #24 evidence. |
| [COMPAT-02 #42](https://github.com/gonz0w/fvwm-x/issues/42) / [DEP-XDG-01 #180](https://github.com/gonz0w/fvwm-x/issues/180) | Remove replaced Python/PyXDG and Perl XDG frontends, obsolete image converters and active oracle dependencies; retain menu/settings behavior through qualified Go/native consumers and independent fixtures. |
| [ARCHIVE-01 #40](https://github.com/gonz0w/fvwm-x/issues/40) / [LEGACY-01 #181](https://github.com/gonz0w/fvwm-x/issues/181) | Inventory and retire individually demonstrated pre-3.x-only installed components, including fvwm-convert-2.6 and both language copies, with exact migration notices. Coordinate inactive-code proofs with DEAD-01 #35. |
| [TOOLING-GO-01 #55](https://github.com/gonz0w/fvwm-x/issues/55) / [BUILD-EVAL-01 #182](https://github.com/gonz0w/fvwm-x/issues/182) | Port owned Python/Perl tools in qualified groups. Compare Muon and CMake on real probes, a native module, Go target, generated configuration, tests/install and Linux/BSD support; record gaps and migration cost before adoption. |
| [LIB-04 #46](https://github.com/gonz0w/fvwm-x/issues/46) / [SVG-CONSOLIDATE-01 #183](https://github.com/gonz0w/fvwm-x/issues/183) | Prove the maintained librsvg/Cairo floor, retire obsolete libsvg-cairo branches/aliases, retain SVG-disabled builds and renderer worker bounds. Update packages, manuals and license inventory. |
| [LINK-01 #49](https://github.com/gonz0w/fvwm-x/issues/49) / [LINK-TARGETS-01 #184](https://github.com/gonz0w/fvwm-x/issues/184) | Replace broad dependency bags with target-owned linkage using symbols/link maps and static/transitive requirements; publish before/after runtime/build/license inventories. Reuse qualified xkbcommon removal in PR144. |
| [LUA-01 #50](https://github.com/gonz0w/fvwm-x/issues/50) | Engine/capability decision, bounded isolated scripting host and useful Perl migration examples. No transparent Perl API compatibility promise. |
| [LIB-06 #48](https://github.com/gonz0w/fvwm-x/issues/48) | Compare current cJSON with yyjson on actual bounded FVWM-X serialization, numeric/UTF-8 behavior, allocation failure, memory and latency; adopt, defer or reject explicitly. |
| [COMPAT-01 #3](https://github.com/gonz0w/fvwm-x/issues/3) | Qualify the FVWM 3.x configuration baseline and approved exceptions, retaining independent safety/platform/release gates. |

Native GitHub subissues link each of the six children to its owner. #180 is
blocked by #179; #42 is blocked by #179 and #50. Lua readiness is required for
the selected scripting handoff, not for every independent Go/PyXDG removal.
Tooling migration follows qualified runtime groups without blocking unrelated
feature work. Parent acceptance still covers the remaining framework, installed
metadata, configuration generators, manuals and references.

## Alternatives and licenses

These are candidates, not adopted dependencies or measured improvements:

| Candidate | License / decision boundary |
| --- | --- |
| [Muon](https://muon.build/) | GPL-3.0-only; C implementation of Meson. Check used modules/features and bootstrap/package gaps. |
| [CMake](https://cmake.org/licensing/) | BSD-3-Clause; compare maintained description complexity and complete dependency closure rather than assuming a simpler port. |
| [Official Lua](https://www.lua.org/license.html) / [Shopify/go-lua](https://github.com/Shopify/go-lua) | MIT; native integration versus Go-hosted implementation, language/coroutine support and resource behavior remain a concrete engine decision. |
| [yyjson](https://github.com/ibireme/yyjson) | MIT; actual FVWM-X serialization and failure behavior decide adoption, not upstream parse benchmarks alone. |
| [LunaSVG](https://github.com/sammycage/lunasvg) | MIT, C++; optional #46 comparison of SVG feature fidelity, fonts/text/filters/resources/alpha, dependency closure and six-target cost. No renderer or Cairo replacement is approved here. |

Retain Xlib, libevent, Xft/Fontconfig/FreeType and Cairo where used. No wholesale
XCB, event-library or GUI toolkit migration is implied. Verify exact pinned and
transitive licenses at each adoption checkpoint; a license name alone does not
establish compatibility or release compliance.

## Verification and completion claims

Implementation owners update actual build/install options, manuals, package
recipes, source distributions and dependency/license inventories together.
Retain applicable command behavior and use independent fixtures rather than
perpetual interpreter-based production oracles. Apply focused checks while
developing coherent groups under the risk-based cadence; run an accumulated
cross-platform checkpoint when the changed compiler/install/backend/link contract
requires it, not after each edit;
real-X behavior, native sanitizer and release gates retain their own scope.

Record installed closure/size, memory, CPU and latency before/after where
applicable. Fewer languages or dependencies do not themselves prove faster or
safer execution. This planning update establishes neither implementation nor
new runtime qualification.
