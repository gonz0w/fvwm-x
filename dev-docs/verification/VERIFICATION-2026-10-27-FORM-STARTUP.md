# Form startup and viewport continuity — 2026-10-27

Owners: #51/#69; Cam. Base: merged PR284,
`48eff197b9390901cf80ea7a49e789a4c3642e82`. This finite three-outcome
checkpoint passes focused Linux qualification. Both broad owners remain open.
[Machine-readable receipts](VERIFICATION-2026-10-27-FORM-STARTUP.json) retain
source, binaries, commands, archives and actual geometry.

## Result

- Reflow minimally reveals a focus that was already visible, preserving range
  anchor, selection and generation. Deliberate wheel scrolling can leave focus
  offscreen and reflow respects that choice.
- The first **ready** scaled view uses configured preferred rows/width and prepared
  control metrics. One-row legal minima remain separate. Later user resizing and
  monitor changes use actual geometry, without restoring startup preferences.
- Ordinary automatic startup measures the managed outer frame and requests at
  most one core relative-pixel Move when needed. Physical monitor bounds limit
  preferred size; readable minima take precedence. Oversized axes anchor at the
  physical top/left edge. Explicit Position/Geometry skip automatic relocation.
  Later user moves remain authoritative.

This is first-ready admission, not a claim that the first mapped frame already
has final scaled geometry. There is no new core placement engine, protocol,
toolkit, resident process, file action or Go behavior.

## Reproduction and source identity

The retained PR284 Form binary was verified as SHA256
`e60d861f4a791897011a3afcb285f0f723c079562ba58746b3c8e738dad5c93d`.
A fresh actual-X Slate1.25 run observed a four-row Details chooser at only
35px (one 27px row plus borders), where four rows require 116px. Classic2 observed
outer frame `[719,542,2300,1754]` on `[0,0,2560,2160]`: fitting dimensions,
but overhanging origin. These are product failures, preserved in the baseline
archive, not inferred from source alone. The implementer also observed the
visible-focus unit regression fail before the policy correction and pass after.

Final frozen inputs: 1,545 files, bytes/sizes/modes aggregate
`d4faa7429dec604873b850331bdfe796c611fd7e93190a85eb6d094f353a709e`.
Final native driver SHA256:
`b20eac228fdd3ceb3bfcacfdd34f1aaa80b555049d36a4fde64c92196991c760`.
The final guest recheck verifies those inputs, three drivers and all retained
normal binary identities after execution. The normal Form is 1,783,704 bytes,
SHA256 `de146884f65949d61c2e071d9c076d7fa19289ac11a0f06daec5cb1212fb53a6`.
Core and browser binary hashes are unchanged from PR284. The Form executable
is 17,544 bytes larger, including debug information; no RSS/speed improvement is
claimed. The PR281 performance reference is 1/5 after PR284, 2/5 on this merge;
no comparison is due.

## Combined checks

Reuse the owned Ubuntu 24.04.5 ARM64 container on the local Linux 6.18.10
ARM64 Podman VM and existing incremental GCC/ASan build profiles. This is actual
Xorg-dummy execution in a local VM, not physical/native-runner release acceptance.

The combined gate passed the affected Form build, four registered C policies
(form-list/input/ui-layout/scale), actual-X geometry/fields/read-only controls,
two scoped ASan/UBSan policies (list/scale, leak detection enabled), and the
changed Form manual render. Pure policy exercises preferred width above legal
minimum, overflow, physical caps and signed/nonzero bound origins.

A source inspection then moved independent pre-ready movement observation ahead
of monitor selection, so a detected cross-monitor move chooses the actual monitor
in that retry. Only the Form build and clarified manual repeat. Initial source
aggregate `f31a82a333b71fb8036a0e6f6b8ffc8b7ca5fa157a6c6d8475626903c02bf926`
and initial binaries/receipts remain separate. Unchanged pure policies and
UIScale-disabled legacy controls retain their successful evidence. Final native
startup cases run against the corrected Form. No Go/race, full OS/install matrix
or full instrumented Form suite repeats.

