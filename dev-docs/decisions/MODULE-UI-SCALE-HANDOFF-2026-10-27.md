# Module UI scale metadata handoff — 2026-10-27

Owner: Cam. This is the initial foundation checkpoint for
[MODULE-SCALE-01 #51](https://github.com/gonz0w/fvwm-x/issues/51), coordinated
with [FILE-VIEW-01 #69](https://github.com/gonz0w/fvwm-x/issues/69). The
[release contracts](RELEASE-CONTRACTS-2026-10-26.json) and
[tracker](../PROJECT-TRACKER.md) retain their full acceptance and status.

## Accepted checkpoint and boundary

The lead accepted an additive, core-owned, versioned `M_CONFIG_INFO` monitor UI
snapshot carrying the committed topology epoch, effective quarter-scale factor,
monitor identity and actual-pixel geometry. A strict bounded consumer library
must publish complete snapshots atomically; an actual native module probe is part
of this checkpoint's acceptance. Existing `Monitor` records and configuration
remain unchanged. Native packet types and installed Perl wire metadata do not
change. No new resident process, dependency or live `MonitorScale` reload is
included.

Form and browser rendering adoption is the next dependent batch. This foundation
alone does not establish automatic UI scaling, screenshots, six-target coverage
or closure of #51 or #69. Client, X11 and module geometry remains actual pixels;
scaling applies to FVWM-owned UI. The existing core owns scale and topology, not
a parallel module authority.

## Snapshot contract

The frozen [consumer header](../../libs/module_ui_scale.h) specifies these text
records inside the existing `M_CONFIG_INFO` channel:

```text
MonitorUI 1 BEGIN EPOCH COUNT ROOT_FALLBACK
MonitorUI 1 ROW EPOCH INDEX ATOM QUARTERS PRIMARY X Y WIDTH HEIGHT NAME_HEX
MonitorUI 1 END EPOCH COUNT
```

The epoch identifies the committed topology; scale configuration is initial-only,
and effective factors are recomputed for committed RandR topology changes. The
epoch is exactly 16 lowercase hexadecimal digits and nonzero. Count is
1–64; row indices are sequential and zero-based. The atom is nonzero and
32-bit, quarters are 4–16, and boolean fields are 0 or 1. Rectangles use
checked actual root pixels. Names contain 1–256 non-NUL bytes encoded in hex;
the emitter uses lowercase and the parser accepts either case. Numeric fields
other than the epoch and name encoding are strict decimals. Work areas remain
in the existing record.

The API owns snapshot, receiver and formatted bundle state. Validation and
formatting reject invalid data; a failed format supplies no partial bundle.
A zero-initialized receiver stages rows and publishes only a complete end record.
Its statuses distinguish ignored, staged, published, unchanged and refused
records. Exact same-epoch replay is unchanged; conflicting replay is refused.
Refusal, loss or abort preserves the last good publication. If the core cannot
prepare a valid complete bundle, it emits no partial `MonitorUI` snapshot and
continues the existing `Monitor` records. Selection returns a
borrowed row and reuses existing monitor ownership and hysteresis. [Focused Linux results](../verification/VERIFICATION-2026-10-27-MODULE-UI-SCALE.md)
qualify this foundation; broader consumer adoption remains pending.

## Existing shipped consumer inventory

The top-level [Meson module list](../../meson.build) installs the named modules;
[Perl metadata](../../perllib/meson.build) remains installed. This is a source
inventory and handoff, not evidence of release qualification. Each row still
needs its applicable #51 evidence or explicit release disposition.

The core's existing [`Monitor` producer](../../fvwm/module_interface.c) emits a
name followed by 14 integers in this order: flags, page counts x/y, viewport x/y,
viewport maxima x/y, current desk, combined screen width/height, and work-area
insets left/right/top/bottom. The existing record has no effective UI scale.
Page counts and desk/flags are not pixel values; viewport, extents and insets
represent actual geometry. The record and its consumers remain unchanged here.

| Shipped consumer | Existing input and boundary | Handoff disposition |
| --- | --- | --- |
| [FvwmPager](../../modules/FvwmPager/messages.c) | Parses all 14 `Monitor` values; uses flags, pages, viewport, desk and screen extents. Its [`DesktopScale`](../../modules/FvwmPager/init_pager.c) is a separate positive projection divisor, with actual-pixel source geometry. | Preserve its existing projection and configuration; future scale adoption needs its own evidence. |
| [FvwmRearrange](../../modules/FvwmRearrange/FvwmRearrange.c) | Reads the 14 values, desk and work-area insets to prepare an actual-pixel placement box. | Preserve actual-pixel placement. |
| [FvwmIconMan](../../modules/FvwmIconMan/fvwm.c) | `ConfigWin` monitor ID and frame geometry identify and place managed windows in actual pixels. | Preserve packet geometry; its own UI remains a future consumer decision. |
| [FvwmIdent](../../modules/FvwmIdent/FvwmIdent.c) | `ConfigWin` monitor ID and frame geometry describe the target in actual pixels. | Preserve displayed target geometry. |
| [FvwmMFL](../../modules/FvwmMFL/request.c) | The core-current-monitor desktop query labels viewport, page and work-area units as actual pixels; page and desk indices remain indices. | Preserve query units. |
| [FvwmButtons](../../modules/FvwmButtons/FvwmButtons.c), [FvwmForm](../../modules/FvwmForm/FvwmForm.c), [FvwmScript](../../modules/FvwmScript/FvwmScript.c) | Receive configuration but have no existing `Monitor` record consumer. | Form/browser rendering adoption follows separately; do not infer existing scale behavior. |
| [FvwmAnimate](../../modules/FvwmAnimate/FvwmAnimate.c) | Uses icon and frame packet geometry for animation in actual pixels. | Preserve geometry; release disposition remains open. |
| [FvwmAuto](../../modules/FvwmAuto/FvwmAuto.c), [FvwmEvent](../../modules/FvwmEvent/FvwmEvent.c) | Event/command handling; Event recognizes monitor events, rather than parsing `Monitor` geometry. | Preserve event semantics; release disposition remains open. |
| [FvwmPerl](../../modules/FvwmPerl/FvwmPerl.in) and [perllib](../../perllib/meson.build) | Installed command/event metadata includes scale commands; no `Monitor` record parser was found in `perllib/FVWM`. | Preserve installed metadata and packet ABI. |

FvwmBacker is owned by parked #258 and outside this checkpoint; #257 System is
also outside it. No new claim is made about either owner's acceptance.

## Verification and remaining work

The [focused Linux checkpoint](../verification/VERIFICATION-2026-10-27-MODULE-UI-SCALE.md)
qualifies bounded and atomic native snapshot handling, effective factors, real
monitor movement/removal, replay and restart. Rendering adoption, live scale
configuration reload, relevant six-target consumer coverage and the full #51
manifest dispositions remain outstanding. #69 retains
its own scale, accessibility, screenshot and support-profile criteria. The
inventory is based on source inspection; the linked report separately records
the lead-owned native execution. No rendering or screenshot result is inferred.
