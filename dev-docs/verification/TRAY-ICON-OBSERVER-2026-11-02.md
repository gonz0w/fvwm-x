# Native tray icon acquisition observation

A3 supports THEME-02 #214 / ICON-01 #215. The source and direct private
binary compilation/linking pass; actual Meson configuration, fixture execution
and the A1–A4 appearance checkpoint remain unqualified. This is developer test tooling, not a shipped tray feature.

## Build and invoke

Use an owner-controlled Linux build with `live-x-tests` enabled, optional D-Bus
support and linker wrapping. The private targets are not installed. They disable
LTO for their own sources, use the existing tray dependencies and do not change
the production target or its flags. Unsupported wrapping leaves them unavailable;
do not substitute the installed tray and report an observation pass.

```sh
meson compile -C BUILD FvwmTrayIconProbe FvwmTrayIconControl
python3 tests/live_x/tray_modern_live.py --build BUILD \
    --artifacts-dir POSITIVE-OUT --phase modern --case-group acquisition \
    --tray BUILD/modules/FvwmTray/FvwmTrayIconProbe --acquisition-probe positive
python3 tests/live_x/tray_modern_live.py --build BUILD \
    --artifacts-dir CONTROL-OUT --phase modern --case-group acquisition \
    --tray BUILD/modules/FvwmTray/FvwmTrayIconControl --acquisition-probe control
```

These are pending recipes, not executed commands. Freeze coherent ordinary
core/session/controller/worker/client inputs and both private tray variants in
the native execution plan. Preserve real-X profile, source/binary hashes and
confirmed cleanup. Use the shared checkpoint; no full matrix per helper.

The control differs only in its generated acquisition-mismatch predicate.
Generation refuses a changed or duplicated predicate and never edits production
source. Both variants keep the current menu/item liveness and identity checks.
The same case first holds a completed batch, then requires the new tuple and
unchanged menu identities before release. Both modes require a fresh path load.
Positive mode requires zero old-path attempts. Control mode explicitly requires
an old-path attempt: its successful script exit means the expected counterexample
was detected, not that the deliberately incorrect variant is valid.

## Observation and ownership

The test-only object wraps ready, busy, timeout, model observation, external
synchronization and picture loading. A hold returns ready=false and busy=true
as a pair. It retains copied IDs and paths rather than borrowed model/picture
pointers. A synchronized theme acknowledgement follows the real call. A native
load attempt is recorded before forwarding unchanged arguments to the loader;
eventual pixels are not the admission oracle.

Private Unix datagrams are nonblocking. The endpoint requires an owned private
parent; it never replaces an existing endpoint. Descriptors close on exec, the
receive queue is bounded, one callback handles at most eight requests and paths/
load records have fixed bounds. The Python peer authenticates Linux message
credentials against the exact owned tray process and preserves its birth/exe
handle. No credentials, clipboard bytes or image payloads cross this interface.
Missing instrumentation, identity drift, overflow or malformed replies fail the
case. No test-control method is present in the installed binary.

`FVWM_TRAY_ICON_PROBE` is only consumed by the private linked observation object.
Its bounded 20ms control polling changes test wakeups and is excluded from
product cost/idle comparisons. The helper has no subprocess supervisor. Release
and disarm work during cleanup; the existing harness still owns process shutdown
and confirmed reap. Failed control/profile cleanup remains a failed outcome.

## Remaining qualification

Source syntax, generation, affected object compilation and separate review are
recorded separately from native execution. Actual private Meson target configuration
and both trace outcomes must pass at A1–A4 before using this proof. The ordinary
visibility/keyboard/fallback and three materially different theme/enlarged-text
captures remain separate positive checks. The existing six-target release scope,
physical/toolkit limits and C2–C5 clipboard queue are unchanged. This source-only
batch closes neither #214 nor #215.
