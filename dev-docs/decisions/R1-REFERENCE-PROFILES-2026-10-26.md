# R1 reference profiles and resource receipts

Owner: Cam; #209/#29/#226. This checkpoint freezes reference definitions and
publishes empirical proposals for review. It does not accept full R1 budgets or
close those three owners. [The manifest](R1-REFERENCE-PROFILES-2026-10-26.json)
contains supported Meson recipes, process boundaries, workloads and missing rows;
[the observation report](../verification/VERIFICATION-2026-10-26-R1-REFERENCE-PROFILES.md)
binds actual results to exact artifacts. The tracker owns live status.

## Reference populations

| Profile | Build and runtime meaning | Current evidence |
| --- | --- | --- |
| Minimal build | Existing optional feature switches disabled; core-only runtime | Recipe frozen; reduced-build qualification and core-only costs remain open |
| Desktop | Shipped configuration, explicit PNG/SVG/Xcursor/XRender, other existing features auto | Three plain Linux ARM64 observations; requested auto resolved separately |
| Individual optional extra | One named feature added to an exact desktop/minimal parent | Pair and workload selected when that feature exists; currently unavailable |
| Development | Desktop features plus noninstalled live-X test targets | Reused normal GCC cache; same runtime artifacts, separate instrumentation scope |

These definitions add no profile selector, build backend, dependency or service.
Meson remains authoritative. Test targets do not become installed desktop
processes. The current **minimal-instrumented** workload uses the desktop-feature
build: core CPU/HWM are measured while FvwmMFL and a synthetic client exercise it.
It does not qualify the reduced build or measure their combined memory.

Requested options and observed results are different fields. `auto` requires
configure evidence; absent dependencies are unavailable, never assumed enabled.
Future platform rows retain their real availability. A complete common install
can contain unused programs; installed bytes and a resolved library-file closure
are separate from resident process memory and actual loaded mappings.

## Compare the frozen reference

From the repository root, with Python 3.9+:

```sh
python3 tests/test_profile_report.py
python3 tools/maintenance/profile_report.py \
  dev-docs/verification/R1-BASELINE-2026-10-26-DESKTOP.json
python3 tools/maintenance/profile_report.py candidate-receipt.json \
  --baseline dev-docs/verification/R1-BASELINE-2026-10-26-DESKTOP.json
```

The helper reads at most 1 MiB per normalized receipt and writes JSON to stdout.
It does not run measurements, change the baseline, write files, start services
or accept a build. Invalid/mismatched receipts return status 2 with a diagnostic.
The [checked-in desktop](../verification/R1-BASELINE-2026-10-26-DESKTOP.json)
and [instrumented WM](../verification/R1-BASELINE-2026-10-26-MINIMAL.json)
receipts are small schema examples with actual values and coverage.

Each context identifies a fixed profile, workload/version, config, source and
artifact populations, environment, compiler, instrumentation and effective
features. Context digest inputs and raw-key mappings are in the observation
JSON. Preserve those inputs when collecting a candidate. Source revision and
artifact identity may differ and remain visible in output; all comparison
context and metric units/scopes/coverage/quantum must match. A changed population
requires an explicit paired profile/extra design and reviewed budget disposition,
rather than editing identity strings to make a comparison pass.

For at least three independent runs with complete coverage, the proposed
envelope is `maximum + max(observed range, counter quantum)`. One observed
repeat range gives a traceable allowance; three repetitions do not establish a
tail bound or statistical confidence. Zero quantum means no additional known
counter floor, not perfect clock resolution. The maximum candidate observation
is compared to the envelope derived solely from the frozen baseline. No
candidate value can enlarge its reference envelope.

Missing/null values and partial/unavailable coverage yield no numeric envelope.
A unique run ID asserts independent collection; the helper cannot prove that
assertion or that an identity string represents a truthful manifest. Review
provenance and coverage along with numbers. Never pool architectures, historical
installations or GC/sanitizer runs into ordinary repeat variance.

## Handoff and stopping boundary

PR256 and the release roadmap are accepted merges. Existing M1–M4 and
settings/IPC/browser evidence remains accepted. Bounded shared-contract work can
proceed on its actual prerequisites. A new default resident or mandatory
dependency still needs reviewed applicable numeric budgets and incremental plus
complete costs. Above-envelope observations require an attributed fix, an
explicit reviewed budget revision, or a release-blocking disposition.

Before RC, #29 owns complete session steady/peak resource accounting, true idle
wakeups, geometry/redraw/X-request metrics and complete budget dispositions;
#5 owns physical/native qualification. #226 owns reduced/other-platform profiles,
per-extra costs and clean archive/build/install/offline acceptance. The manifest
lists each missing row. These finite gaps do not reopen R2/R3 audits or mandate
an OS/theme matrix per developer-tool change. #209 remains open for the accepted
foundation handoff; this subset is not permission to claim its full completion.
