# Mixed-monitor workspace placement checkpoint

The Linux ARM64 incremental core build, workspace core unit and Go command
metadata unit passed. The checkpoint reused 34 passing commands from the
placement foundation and earlier workspace work. The first build attempt
failed on an execution-context declaration. Correcting the include order
resolved it. An intervening host admission stopped before starting the owned
guest. Comparing the QEMU executable basename resolved that admission issue
under the same VM count and memory limits.

The Slate Xorg dummy scene confirmed DUMMY0 at 1920×1080 and 1× scale and
DUMMY1 at 1280×1024 and 2× scale, with its origin at x=1920. The primary's
dynamic work area started at y=56 after installing the strut. The scene
timed out waiting for all three fixture windows to change and fit within
the work area after `WorkspaceLayout columns`. The scene recorded zero
completed cases and zero screenshots. The observer reported zero unexpected
X errors. Both owned shutdowns were confirmed.

The next focused run can reuse the passing build and unit checks. The
fixture now records pre-command and final observed frame/client geometry,
sets the pointer on the primary just before dispatch, and attempts a
failure screenshot before tearing down the scene. These fixture changes
passed Python syntax checks and still need live execution.

Issues #220, #221, #222 and #223 remain open. Profile persistence, matching
after restart and panel/settings controls remain outside this command
checkpoint.

[Recorded topology, checks and evidence hashes](VERIFICATION-2026-11-11-WORKSPACE-MIXED.json)

## Columns and unplug recovery

Direct rectangle comparisons corrected the placement guard and frame
revision update. A bounded retry for grid-constrained windows fit the
hinted client at 622×985 pixels in a 640×1024 cell. The incremental core
build passed and retained the earlier workspace and metadata unit passes.

The final scene reused the exact built binary and passed six of nine
cases. Columns, owned floating restore, preserving an intervening move,
connected docking, stale overview rejection and overview refresh after
unplug passed. The fallback reported three applied windows. The
fixture then compared their dimensions with a baseline taken before an
earlier cross-scale round trip using ordinary Move. The fixture now
takes a separate measurement immediately before each restore and
records both sides. That change has a Python syntax pass and awaits
live verification. Scratchpad summon after unplug, fallback pixel
preservation and replug restore remain pending.

[Slate columns capture](../../doc/screenshots/workspace-placement-columns-slate-dark.png)

## Final combined result

All nine Slate cases passed on Linux ARM64 with Xorg dummy outputs.
The fallback retained all three client sizes, the same scratchpad
allocation appeared centered on the surviving work area at its existing
client size, and replug restore moved one window while preserving a newer
manual placement and ignoring a retired allocation. The overview published
the active roles after unplug and replug. The exact core binary and all
37 prior passing commands were reused. The test guest and container stopped.

This qualifies the mixed-monitor slice of #220–#223. Persistent profiles,
restart matching, panel and settings controls, real-terminal provider
coverage, actual XID reuse and full platform qualification remain open.

[Final mixed-monitor capture](../../doc/screenshots/workspace-placement-mixed-slate-dark.png)
