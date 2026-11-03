# A5 explicit icon refresh and lifetime boundaries

ICON-01 #215 gains explicit same-profile tray source refresh. Refresh/Reload
keeps one coalesced intent until the last pending configuration snapshot commits.
Older completed loads and new requests wait behind that intent. With a valid
model/extent, the existing nonwrapping acquisition fence and cell/menu reset run
before result admission. Automatic snapshots retain their cheap unchanged-source
behavior. Refresh keeps visibility/retry policy; Reload remains failure recovery.
Retirement failure and acquisition exhaustion remain blocking.

[Exact source/build evidence](VERIFICATION-2026-11-02-ICON-LIFECYCLE.json) ·
[Ownership contract](../decisions/NAMED-MENU-ICONS-2026-10-26.md#a5-explicit-refresh-and-artifact-ownership--2026-11-02) ·
[Supplemental performance observation](PERFORMANCE-2026-11-02-SETTINGS-PROJECTION.md).
Base: PR349 `d2624fb79d0e7634054a6c134a1f31602a76a8c1`.

## Compilation and authored acceptance

One reviewed build-only continuation reuses the owned native Linux ARM64
PNG/SVG/session-D-Bus profile, SDK/cache and existing build. Three tray variants
link; iconrender and pinboard-owner ordinary/race test ELFs compile (four ELFs).
Six tooling commands take 3.96 seconds overall, including 1.63 seconds for tray
compilation/linking. Native C is uninstrumented; NLS/docs are disabled. There are
zero native product/unit/UI invocations, installs or new packages in this lab.
Container and machine shutdown are independently confirmed; both sessions were
consumed. Previous tray ELFs remain as relevant controls for the next checkpoint.

The two changed production C units, private UI header and optional observer are
compiled through the normal Meson targets. The tray executable changes from
1,918,848 to 1,917,968 file bytes (−880); debug executable file size does not
establish RSS or speed. No new resident, dependency, watcher, cache engine or
configuration migration is introduced.

Initial cases were authored before affected implementation, then tightened by
separate source analysis. The pending runtime set includes:

- Existing same-path PNG/SVG/XPM replacement and new missing-theme/inheritance
  cases: repeated cached requests retain prior results, a fresh acquisition sees
  changed bytes/metadata, and older exported assets remain byte-exact.
- A private held menu batch under two explicit Refresh requests with the same
  tuple: final configuration completion and a native sync-return witness precede
  release; item/menu identities survive. The positive refuses old full paths;
  a fresh content-derived basename must change. A fresh private directory alone
  is insufficient. The weakened control must admit old content and reach fresh
  content. Held files remain unchanged before release and retire after consumption.
- Pinboard shutdown with no possible native acknowledgement retains active,
  pending and queued-obsolete image generations. Existing owned/bounded retirement
  policies and acknowledged-generation cases remain composed dependencies.

The sync witness establishes a returned sync after final config completion; it
is not an exact acquisition-generation receipt. Tests, private controls and
primary manuals share the selected contract. Source/plan review is clear, and
Python syntax, the exact control predicate and repository/whitespace checks pass.
Final integrated and exact signed-delivery review follow.

## Retained gaps and continuation

A5–A8 is the next combined Linux runtime/race/theme checkpoint. Authored cases
and linked ELFs are not passing executions. No UI surface was added here; the
A4 three-profile captures retain their own source identities. Their public
harness-recipe guidance is clarified. Full affected support/compiler/install/
default-server, native C sanitizer and release gates remain separate.

Exported application/directory menus retain absolute asset paths after generator
exit; existing pictures can load again at another scale, and menu files can be
read/copied later. No last-reference proof exists for automatic shared deletion.
Private tray batches and pinboard generations already own narrower safe cleanup
boundaries. Preserve those rather than introducing a collector. Whole #215
export lifetime, explicit asset choice and wider #214/#215 acceptance remain open.

The supplemental host settings-projection observation is six separate benchmark
processes, with all process groups stopped. It records the A4 codec's higher
query/allocation cost and smaller wire reply honestly; it is not a new native-X
qualification. General performance remains due (five checkpoints before this
merge, six afterwards). #29/#226 retain release-budget ownership.
