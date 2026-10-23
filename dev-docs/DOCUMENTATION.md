# Documentation and manual policy

Man pages are the primary OS-installed documentation. Keep AsciiDoc in `doc/`
as the reviewed source for both man and optional HTML output. This prevents
drift between two independently maintained manuals and uses the already adopted
manifest/renderer; it adds no desktop runtime package.

Complete multilingual UI/manual/config-comment coverage is later polish under
I18N-02, after the existing program. English AsciiDoc remains canonical; evaluate
versioned PO extraction/reinsertion for localized man and derived HTML output.
Tool/dependency adoption and localized install behavior need their own gates.
See the [requested scope](decisions/LOCALIZATION-POLISH-2026-10-08.md); current
DOCS-01 manual-quality work retains its placement.

## Developer document structure

- Stable guides: README, PROJECT, DEVELOPERS, ARCHITECTURE, WORKFLOW,
  DOCUMENTATION, STANDARDS, THEME-STANDARDS, SUPPORT/BSD and current
  security/package guides.
- PROJECT-TRACKER owns status and order; detailed cleanup/optimization roadmaps
  expand acceptance rather than create separate queues.
- `decisions/` stores proposals and accepted contract decisions with explicit
  status, owning tracker ID and compatibility implications.
- `verification/` stores dated scope, results, failures and limitations.
- `history/` preserves retired planning and inherited upstream notes. These are
  discoverable history, not current workflow instructions.

## Man-page quality

Each installed command/module needs a useful NAME and SYNOPSIS, understandable
behavior, exact options/defaults and relevant examples. Document configuration,
environment and file locations, exit/errors, platform capabilities and security
limits where applicable. Use consistent terminology and useful SEE ALSO links.
Keep configuration-referenced executable/module entry points usable; update
packaging and manual manifests together when other names change.

Verify examples against supported commands. Check long options, literal paths,
tables, narrow terminal wrapping, non-ASCII text and references in actual roff
output. Render relevant pages with mandoc on BSD and appropriate Linux man
tooling; classify warnings by cause rather than claiming that every inherited
formatter diagnostic is a defect. Stage the package and verify manual coverage,
destinations and optional-component inclusion. No network asset may be necessary
to read installed documentation.

The requested manual rewrite and plain technical language belong to DOCS-01.
ASD-STE100 compliance requires a selected issue/dictionary and explicit review;
AI-written prose alone is not certification. Website visual design remains a
separate later task and does not block man-page improvement.

## Build and export

Recommended OS install with manuals and without HTML:

```sh
./build -b builddir-man -Dhtmldoc=false -m -i
```

Direct Meson uses `-Dmandoc=true -Dhtmldoc=false`. Asciidoctor is a build-time
requirement for generation, not a runtime requirement to read installed pages.
Package/release builds should supply generated manuals; raising build defaults
or changing source-tarball contents belongs to the build/release owner and its
parity gate, not this developer-document migration.

Use the existing helper for a fresh review export:

```sh
python3 tools/maintenance/docs.py --output /tmp/fvwm-x-man-review --format man
python3 tools/maintenance/docs.py --output /tmp/fvwm-x-html-review --format html
```

Run from the repository root. Choose a new destination for each run; the helper
refuses an existing output directory. Add `--prompt` when qualifying the optional
Prompt manual. `--format both` generates both from the same input. Installed
documentation and optional-component parity are already maintained by Meson.

HTML remains optional (`htmldoc=false` by default), static and derived from the
same source. Require no JavaScript, external fonts/assets, site framework,
service or independent manual source. Embedded lightweight CSS and local links
are sufficient. Retain the public website helper's existing behavior until its
replacement is adopted through the owning compatibility gate.

## HTML and accessibility acceptance

Actual public HTML exports target applicable WCAG 2.2 AA criteria and conforming
semantic HTML/CSS. Use document language, logical heading hierarchy, useful link
text, readable code/tables, landmarks and text alternatives where needed. Prefer
native HTML semantics; add ARIA only when required. Validate generated markup
and review keyboard/focus, screen-reader reading order, contrast and enlarged/
narrow layouts in a real browser. Automated validators alone are insufficient.
Keep assets offline and lightweight. DOCS-01 owns export/template fixes; website
visual design remains deferred. Record tested export/source fingerprints and
remaining gaps using [STANDARDS.md](STANDARDS.md).

Man output retains its primary role and independent roff/content/install gates.
Native desktop UI follows [THEME-STANDARDS.md](THEME-STANDARDS.md), adapting W3C
guidance through WCAG2ICT. A good man page or accessible web export does not prove
native accessibility, and a readable native palette does not qualify HTML.

## Review evidence

The existing 28-page website-manual review output totals 1,434,135 bytes
(about 1.43 MB). An HTML-parser inspection found no script tags or external
asset requests. This measures that prior review output, not a full website or
all future HTML. The [tooling checkpoint](verification/VERIFICATION-2026-10-05-TOOLING.md)
already records actual Asciidoctor/man/HTML parity checks.

Fresh manual generation could not run on this macOS host during migration:
Asciidoctor is absent and Podman image access returned a storage `readlink`
error after socket access was permitted. Existing user manuals and build targets
were retained. DOCS-01 still requires real renders/lint and content review;
this migration is not evidence of a completed manual rewrite or new passing
man-package gate.

## Published developer evidence

The [development gallery](../doc/development-preview.md) publishes a small set of
actual native-X screenshots with synthetic files, descriptive alternatives,
configuration examples and an image hash/provenance manifest. It is a repository
guide, not an installed manual or finished-release claim. Reuse inspected,
source-bound captures rather than rerunning the matrix for documentation updates.

`make check` verifies current inline-Markdown local file targets offline,
including decision/verification links and the archive index. It ignores code
examples, archive internals and vendored documents; remote URLs, anchors,
reference-style Markdown and HTML still need their own checks. Document imports
retain original input/output SHA256 values and dated scope. Portable navigation
notes identify retired fixture paths and historical source positions. Archived
control files remain inactive/local; publication does not adopt their workflow.
