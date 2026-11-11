# Shared layout and docking foundation

The affected Linux ARM64 core build and five unit checks passed together.
The new units cover actual-pixel columns and master-stack cells and exact
docking connector resolution. Existing workspace and palette units cover
the overview's placement revision check, including an unchanged rectangle
with a newer revision. The revision advances at frame, desk and monitor
placement commits and ignores viewport translation.

The initial run built the core and passed both new policy units. The overview
test reused its output row before checking the prepared command. Reordering
the assertions qualified both admission and dispatch refusal. The repair
rebuilt only that unit and reused the core and policy passes. The owned guest
shut down cleanly.

[Source and binary hashes, checks and shutdown receipt](VERIFICATION-2026-11-11-PLACEMENT-FOUNDATION.json)

The planner and connector resolver are tested policy code. The layout
command, docking profile storage and guarded restore consumers are still
in development. The placement hooks need native coverage with those
consumers. One mixed-monitor run will cover the new actions alongside the
open #221 and #223 cases. [Batch plan](../decisions/WORKSPACE-LAYOUT-DOCKING-2026-11-11.md)
