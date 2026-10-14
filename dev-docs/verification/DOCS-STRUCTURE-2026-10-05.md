# Developer documentation migration

Recorded 2026-10-05. This is a documentation/context-management checkpoint,
not runtime, package or manual-content qualification. Current scheduling stays
in [PROJECT-TRACKER.md](../PROJECT-TRACKER.md).

## Scope

- Migrated 183 files: former `.planning` and `.gsd`, inherited upstream notes,
  35 dated verification/review reports and existing proposals/website brief.
  Dirty and untracked planning material was included.
- Preserved former generated AGENTS.md in history and replaced the active guide
  with concise current constraints, ownership, verification and workflow rules.
- Added maintained project/requirements crosswalk, architecture map, developer,
  workflow and documentation guides, with category indexes for decisions,
  verification and history. The active tracker path remains stable.
- Disabled `enabled` and `auto_update` in the ignored graph config. Derived
  graph/cache files and hook scripts remain local; no plugin was uninstalled.
- Preserved old requirement identifiers with a historical namespace. MONITOR-01
  and MODULE-SCALE-01 reconcile remaining qualification/consumer requirements
  against current code, without resetting implemented geometry/topology work.
- Updated installation guidance to recommend man pages and optional HTML.
  No Meson target/default, runtime source, dependency or publishing workflow
  was changed by this migration.

## Preservation and checks

Before moving files, captured a 196-file snapshot including dirty/untracked
developer/planning content, active guide/install references and graph config.
The [migration manifest](../history/MIGRATION-2026-10-05.json) records original
paths, source SHA-256, modes, destinations and hashes after link repair.
Sources were checked against the snapshot before relocation to avoid overwriting
the build thread's edits. Original bytes remain in the local migration snapshot;
all migrated destinations and modes are retained in the checkout.

Rebased Markdown links to migrated documents/source targets. Current guide,
tracker, tool-guide and archive links were checked, including source-line suffixes.
Checked unique active backlog IDs and retained the single execution-order table.
Retired planning roots are absent; graph automatic updates are false. Historical
content remains discoverable and is not treated as current instructions.

The existing 28-page website manual export totals 1,434,135 bytes. An HTML-parser
inspection found zero script tags and zero external asset requests. This supports
keeping optional derived HTML; it is not a full website audit or new render.

## Limits and follow-up

Fresh Asciidoctor generation could not run locally: the host has mandoc but no
Asciidoctor; Podman access returned a storage readlink error after socket access
was permitted. Existing tooling evidence is retained in the
[adoption report](VERIFICATION-2026-10-05-TOOLING.md). No package installation,
running build/VM change or manual-source rewrite was used to work around this.

DOCS-01 retains manual content review, formatter/lint and staged OS install
qualification. Website visual design remains deferred. Native sanitizer CI,
physical hardware, third-party themes and toolkit/compositor gaps are unchanged.
Future evidence belongs under verification and contracts under decisions.