| Theme | Actual scale | Row height | First-ready list height | Legal one-row list height | Captures | Module Wait0 |
| --- | --- | --- | --- | --- | --- | --- |
| Slate dark |1.25|27|116|35|8|28|
| Dune light |1|21|90|27|8|28|
| Enlarged Classic 18pt |2|68|284|80|8|28|

All three first-ready Details lists show four complete configured rows. Native
width checks verify complete actual control extents; these scenes' widths equal
their control minima. Preferred width above minimum has pure policy coverage,
not a separate native wide-list regression claim.

Native cases also retain input cursor/draft (`drxaft` after inserting at index 2),
selection/focus packets, generations/resources and child XIDs through shrinking.
A last-row hit proves initially visible focus 7; after one-row shrink, first-row
hit still identifies 7. Actual wheel paint changes before reflow, then first-row
hit identifies 10 while focus 7/selection remain unchanged until that click.
Intentional core `Move 100p 120p` after automatic-placement captures remains at that
origin through later resize. Positive Position and Geometry match unscaled twins
and retain their origins after resize. Existing complete-value reconstruction,
read-only raster restoration, stale callbacks, snapshots and parent replacement
continue to pass. Numeric XIDs may be recycled; lifetime checks use native aliases
and actual child waits rather than numeric inequality.

There are 24 captures, 84 actual module Wait0 receipts and 12 direct zero exits;
no unexpected observer X errors, failure or cleanup entry remains in final runs.
The first final fixture attempt failed only its test-owned Form teardown: literal
`Escape` is not a supported button shortcut. It is preserved; changing that
fixture to F11 repeats only Slate native execution, with unchanged product bytes.
Dune and Classic each run once on the final fixture. This is distinct from the
two pre-fix product failures.

## Screenshots, review and limits

The [three unmodified published images](../../doc/development-preview.md#scaled-form-startup)
show the first-ready four-row chooser at all three scales. Lead inspected those
three; separate review inspected nine further shrink/minimum/wheel images and
verified all 24 PNG hashes. The native manifest records actual client/frame bounds
at capture time; no fixture placement fallback remains. Classic's main frame is
`[260,0,2300,2160]`, inside the physical screen. The panel can overlap its title
at the top edge: reserved work-area containment is **not qualified**.

Earlier user moves cannot be distinguished before the first managed-frame sample;
this is documented. Source/pure checks cover nonzero/signed geometry, but no new
nonzero-origin monitor, negative explicit placement, reserved-area, mixed-monitor
pre-ready user-move or oversized native fixture is claimed. Font/refusal paths
retain source inspection and earlier qualified scope, not a fresh full fault
matrix. Full shipped-consumer/six-target, multilingual, physical, assistive and
release acceptance remain under #51/#69. #257/#258 stay parked.

Separate read-only Sol/high runtime and integrated evidence review reports no
remaining actionable runtime finding; final document review is recorded with the
candidate. This is a second agent examination, not human or release approval.
Workflow Luna/medium, analyst Astra/high and implementer/reviewer Sol/high explicit
spawn settings were accepted. Native named-role loading is unavailable; backend
identity is unattested. No fallback model was silently selected.

## Evidence and resource cleanup

Canonical archives live in the owning host ledger
`builddir/codex-2026-10-27/r1-form-startup/`; their full payload sizes, hashes,
modes, member sets and gzip CRCs were verified before duplicate retirement.

| Archive | Files | SHA256 |
| --- | --- | --- |
| baseline-evidence |54|`d2e2b1d5d216ee9c231b0651f38ad45b2be38b15a737d5650bd32d0fbfca02ff`|
| native-first-attempt |37|`2d5c555d42ef6761de1f3ffb177e3d8ae024b1764311b01c5136a831b64dcce7`|
| native-final |339|`058600dabff46e84749d1cb55c166a48a1a6a7834d4146b72f1c46f68b1259af`|
| legacy-final |35|`73ab1d40c891cdf44a809f704ce766ecda2c945d3debf5953ef619259c10b8b0`|
| checkpoint-final |27|`5a09f71e012e52160694cca40c57e4527711a3b19f584f7e6681845b5045b140`|

Only owned verified guest evidence duplicates retire. Source/build/install stages,
caches, VM disks and performance references remain. The owned container and Linux
VM stop before the controller is released; host receipts record actual completion.
