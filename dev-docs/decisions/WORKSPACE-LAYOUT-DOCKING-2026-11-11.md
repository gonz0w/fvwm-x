# Layout and docking batch

This batch owns #220 per-desktop layout and #222 monitor-role docking
profiles. PR377 provides the accepted workspace overview and scratchpad
baseline. Carry the remaining #221 and #223 mixed-monitor cases into
the same native checkpoint as these placement consumers.

## Placement ownership

The core remains the owner of placement. The overview already resolves
window allocation identities and validates the committed topology epoch.
Those checks cover target lifetime and destination freshness. Restoring a
previous placement also requires proof that a newer writer has not taken
ownership, including a move away and back to the same coordinates.

Start at the authoritative geometry and workspace mutation paths in
`fvwm/frame.c`, `fvwm/geometry.c`, and `fvwm/virtual.c`. Establish a bounded
placement revision and invalidate stale restore records when an external
placement commits. Include workspace and monitor ownership in the guarded
facts. Do not advance the revision on a read or a redundant configure.
Separate viewport translation from changes to a window's virtual placement.

A captured record carries the allocation identity, placement revision and
committed topology epoch. Prepare against copied facts and resolve the
allocation before each operation. Record the observed constrained result
of an owned move or resize. Refuse restoration when a newer placement or
unsupported state supersedes that result. Destroyed windows retire their
records. XIDs, output IDs and monitor name atoms belong to the current
server session.

## Consumers

Layout selection is explicit per desktop. Begin with floating, columns and
master-stack. Use the existing snapping styles and command engine. Plan
rectangles in actual pixels within the committed, strut-aware work area.
Delegate size-hint constraints to the existing core resize path. Honor
user/rule exclusions before applying a plan, including transients, sticky
windows, fullscreen windows and user-pinned placement. Restore only the
changes still owned by the layout operation. Keep automatic layout triggers
out of the first slice.

Docking profiles use named user roles mapped to explicit connector names.
Use bounded data-only storage through the existing private settings
publication patterns. Define the selected panel, workspace and window
placement fields before adding persistence. Resolve missing or ambiguous
roles to a reported reachable fallback. Preserve client dimensions during
docking placement. Across restart, restore only an unambiguous match from
the explicit profile rules with current ownership established afresh.

Reuse `FScreenGetTopologySnapshot()`, the existing monitor resolver and
`EWMH_GetWorkAreaIntersection()`. Retain the existing FVWM commands and
configuration. `FvwmRearrange` is the baseline for explicit tiling behavior.
Its window-list and command path lacks the per-placement restore ownership
needed by this batch.

## One mixed-monitor checkpoint

Reuse the owned Xorg dummy setup from
`tests/live_x/notify_topology_live.py`. Use two outputs at different scales
with a nonzero secondary origin and a reserved strut. Observe committed
epochs, scale factors, work areas and actual window geometry.

1. Apply layout to the selected desk and verify constrained geometry for a
   representative size-hinted client.
2. Capture docking and overview targets, change the secondary scale and
   primary output, and check stale actions are refused.
3. Disable the secondary output. Verify reachable docking placement,
   scratchpad summon into the current work area, and overview refresh.
4. Re-enable the output. Verify that a newer user move survives restore and
   a destroyed allocation cannot receive an old action.

Check that docking and scratchpad transitions preserve client pixel sizes.
Capture one mixed-monitor scene. Reuse the accepted #374 and #377 theme,
keyboard and placement checks where their inputs remain valid. Physical
docking, real-terminal provider and actual XID reuse retain their own
qualification. Schedule the run when both placement consumers are available.
