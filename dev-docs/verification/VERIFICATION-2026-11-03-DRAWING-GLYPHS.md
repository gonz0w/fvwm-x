# Shared text drawing and optional menu glyphs

G1 consolidates the paired GC/Xft clipping lifetime used by Form fixed labels
and Buttons titles. A7 adds Cam's accepted optional 16-glyph Lucide menu theme,
complete ISC/MIT notice, reversible source provenance and a labeled example.
The scoped Linux checkpoint is accepted from source-bound independent passes
and diagnosed focused repairs. Original failures remain recorded.

[Exact results and receipts](VERIFICATION-2026-11-03-DRAWING-GLYPHS.json) ·
[Asset decision](../decisions/ICON-ASSET-CHOICE-2026-11-02.md) ·
[Core comparison](PERFORMANCE-2026-11-02-DRAWING-CORE.md) ·
[Raw captures and recipes](../../doc/screenshots/lucide-menu-manifest.json).
Base: merged PR351 `aa4792c665622bb222b3939f7354f485b54bf05e`.

## Result and ownership

`flocale_draw_string_clipped` borrows the font/string/GC synchronously and owns
one temporary Region. It applies both clip paths, draws, clears its GC clip,
restores descriptor clip fields and retires the Region. The caller supplies a
prepared rectangle and a GC without persistent clipping. Empty rectangles do
nothing; Region creation failure returns False without changing caller state.
Submission status does not acknowledge asynchronous X-server errors. Geometry,
background clearing, temporary text, font generations and image/death-row owners
stay in the two callers. Existing shared relief/image/alpha owners remain.

`-Dlucide-menu-icons=true` is disabled by default and independent of SVG support.
Its tagged private install stage has exactly **20 files / 19,201 content bytes**,
below the 32 KiB planning cap; default-off introspection contains no assets or
example. These are measured file contents at the recorded prefix, not allocated
filesystem/package bytes or memory. SVG rendering uses the existing optional
worker. There is no new resident, renderer, toolkit or automatic activation.

The example uses private Colorsets 240–243 and a named menu style. Setup snapshots
the desktop palette; reapply it after palette changes. All sixteen catalog rows
are Nop demonstrations; the heading and unavailable row exercise other states.
Application artwork, tray/pinboard and Vector controls keep their own policies.
Source parity reverses only the explicit black stroke replacement and verifies
all pinned upstream hashes, geometry and complete notice. Named lookup exercises
the actual theme under a custom XDG prefix at all thirteen quarter steps and
normal/selected/disabled request states: 624 lookups per ordinary/race run.

## Composed verification

Affected G1 core/Form/Buttons/probe compilation took 8.91 seconds including a
needed Meson target-metadata refresh. A7's three menu/browser Go consumers,
resolver ordinary/race test ELFs, configuration and tagged staging took 5.83
seconds. Native C is uninstrumented Linux ARM64 GCC under AppleHV; PNG/SVG and
session D-Bus are enabled, NLS/docs disabled. All owned containers/machines were
confirmed stopped after each bounded run. No system install or full OS matrix ran.

- Iconrender and pinboard-owner ordinary/race runs pass: 68 top-level test PASS
  lines plus 42 subtest PASS lines. Resolver package coverage is composed from
  unchanged ordinary/race ELFs: 43 top-level passes and 79 subtest passes each,
  then the one failed boundary case passes in each focused retry. Counts are
  cases/subtests, not assertion totals.
- Real-X fontset and Xft clipping, negative/narrow rectangles, unclipped reuse,
  descriptor restoration, empty no-op, allocation failure and zero X errors pass.
  The Render-disabled variant passes with XFontSet. Separate XFontStruct-only
  execution was not observed.
- Both matched layout workloads pass in Slate, Dune and enlarged Classic:
  eleven bar leaves cover the full width, action/keyboard identities survive,
  Form/browser content remains bounded through resize, and idle/CPU/RSS/latency
  observations are retained. The control substitutes pre-G1 core/Form/Buttons;
  other helpers are shared. This is an isolated drawing comparison, not an
  entire historical desktop comparison. Both sides use the corrected same fixture.
- The accepted A5 source-refresh positive/control jobs pass. The positive refuses
  old exact paths, admits changed content and preserves item/menu identities;
  the weakened control admits old and fresh paths. This witnesses loader behavior,
  not visible stale-pixel absence.
- Final menus have 48 named per-glyph readiness observations across three profiles.
  Actual adjacent glyph foreground/background/intermediate pixels witness normal,
  selected, disabled and heading roles. Observed role contrast exceeds 4.5:1;
  Home/Return complete native selection and Nop dismissal. An unrelated image
  retains all 2,304 original green pixels. Inspected raw captures show all sixteen
  glyphs and legible labels without obvious clipping in all three profiles.
- Six alternating periodic core runs complete. Command-median ranges overlap;
  no causal speed gain is established. Core text grows 392 bytes; data/BSS are
  unchanged. The separate query-projection cost report remains applicable.

## Diagnosed failures and limits

The first holder expired safely during a user pause before dispatch. The next
build stopped before compiler work because Meson had not regenerated its new
fixture target list. Both receipts remain. Initial combined runtime completed
all fifteen jobs, with eight independent passes and seven failures. Repairs
reused accepted results rather than repeating the checkpoint.

The existing sparse-file boundary test needed a logical length above 128 MiB;
its inherited 16 MiB file limit prevented setup. Only that exact test's two child
soft limits were raised for retries. The font oracle wrongly required an
XFontStruct instead of accepting the loader's XFontSet; actual backend reporting
now distinguishes fontset/core-font/Xft while preserving every drawing assertion.
The old layout fixture queried ten of eleven configured leaves. The example's
Greyed syntax was corrected. A private build-tree Home binding was required,
matching ConfigFvwmDefaults. Visual review then rejected partial asynchronous
captures despite passing role witnesses; the final fixture verifies every glyph
before capture using the existing deferred-image commit boundary. Earlier partial
captures remain private and excluded from publication.

Full six-target/compiler/install/default-server, C sanitizer, physical/compositor,
mixed-monitor and remaining Buttons resource qualification stay open. Logical
quarter-step lookup is not actual quarter-scale rendering. SVG-disabled native
menu behavior and full optical/accessibility acceptance remain queued. This is
not WCAG certification or whole-desktop/hardware performance acceptance.
#210/#214/#215 remain open under their frozen criteria; #211/#212 comparisons
and parked Daybreak work receive no implicit adoption or execution.
