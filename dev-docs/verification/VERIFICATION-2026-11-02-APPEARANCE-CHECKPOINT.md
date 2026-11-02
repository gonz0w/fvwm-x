# A4 shared appearance checkpoint

THEME-02 #214 / ICON-01 #215: the accumulated A1–A3 sources now have a
focused native Linux acceptance result. The combined checkpoint also repairs
two reproduced product defects: complete settings replies exceeded their fixed
8 KiB budget, and unsolicited tray configuration could undo runtime visibility.
[Machine-readable evidence](VERIFICATION-2026-11-02-APPEARANCE-CHECKPOINT.json) ·
[Test design](TEST-DESIGN-2026-11-02-APPEARANCE-CHECKPOINT.md) ·
[Development captures](../../doc/development-preview.md#desktop-component-preferences-in-progress).

Base: merged PR348 `765191d7b3ba8e6d2f6936cb205f3a853ef83445`.
The JSON binds current changed source, reviewed plans, exact preparation/run
receipts, test and product ELFs, raw capture/configuration hashes and cleanup.
Owner artifacts remain under `builddir/codex-2026-11-02/r1-appearance-checkpoint`.
The report is a composed result; failed earlier attempts remain separate.

## Behavior and observed evidence

Complete settings query/validate replies retain all standalone fields, typed
values, explicit nulls and distinct diagnostics. Scoped group/metric defaults
and explicit value-inheritance version 1 remove repeated equal metadata/values;
shipped fixture decoders and primary manuals use the same precedence. The cap
remains 8192 bytes. The initial complete response was 8211 bytes; maximum valid
string/override cases reached 10668. The strengthened 64-case query/validate
matrix now measures 6292–7935 bytes and reconstructs each record exactly.

Ten affected Linux Go jobs pass: desktopsettings, desktopcontroller,
desktopcheck, desktopworker and xdgmenu, each ordinary and race. Eight unchanged
noncontroller jobs compose from native03; two corrected controller jobs compose
from native04. There are 644 passing top-level Go tests (1882 passing lines including
subtests) and zero failing lines in these accepted jobs; neither is an assertion
count. Five
independent Python decoder examples also pass on Linux. Focused Darwin pure-Go
regressions established the reply-budget repair first; they are separate evidence.

Native06 passes the component fixture in Slate dark, Dune light and enlarged
Classic. Actual X geometry is identical for these sampled explicit component
preferences: compact Form 749×406 and short menu 236×120; spacious Form 749×498
and menu 236×184; strong border changes only the menu to 240×188; enlarged font
produces Form 1043×606 and menu 291×224. Controls remain contained. The independent
schedule Form retains its old geometry; reopening adopts spacing (height 127→129).
Restoring inherited choices restores the sampled profile baseline. The actual
product launcher is captured separately from the fixed short geometry probe.

Actual preview recovery reports not-submitted intent first; the final terminal
consumer notice precedes new launcher/tray publication. An externally changed
foreground causes restoration refusal: the notice is observed, new publication
stays blocked, and a reopened editor retains the old submitted tuple. This
qualifies the selected recovery/conflict behavior, not arbitrary command ordering.

Four native05 UI jobs pass and are not repeated at native06:

- Ordinary tray: changed padding proves completion while runtime Hide/Show
  survives preserving Refresh. Explicit Reload applies stored Off. Reload retains
  precedence in both overlapping request orders. Existing keyboard/focus,
  malformed-client, capacity, disconnect, save-set and fallback cases also pass.
- Acquisition positive: unchanged item/menu identities survive a held completed
  batch; zero old-path loads and one fresh-path load occur after the profile change.
- Deliberately weakened control: one old-path load and a distinct fresh-path load
  occur. This is the expected counterexample, not a product-positive pass.
  Both observers report zero errors/overflow and release/disarm their gates.
- Modern appearance: independent native desktop icon choices paint red→blue→
  hicolor while application icon preference and live identity stay unchanged.
  Three theme/font profiles plus missing-name and text fallback checks pass.

The tray correction makes automatic unsolicited snapshots preserve visibility
and retry policy. Startup, explicit Reload and M_SENDCONFIG keep their semantics.
No configuration migration, dependency, state-file schema or reply-cap change.

## Profile, failures and cleanup

Linux ARM64 runs with native CPU through the existing AppleHV/container profile:
Ubuntu24.04, GCC13.3.0, Go1.27.2, Meson1.7.0, PNG/SVG/session D-Bus enabled,
NLS/docs disabled. Native C is uninstrumented; Go race is a distinct execution.
The owned Meson build initially compiled 21 selected native targets and ten Go
ELFs. Continuations recompiled only affected binaries; native06 rebuilt nothing.
There were zero installs, new packages, disk expansions or platform-matrix runs.

| Retained attempt | Actual outcome and disposition |
| --- | --- |
| native01 | Configuration passed; ambiguous `fvwm3` target stopped compilation. Use executable selector. |
| native02 | Products/test ELFs built; initial settings job exposed stale fixtures and incorrect package working directory. No UI run. |
| native03 | Eight noncontroller jobs pass; controller ordinary/race each have six failures. Correct scope/count/recovery oracles; reproduce the actual reply overflow separately. |
| host-policy01–05 | Retain compiler file-limit failure, measured overflow and intermediate insufficient compaction. Final 10 Go/5 Python cases pass with process groups stopped. |
| native04 | Controller ordinary/race and decoder pass. UI jobs fail on unreadable generated header, long private socket path and actual tray visibility regression. |
| native05 | Four tray jobs pass; component conflict waits on background after changing foreground. Native subscriber proves foreground publication; fix only the tuple index. |
| native06 | One corrected component job passes all three profiles using unchanged ELFs. |

All six owned appearance attempt labs have independently confirmed container and
machine shutdown. The metadata-only inventory lab also stopped. Runtime fixtures
reap owned children; acquisition gates are released/disarmed. Another owner's
QEMU is preserved. No failed run is relabeled as passing.

## Captures, performance and remaining acceptance

Separate product-role inspection covers the six native06 enlarged Forms/product
menus: visible text and controls fit without obvious clipping. Lead inspection
composes. Raw selected PNGs and exact configuration recipes are published with a
[capture manifest](../../doc/screenshots/desktop-components-manifest.json).
The three earlier modern tray captures have a truncated popup-manager title;
the enlarged popup overlaps part of another window's title bar. Their visible
body text is legible; this is not a clean full-desktop/accessibility claim.

The same-profile FvwmDesktop debug executable grows from 6,753,568 to 6,756,160
bytes (+2592). This is file size, not resident memory or a timing result.
Performance reaches 5/5 on this production-fix merge. The prepared supplemental
codec benchmark did not execute: quiet preflight found another owner's QEMU.
The general comparison remains due; retain the PR341 property-core reference.
No speed or RSS improvement is asserted and no new VM boot is selected solely
for the counter.

Full six-target/compiler/install/default-server, native C sanitizer, physical
hardware, accessibility and release qualification remain open. Updated manuals
have source/link review; current Asciidoctor rendering is unavailable. Asset
choice, reference-safe disk-cache retirement, file-change invalidation and broader
panel roles remain finite #214/#215 acceptance. Neither owner issue closes here.
C2–C5 clipboard acceptance is separate; capture remains off pending a qualified
lock source. Separate integrated and exact signed-delivery reviews follow.
