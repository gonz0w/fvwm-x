# Published developer documentation/evidence — 2026-10-13

MAINT-02 child [#89](https://github.com/gonz0w/fvwm-x/issues/89) is one three-story
batch: restore referenced documentation, make evidence navigation portable, and
add an offline current-document link check. Base:
`553ce61aa3e1b3e506e256397f158bb530235b75` (build-efficiency PR #88 merged).
Final head/tree/base are in the PR; tested inputs are identified below.

## Restoration and provenance

The prior guide audit observed 227 absent targets/595 references in its selected
published guides. The full reference closure restores 370 frozen original
project documents: 232 current guides/decisions/reports and 138 archive documents.
362 are byte-identical to their frozen inputs; the others have explicit
navigation/current-hosting notes or file-link transformations. Original SHA256,
output SHA256 and changes are in the [import map](VERIFICATION-2026-10-13-DOCUMENTATION-EVIDENCE-IMPORT.json).
Original bytes remain in the local immutable `original-documents.tgz`, whose
SHA256 is recorded in that map. The prose/decisions/dated results remain their
original assertions, not new runtime qualification.

Five CI asset receipts are exact copies from their original local checkpoint;
the old validator is `.py.txt` data, not an active runner. Superseded XDG fixture
names remain described as frozen `efe8` inputs, with a link to current provider
fixtures. Historical workstation source links use portable file paths and an
explicit dated-position note; current line identity is not asserted. The source
map links published configuration/services and leaves the separately owned
artwork generator out of that map.
README/index clarify the qualified core/utility publication scope. Desktop/artwork
runtime, source-release adoption and manual/site redesign keep their owners.

Archive prose and the original migration manifest are discoverable. Retired GSD
control/configuration files stay in the local archive; a README identifies that
boundary. No active workflow, sentinel, hook or runtime source is restored.
Archive prose is historical data, excluded from current-guide link validation.

## Checker and actual verification

`make check` now checks inline Markdown local file links in root/current
developer guides, decisions, reports, maintenance/CI/PR guides and the archive
index. It skips fenced/indented/inline code, archive internals and vendor docs;
it handles relative/escaped/angle targets, titles and balanced path parentheses.
Local targets must exist inside the checkout. Reference-style Markdown, anchors,
HTML and remote URLs are explicitly outside this small offline check. No renderer,
network, WM compilation or VM boot is required by it.

- Final macOS focused policy: 16/16 passes. Initial macOS parent-alias failures
  were repaired without following document symlinks; cyclic-target diagnostic
  expectations retain Python-version portability. Initial restoration missing
  links/failed staging assertions are retained in local logs and corrected.
- Source-bound affected Linux/FreeBSD/OpenBSD AMD64/ARM64 checks: 96/96 passes,
  no skips, in existing guests with fresh owned test directories. This is Python
  filesystem/parser policy coverage; no compiler or whole-WM matrix ran.
- Local `make verify`: 137 passes, seven existing native-Meson skips. Previously
  accepted Meson/NLS/runtime evidence retains its original scope. New-link and
  repository/branch/staged whitespace, Python syntax and JSON checks pass.
- Full selected current-document graph: zero local file-link errors. All 370
  frozen input/output identities and five receipt identities validate. Imported
  archive internals have separate historical scope; this is not full Markdown,
  external URL, anchor or HTML conformance.
- Runtime C/header/Go, Meson/dependency/default profiles, installed manuals,
  configuration format, VM disks and active CI/retired automation remain unchanged.
  Restored report source identities describe their dated inputs, not today's
  private shared checkout or another chat's unpublished runtime.

| Tested input | SHA256 |
| --- | --- |
| `tools/maintenance/common.py` | `8b42f45cd946c1823b2cc9bbbad36c63e2c76ed7b34ee478b1ea884442964826` |
| `tools/maintenance/repository_links.py` | `eedab8aa96eda56a0737c991d3c738a2c9c486aac7a7c816d3c197bddc5287c2` |
| `tools/maintenance/tests/test_repository_links.py` | `b3f0b7c0bd5d50f994b7546b5ed1e80486d7d8026ebf3dd53558c75654235d5b` |

[Native policy receipts](VERIFICATION-2026-10-13-DOCUMENTATION-EVIDENCE.json), [import provenance](VERIFICATION-2026-10-13-DOCUMENTATION-EVIDENCE-IMPORT.json).
Raw logs, original frozen documents and scripts remain in ignored
`builddir/codex-2026-10-13/documentation-evidence/`. Local original archive SHA256:
`d4bb98b9ec6b3bccbcf699c8e9a7522fae76b7264c468106a7fcc44c1c7a331b`.

This closes only the documentation availability/link-check slice on merge.
MAINT-02 source-release layout/build/install/license and predecessor-retirement
gates, native/physical release checks and DOCS-01 redesign stay open.
