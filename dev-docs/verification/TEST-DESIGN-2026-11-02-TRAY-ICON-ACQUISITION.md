# A2 tray icon acquisition: pending native observation

THEME-02 #214 / ICON-01 #215. PR347 recorded this design before its fixture
was authored. A3 now supplies [test-only observation source](../../tests/tray_icon_probe.c),
[private control generation](../../tests/tray_icon_control_source.py) and the
[native case source](../../tests/live_x/tray_icon_acquisition_cases.py).
Compilation/link/runtime qualification remain pending; source authoring is not
an executed outcome. See [invocation and limits](TRAY-ICON-OBSERVER-2026-11-02.md).
It belongs to A1–A4; preserve ordinary tray fixtures and theme captures.

The result-admission boundary must reject a batch requested under an older
local theme/size/scale acquisition, including an ABA return to the same tuple.
Menu liveness, item identity/generation, menu generation and node bounds remain
independent requirements. Counter exhaustion stays terminal for that UI owner.
No worker-wire format changes or shared disk-cache removal are selected.

## Independent oracle

Use a load-attempt observation, not only pixels. The existing menu reset can
drop an incorrectly admitted picture again before painting. A screenshot alone
therefore does not establish whether an old acquisition was admitted.

1. Reuse `tray_modern_live.py`'s private bus, actual tray and menu node icon
   `tray-fixture` from `tray_sni_client.c`. Keep item icons unnamed so the held
   completed batch is unambiguously a menu request.
2. Link a private, test-only observation object around `tray_icons_ready` and
   `tray_icons_busy`. When the real menu batch completes, record its exact result
   path and hold admission. During the hold return ready=false **and busy=true**;
   otherwise the UI could start another batch and overwrite retained bookkeeping.
3. Change the configured theme while that batch is held. Observe the real
   `tray_sni_sync` completing with the new tuple before releasing the result.
   Require the menu still open and identical item/menu identities; a changed
   menu would fail to exercise this acquisition-specific case.
4. Observe bounded exact-path calls to `PCacheFvwmPictureAtScale`, forwarding to
   the real loader. Require zero calls for the held old path and a positive load
   for the fresh theme. Keep the existing paint/fallback checks as separate
   positive coverage, with the enlarged and other theme configurations.

The gate must have the existing finite harness deadline, capped records and an
unconditional cleanup release. No production test hook, additional supervisor,
package or default behavior is needed.

## Negative control and remaining work

For the same private test build, disable only the acquisition-mismatch predicate
and preserve the added menu liveness/identity checks. The unchanged fixture must
then observe an old-path load attempt. The private control source is authored, not executed;
it must not be described as visible stale paint or a measured performance gain.

Also cover theme/size/scale ABA, menu close or item replacement while pending,
missing/disabled formats, existing fallback, owner shutdown and acquisition
exhaustion. Review the authored observation object and fixture before dispatch;
record exact source/binary identities, original failures and cleanup. This design
and the affected object compilation do not complete those runtime criteria or
close #214/#215.
