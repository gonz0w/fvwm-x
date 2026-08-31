---
schema_version: 1
open_count: 0
waived_count: 0
fixed_count: 8
total_count: 8
last_updated: 2026-08-31T15:40:57.836Z
---

# Broken Windows Ledger

> Cross-phase defect register. With `workflow.windows_enforce` enabled, `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 01 | deviation | tests/run_in_linux.py |  | Darwin verification stages source in a case-sensitive Podman volume instead of using the host bind as the working directory | fixed |  | 2026-08-30T12:36:38.136Z | 2026-08-30T12:37:49.405Z |
| 2 | 01 | deviation | libs/monitor_topology.c |  | Partial-allocation destruction required a NULL monitor-array guard | fixed |  | 2026-08-30T12:36:38.221Z | 2026-08-30T12:37:49.484Z |
| 3 | 01 | deviation | tests/run_in_linux.py |  | Reusable Darwin staging required stale-source cleanup before each copy | fixed |  | 2026-08-30T12:36:38.307Z | 2026-08-30T12:37:49.565Z |
| 4 | 01 | deviation | tests/meson.build |  | Rebased wrapped FVWM sources for tests subdirectory resolution | fixed |  | 2026-08-30T14:08:25.377Z | 2026-08-30T14:08:49.026Z |
| 5 | 01 | deviation | tests/live_x/live_x_harness.py |  | Corrected ordinary FVWM readiness after Status FIFO attachment | fixed |  | 2026-08-30T14:08:25.463Z | 2026-08-30T14:08:49.103Z |
| 6 | 02 | deviation | fvwm/fvwm3.c |  | Initialized the monitor scale configuration lifecycle so startup directives can mutate the production store | fixed |  | 2026-08-30T19:44:36.032Z | 2026-08-30T19:44:58.650Z |
| 7 | 04 | deviation | fvwm/virtual.c |  | checkPanFrames also converts mapped pan-frame thickness so X resizes keep scaled pixels | fixed |  | 2026-08-31T15:40:29.638Z | 2026-08-31T15:40:57.765Z |
| 8 | 04 | deviation | fvwm/move_resize.c |  | DoSnapWindow also converts snap proximity at owning-monitor factor | fixed |  | 2026-08-31T15:40:29.708Z | 2026-08-31T15:40:57.836Z |

````json
[
  {
    "id": 1,
    "kind": "deviation",
    "phase": "01",
    "file": "tests/run_in_linux.py",
    "line": null,
    "description": "Darwin verification stages source in a case-sensitive Podman volume instead of using the host bind as the working directory",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-30T12:36:38.136Z",
    "resolved_at": "2026-08-30T12:37:49.405Z"
  },
  {
    "id": 2,
    "kind": "deviation",
    "phase": "01",
    "file": "libs/monitor_topology.c",
    "line": null,
    "description": "Partial-allocation destruction required a NULL monitor-array guard",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-30T12:36:38.221Z",
    "resolved_at": "2026-08-30T12:37:49.484Z"
  },
  {
    "id": 3,
    "kind": "deviation",
    "phase": "01",
    "file": "tests/run_in_linux.py",
    "line": null,
    "description": "Reusable Darwin staging required stale-source cleanup before each copy",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-30T12:36:38.307Z",
    "resolved_at": "2026-08-30T12:37:49.565Z"
  },
  {
    "id": 4,
    "kind": "deviation",
    "phase": "01",
    "file": "tests/meson.build",
    "line": null,
    "description": "Rebased wrapped FVWM sources for tests subdirectory resolution",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-30T14:08:25.377Z",
    "resolved_at": "2026-08-30T14:08:49.026Z"
  },
  {
    "id": 5,
    "kind": "deviation",
    "phase": "01",
    "file": "tests/live_x/live_x_harness.py",
    "line": null,
    "description": "Corrected ordinary FVWM readiness after Status FIFO attachment",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-30T14:08:25.463Z",
    "resolved_at": "2026-08-30T14:08:49.103Z"
  },
  {
    "id": 6,
    "kind": "deviation",
    "phase": "02",
    "file": "fvwm/fvwm3.c",
    "line": null,
    "description": "Initialized the monitor scale configuration lifecycle so startup directives can mutate the production store",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-30T19:44:36.032Z",
    "resolved_at": "2026-08-30T19:44:58.650Z"
  },
  {
    "id": 7,
    "kind": "deviation",
    "phase": "04",
    "file": "fvwm/virtual.c",
    "line": null,
    "description": "checkPanFrames also converts mapped pan-frame thickness so X resizes keep scaled pixels",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-31T15:40:29.638Z",
    "resolved_at": "2026-08-31T15:40:57.765Z"
  },
  {
    "id": 8,
    "kind": "deviation",
    "phase": "04",
    "file": "fvwm/move_resize.c",
    "line": null,
    "description": "DoSnapWindow also converts snap proximity at owning-monitor factor",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-08-31T15:40:29.708Z",
    "resolved_at": "2026-08-31T15:40:57.836Z"
  }
]
````
