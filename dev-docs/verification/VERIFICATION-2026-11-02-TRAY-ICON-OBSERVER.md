# A3 private tray acquisition observation — 2026-11-02

Scope: THEME-02 #214 / ICON-01 #215, base merged PR347
`04e4e31a97ccad3e5306623b0cbc9daeb13116b0`. This adds optional non-installed
Linux observation binaries and a private-bus/native-X fixture. The shipped tray
source, ordinary target, dependencies and configuration contract are unchanged.
The [observation contract and recipes](TRAY-ICON-OBSERVER-2026-11-02.md) describe
its bounds, identity checks and expected negative control. The
[machine-readable receipt](VERIFICATION-2026-11-02-TRAY-ICON-OBSERVER.json)
separates source/build outcomes from pending native execution.

## Completed focused checks

- Repository foundation and whitespace checks pass.
- Three Python sources compile for syntax without importing the live-X fixture.
- The generator produces precisely the intended one-predicate substitution;
  missing or duplicated predicates raise before changing an existing output.
  Original product source bytes remain unchanged. These are three executed
  build-tool checks, not product or native-X fixture execution.
- Separate source/plan review and an affected build-driver repair review have
  no blocking findings. The final Linux ARM64 existing GCC profile compiles ten
  objects and links both private variants. All 12 compiler/link commands pass;
  54 exported files have verified hashes. Build/quiesce/export takes 3.358s
  after lab readiness (2.120s in compiler/link processes); startup/shutdown are
  additional. This is compilation/linking, not candidate execution.
- The original driver stopped before its first compiler because exact cached
  per-source profile selection was unavailable. Its failed receipt is retained.
  The repaired driver requires the exact production SNI UI output identity and
  applies that common Tray-target profile to all component sources. Both labs
  independently confirm the container and machine stopped. No package, install,
  disk/resource change or other workspace build mutation occurred.
- Native Meson Python import is unavailable; no Meson AST/configuration pass is
  claimed. Direct affected compilation/linking uses the retained validated Ninja
  dependency recipe, fresh component objects and pinned cached library inputs.
  Actual private Meson target configuration remains queued.

## Remaining acceptance

Private Meson target configuration and the positive/expected-negative trace
outcomes remain unqualified. Hold/release and exact-path loader observation are
not inferred from source review or screenshots. A1–A4 runtime/race, preview
recovery, ordinary visibility/keyboard/fallback and Slate/Dune/enlarged-classic
screenshots remain at the shared appearance checkpoint. No product/fixture has
been run for A3. No install, support-matrix or release pass is claimed. This
checkpoint closes neither #214 nor #215 and does not enable clipboard capture.

This test-only slice leaves the periodic implementation counter at 4/5 against
the preserved PR341 reference. Instrumented 20ms wakeups are excluded from
product performance evidence. The lead continues the selected implementation
fallback while temper_implementer is unavailable; backend model identities are
unverified. Separate final integrated and signed-delivery review remain required.

## Native build identities

| Variant | SHA-256 | File bytes |
| --- | --- | ---: |
| Positive | `6eef8f27b2415f01e79434dda98546bb407a238a44818dad612d13b6d1d9d12b` | 2,087,200 |
| Control | `71a4ebc44c71a358f86721d22ff2f41f0c0baf6ecd9df95dfe8f3635221fb054` | 2,087,024 |

Existing AppleHV Linux ARM64 container/profile, generated configuration and
library pins are preserved in the JSON/owner receipts. No private binary was
invoked. Instrumented file size is not a production size or performance delta.
