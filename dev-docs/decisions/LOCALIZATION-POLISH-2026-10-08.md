# Complete language support as late roadmap polish

Status: **scope and late scheduling requested on 2026-10-08; implementation
queued, tooling recommendations proposed**. The user requested translated man
pages, complete use of existing catalog languages and translated configuration
comments after everything else. No service, dependency or translation has been
adopted by this record.

Owner: I18N-02, coordinated with I18N-01, DOCS-01, TEXT-01, XDG-01, CFG-01,
THEME-01, A11Y-01, MAINT-02 and TOOLING-GO-01. The
[tracker execution order](../PROJECT-TRACKER.md#execution-order) is authoritative.

## Placement and scope

Take this up as polish after the existing implementation and qualification
program, stages 1 through 8 when applicable. RELEASE-01 remains conditional on
the user's request; this adds no prerequisite to the existing release gate and
does not authorize publication. Deferred future desktop exploration remains
separate. Existing catalog regeneration/parity work under I18N-01 and text/input
foundation work under TEXT-01 retain their earlier owning checkpoints.

Target all eleven current catalog locales: Arabic (`ar`), Danish (`da`), German
(`de`), Spanish (`es`), French (`fr`), Georgian (`ka`), Russian (`ru`), Swedish
(`sv_SE`), Ukrainian (`uk`), Simplified Chinese (`zh_CN`) and Traditional Chinese
(`zh_TW`). Reconcile the language manifest, actual catalogs and legacy tooling;
catalog existence is not evidence of usable or complete support.

Cover all first-party installed user-facing menus, dialogs, modules, desktop
helpers, onboarding, help, relevant diagnostics, shipped configuration comments,
and manuals for the features still shipped at that checkpoint. Optional HTML
comes from the same translated manual source. Developer documents, C comments,
third-party applications and retired examples are outside this bounded scope.

## Recommended translation architecture

Keep reviewed English source and versioned UTF-8 PO catalogs in Git. Use separate
components for core/module UI, desktop helpers, manuals and configuration
commentary, with a shared glossary and translation memory. Preserve existing
translations and flag changed or ambiguous entries for review. Extract complete
messages with context, plural forms and named/typed placeholders where the
consumer permits them; avoid assembling sentences from translated fragments.
Audit the C/Go/config/Script consumers after planned runtime migrations before
choosing adapters. Extend existing gettext support where suitable; a wholesale
runtime localization-library replacement is not implied.

Use Weblate as the leading contributor-workflow candidate: browser editing,
language reviewers, glossary, translation memory, screenshots and context, plus
reviewable machine/LLM drafts. Catalogs remain usable with offline editors and
ordinary contributions. Build, install and desktop operation must not require
Weblate, a network connection or an AI service. Hosting, credentials, costs and
external integration are later owner decisions.

Give AI drafts the glossary, surrounding paragraphs, screenshots and an explicit
list of immutable syntax. Validate tokens before accepting a draft and require
fluent-language review for a completion claim. Record draft/review provenance;
back-translation or a second model can find problems but cannot establish
linguistic correctness. Keep unfinished/fuzzy drafts out of qualified artifacts.
With Weblate, review status alone is insufficient: configure the commit quality
filter to export only approved translations when using a reviewed workflow.

## Manuals

Prototype extraction from canonical AsciiDoc into PO, reinsertion into generated
localized AsciiDoc, then rendering through the existing manual manifest/helper.
Translate source prose once for both man and optional HTML. Include tables,
cross-references, shared includes and the split commands/menus/styles pages in
the prototype so translation of the main manual is reused consistently.

po4a is the leading reference candidate: its maintained formats include AsciiDoc
and man. It is Perl-based, so evaluate it as optional maintainer/reference
tooling against the approved TOOLING-GO-01 direction; it must not reintroduce a
Perl desktop runtime or become an unreviewed mandatory build dependency. Qualify
its behavior on this repository's AsciiDoc dialect before adoption. A Go-based
alternative needs demonstrated extraction/reinsertion parity, not an assumed
easy parser rewrite. Weblate's direct AsciiDoc format is another candidate, but
its documented immature support and service-owned translation state make PO
round-tripping the preferred workflow to evaluate first.

Keep command names, options, paths, environment variables and working examples
literal; translate explanatory prose and explicitly designated display text.
Stage manuals under platform-appropriate localized man directories, commonly
`share/man/<locale>/man1`, with English fallback. Verify actual lookup and locale
aliases on Linux, FreeBSD and OpenBSD rather than assuming identical behavior.
Document language selection and English recovery. Reading installed manuals is
offline; decide regenerated versus distributed artifacts with build/release
owners through reproducibility and package gates.

## Configuration comments and labels

Generate localized commented copies/reference views from the single canonical
configuration plus comment catalogs. Use stable IDs and source hashes for
comment blocks so changes invalidate stale translations. Prefer an explicit
localized sample/export action and visible source version over rewriting an
existing personal configuration or automatically replacing it on locale change.

Only translate parsed comment prose, not text after every `#`: FVWM includes
color literals and quoted content. Preserve directives, functions, variables,
bindings, quoting, continuation rules, paths and executable configuration bytes.
Compare parsed command streams and behavior against the canonical file. Runtime
menu/button labels are a separate catalog consumer; translate their display
strings while preserving action identities and valid mnemonic bindings.

## Completion and maintenance gates

Start with a representative pilot: a desktop menu/dialog, a Script/Form consumer,
a manual with includes/tables/examples and a commented config sample. Test a
longer-text locale, Arabic RTL/mixed command text, Georgian, and Chinese rendering
and input. Then batch the remaining components and languages at coherent
acceptance checkpoints; do not run an entire platform matrix per string edit.

- Inventory every current user-facing source and installed manual/config sample.
  Publish per-language, per-component totals for reviewed current translations,
  missing/fuzzy/stale entries and actual functional checks. The final goal for
  each listed locale is full reviewed coverage of this inventory; catalog
  percentages alone do not establish completion.
- Check syntax, format placeholders, plurals, markup, references and mnemonic
  collisions automatically. Use synthetic accented/expanded and RTL test text
  to expose untranslated strings, truncation and layout assumptions early.
- Exercise actual X11 UI for wrapping, sizing, focus, navigation, mixed-direction
  text, font coverage and TEXT-01 input-method/composed-character behavior.
  Translation alone cannot qualify shaping, editing or language usability.
- Render and read actual localized roff/HTML, check examples, non-ASCII text and
  narrow layouts, and stage locale/default/minimal installs on the six supported
  targets. Test absent locales/catalogs and English fallback explicitly.
- Regenerate deterministically from recorded source/catalog/tool versions,
  verify clean/offline packaging and record focused versus full qualification.
  Make language claims only for reviewed, tested component/profile coverage.

After adoption, source changes automatically refresh catalogs and produce a
review queue and coverage report in normal CI. Reuse approved translations;
review changed segments and their context rather than retranslating whole books.
Do not set up a recurring service or background automation during roadmap work.

## Research and outstanding decisions

Primary references consulted on 2026-10-08:

- [po4a formats and maintenance model](https://www.po4a.org/).
- [Weblate translation/review/LLM workflows](https://docs.weblate.org/en/latest/workflows.html).
- [Weblate direct AsciiDoc support and limitations](https://docs.weblate.org/en/latest/formats/asciidoc.html).

Before implementation, select the versioned extraction tool and its dependency
contract, service hosting if wanted, reviewers for each locale, install/export
selection, and artifact policy. These choices remain proposed. This record
establishes the requested scope and sequence, not completed translations or
passing localization evidence.
