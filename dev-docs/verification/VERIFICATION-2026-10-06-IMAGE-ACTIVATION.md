# IMAGE-01 combined core activation — 2026-10-06

Status: accepted local combined qualification. Native Linux release sanitizer CI
and physical hardware/toolkit/compositor checks remain unrun release requirements.

Normal startup installs the loader after X/visual/error initialization and
before defaults and user configuration. Workers start and image owners publish
only at safe outer event-loop boundaries. An unexpected unbound deferred image
is refused with a diagnostic. Depth-one bitmap colorsets retain their legacy
path through one shared predicate. Modules retain their existing completion path.

SVG, compressed XPM and ambiguous-format core requests now return while decoding
continues. The previous acknowledged image remains until matching completion;
first-load images remain absent until success. The isolated worker and bounded
parent preparation retain approved deadlines, memory limits, decoder order,
relative SVG resources, visual conversion, tint/background rounding and cache
ownership. Restart/shutdown cancel and reap owned work before closing X.

## Qualified source and evidence

Evidence directory: `builddir/codex-2026-10-06/svg-core-activation/`.
Final source: `source-activation-qualified.tgz`, 1261 files, SHA-256
`20111051d8f17206a4ca48a0841e323e5a9904a5ee53eeffa3792760c6c1d668`.
All six platform sources and the separate sanitizer source match every frozen
file hash and permission mode. `qualification-summary.json` records checked
suite composition, source identities, installs and evidence archive hashes.

This source extends the recorded image-owner/effects checkpoints. It does not
qualify another thread's newer desktop/theme or runtime-language changes, or the
subsequent approved button-release batch. Go and generated documentation are
disabled for the C/runtime qualification. Full core/modules are compiled.

| Profile | Qualified test entries | Staged code identities |
| --- | ---: | ---: |
| Linux ARM64 GCC | 119 | 15 |
| Linux ARM64 Clang | 119 | 15 |
| Linux AMD64 GCC | 119 | 15 |
| Linux AMD64 Clang | 119 | 15 |
| FreeBSD ARM64 Clang | 108 | 15 |
| FreeBSD AMD64 Clang | 108 | 15 |
| OpenBSD ARM64 Clang | 108 | 15 |
| OpenBSD AMD64 Clang | 108 | 15 |
| Total | 908 | 120 |

Installed core, worker, fvwm-root and native modules match tested binaries after
exact expected install-rpath rewriting. Patched PNG profiles retain the prior
verified dependency scope; Linux AMD64 uses the recorded Debian security-backport
prefix and explicit assertion. FreeBSD disables XPM, so its gates do not assert
XPM decoder coverage.

All four installed default BSD servers pass 13 image driver invocations each
(52 total): production startup/restart, colorsets, mini/file icons, menus and
real/synthetic decorations at relevant 1×/2× scales, plus software-effect fault
injection. FreeBSD uses scfb, OpenBSD ARM64 uses wsfb and the root-sized monitor
fallback, and OpenBSD AMD64 uses vesa. Server logs are archived. Software fault
injection uses a fixture linked to production objects with a capability override;
the other default-server drivers use the staged production executable.

Linux ARM64 Xcursor-enabled/XRender-disabled passes 118 qualified entries.
The SVG-disabled Clang profile passes 101. Scoped Clang ASan/UBSan has 30 unique
checks: the initial 17 leak-enabled policy/direct-X and 11 leak-disabled actual
core entries, four affected fixture reruns including one additional SVG-isolation
check, and one additional leak-enabled fortified input check. Leak checking for
actual core processes remains disabled because of documented legacy allocations.
These local passes do not replace native release sanitizer CI.

## Behavior and failed-run provenance

The production executable passes pending startup, independent client/timer
service during a stalled worker, supersession and restart with fresh loader
ownership at 24/8-bit X. The combined suite includes image owner cancellation,
publication acknowledgement, effects, shape, decoded averages, cursors, XOR tile,
menus, decorations, mini-icons, file-icon sizing/donation and ordinary runtime
regressions. The initial 11 focused owner/startup entries are preserved separately
in `evidence-first-focused.tgz`.

Initial full-suite source `source-activation-initial.tgz` has SHA-256
`b83984cfc0a1976bc6864ba55cb843be4c1cb14033b6eda6207be114f83e0b08`.
Production C and its build inputs did not change during qualification. Four
fixture corrections and a test-only linker-wrapper change were necessary:

- SVG isolation now requires core command/client service during worker waiting,
  rather than the old synchronous command delay; the standalone module path still
  checks its synchronous deadline.
- XPM cursor-success checks are conditional on compiled XPM support.
- Menu tearoff readback waits for mapping/management and positions the window
  inside the actual framebuffer before XGetImage.
- AMD64 fortified Clang calls `__open64_2`. The image-input injection target now
  wraps both fortified open entry points, retaining descriptor/failure coverage.

Affected fixtures were rerun, including the corrected image-input target on all
standard and optional profiles and scoped sanitizer. Full-suite results are
composed by exact test name with explicit fixture provenance; unaffected passes
remain valid for identical production code. Initial failures, registration
errors and intermediate archives are retained. `fixture-only-source-delta.json`
and `fortify-fixture-delta.json` record the deltas. The intermediate corrected
archive `source-activation-final.tgz` has SHA-256
`54b5563b06eefc59630c019bd55bf8801216495592ab00c546df278450b657c1`.

Evidence collection now uses distinct archive paths for normal/sanitizer sources
sharing one container. Final collected archives were read and validated after a
shared-path collection race was corrected. This affected collection, not source,
compiled code or tests.

Storage latency, broader decoder CPU/permissions isolation and remaining data
input audits are separate open items. This checkpoint closes asynchronous core
image activation and its owned-resource contract, not the entire security audit.
