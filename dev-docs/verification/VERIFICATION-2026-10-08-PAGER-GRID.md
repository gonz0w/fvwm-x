# Pager grid and scale preparation — 2026-10-08

Status: implemented; focused checks passed; full qualification pending.
Owner: module ownership/construction and numeric UI follow-up in PROJECT-TRACKER.

Three stories follow the shared numeric-parser work:

- Prepare Rows/Columns with division and remainder instead of a signed grid
  product. Preserve default/zero handling, representable decimal-prefix grammar
  and large empty grids; no additional configuration limit is introduced.
- Traverse displayed desks when shaping and reconfiguring windows. Work scales
  with `ndesks`, rather than all Rows × Columns cells. Extract the shape owner
  into `pager_shape.c`; its C control invokes the real owner while recording the
  X request. It verifies ordinary labels/monitor selection and a one-desk grid
  with both grid dimensions at INT_MAX.
- Prepare DesktopScale as a positive representable integer before publishing to
  unsigned storage. Zero, negative, malformed and overflowing input keeps the
  previous scale. This removes a mismatched scanf destination and zero divisor.

Source archive SHA256:
`6e017b4e8679810dff65f58c221ef21f5715d8f9c9cdb14986abea88fcd682e0`.
The frozen source contains 1407 files and changes twelve owning paths from parser
source9f17. No unrelated shared-workspace build blocks are included.

Normal/scoped Linux ARM64 passes eleven C and twenty-three actual X/core checks
per profile. FreeBSD/OpenBSD ARM64 each passes eleven C and twenty-one actual
checks. Complete 279/124/143 payloads validate every source hash/mode, exact
executed runner and outcome; native archives identify fourteen compiled
executables. Scoped C leaks are enabled; actual X-process leaks are disabled.
These are focused results, not the release native-Linux sanitizer result.

The real-X driver checks a 100 × 100 empty-cell grid with one displayed desk,
60000-pixel explicit geometry and ShapeLabels; the resulting X shape has one
rectangle. It also checks invalid scale and overflowing grid values, subsequent
extreme current-desk updates and normal EOF. No timing benchmark is claimed.
An initial extracted owner lacked the Module.h type declaration. The failing
Linux/BSD build logs, exact source and runners are retained under
`builddir/codex-2026-10-08/pager-grid-production/initial-missing-packet-header`.
Adding the declaration resolved compilation; all final focused checks pass.

The three parser stories and these three grid stories share one undispatched
six-story checkpoint, fourteen paths from sourcea653. There is no separate9f17
full matrix. Required combined scope is 1800 standard entries/eight profiles,
120 installed identities, 288 BSD default-X driver invocations, optional230/210
and scoped116 (40 C leak-enabled, 76 actual leak-disabled). Dispatch requires
strict qualification of the preceding combined startup/monitor sourcea653.

Other Pager derived-geometry arithmetic and nested drag waits remain open.
Native Linux release CI, physical monitors, toolkits and compositors have
separate coverage requirements.

Four related derived-dimension stories are now folded into this undispatched
parser/grid checkpoint. Final sourcece40 is strictly focused-qualified and shares
one ten-story1800/120/288 optional230/210 scoped116 full matrix after sourcea653.
There is no separate6e01 matrix. [Dimensions](VERIFICATION-2026-10-08-PAGER-DIMENSIONS.md).
