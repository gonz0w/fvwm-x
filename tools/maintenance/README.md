# Maintenance tools

The documented `./build` options now delegate to the standard-library Python
wrapper, and Meson documentation targets use `docs.py` / `manuals.json`.
Modern parser runtime and translation maintenance are adopted. Metadata and
source-release workflow replacements remain explicit candidates; CI and release
workflows have not switched. The build/documentation paths use the existing
Meson Python requirement (3.7+); other maintenance tools retain their Python
3.9+ developer baseline. No new desktop library is required.

All generator outputs go to a **fresh** directory: an existing destination is
refused, partial output is removed on failure, and complete output is published
only after success. Use a new name when repeating a comparison. The build
wrapper is different: it can reuse its explicitly selected Meson build and
append its log, and wipes only when explicitly requested with `./build -f` or `--wipe`. Do not point it at
another thread's active build. This is trusted developer workspace tooling,
not a privileged service or hostile-input sandbox.

Run from the repository root:

```sh
python3 -m unittest discover -s tools/maintenance/tests -v

# Review the plan first; the default builddir is separate from ./build's.
python3 tools/maintenance/build.py --dry-run -Dgolang=disabled --test
python3 tools/maintenance/build.py -Dgolang=disabled --test --verbose

# Explicit install and prefix are supported; installation is never implicit.
# Options containing spaces remain single arguments.
python3 tools/maintenance/build.py --dry-run --prefix '/path with spaces' --install

# Render every manual, including the desktop, with optional Prompt separately.
python3 tools/maintenance/docs.py --output builddir-maint/docs-1 --format both
python3 tools/maintenance/docs.py --output builddir-maint/docs-prompt-1 --format man --prompt
python3 tools/maintenance/docs.py --output builddir-maint/website-1 --format html --website

# Candidate Perl output; does not replace perllib/FVWM/*.pm.
python3 tools/maintenance/generate.py metadata --output builddir-maint/metadata-1 --epoch 0

# Candidate parser/scanner: default pair is Bison 3.8.2 and Flex 2.6.4.
python3 tools/maintenance/generate.py parsers --output builddir-maint/parsers-1
# The portable developer wrapper uses the same fresh-output contract.
sh modules/FvwmScript/fvwmscript-syntax.sh --output builddir-maint/parsers-2

# Maintained gettext output, using po/POTFILES and po/LINGUAS.
python3 tools/maintenance/translations.py languages
python3 tools/maintenance/translations.py pot --output builddir-maint/pot-1
python3 tools/maintenance/translations.py update --language fr --output builddir-maint/fr-update-1
python3 tools/maintenance/translations.py build --language fr --output builddir-maint/fr-build-1
python3 tools/maintenance/translations.py update --all --output builddir-maint/all-update-1
python3 tools/maintenance/translations.py build --all --output builddir-maint/all-build-1

# Read-only working-tree inventory, with optional local pkg-config versions.
python3 tools/maintenance/source.py inventory --untracked --dependencies --output builddir-maint/inventory-1

# Source archives require a clean, committed snapshot by default.
python3 tools/maintenance/source.py archive --ref HEAD --epoch 0 --output /tmp/fvwm-source-1
```

`build.py` uses argv subprocess calls, drains diagnostic output in bounded
chunks, and returns the failed command's exit status even in verbose mode.
A changed option or `--reconfigure` reconfigures without `--wipe`. `--install`
retains terminal input for Meson's normal privilege handling. Quiet-mode errors
point to the retained log. GNU `nproc`, shell `eval` and Bash are unnecessary.
A dry run performs no build, output creation or installation.

`manuals.json` is the shared source list used by Meson and the standalone
renderer. Meson tracks it as a configure dependency. The renderer copies
includes and input sources into its output, extracts validated sections, and
handles Prompt consistently for man-only or HTML builds. `--website` creates
HTML fragments/front matter for review; it does not publish or push a website.
Add `--website-directory` for a flat fresh `Man/` output. The public website
migration is deferred to [DOCS-01](../../dev-docs/decisions/WEBSITE-MANUALS-BRIEF.md);
the legacy entry point remains available and other migrations can proceed.

`generate.py metadata` supports the current command-table and bounded integer
macro grammar. Unknown constructs fail rather than producing partial public
metadata. It checks exact named extended-mask coverage and preserves public
exports and POD through the checked templates in `metadata/`. The committed
Constants.pm and Commands.pm are the normal build/install inputs. The two old
developer entry points now delegate to this generator and require `--output`;
they do not overwrite runtime metadata. PERL-PROTO-01 owns native packet decoder
qualification. JSON companions provide the same values for future Go/Lua users.
Use an explicit reproducible epoch for command metadata. `parsers` requires an
exact declared tool-version pair and records it with outputs. The legacy pair
can be compared with `--bison-version 2.3 --flex-version 2.6.4`. The modern
pair is adopted and locally qualified by the combined GEN-01 checkpoint on
eight compiler profiles across all six targets. Generated C sources remain committed, so normal
builds do not need Bison or Flex. The developer wrapper requires `--output` and
does not overwrite runtime files. Failed generation publishes no partial set;
both generated C files must include `config.h` first. `generators.json` records
the exact versions and input/output SHA256 values.

The [AMD64 translation follow-up](../../dev-docs/verification/VERIFICATION-2026-10-11-I18N-AMD64.md)
completes focused real generation and catalog-only staged installs on Linux,
FreeBSD and OpenBSD. The later [adoption checkpoint](../../dev-docs/verification/VERIFICATION-2026-10-11-I18N-ADOPTION.md)
completes the whole-source/distribution trial and retires the helper and unused
GMO binaries with an exact backup.

`translations.py` delegates C extraction and catalog handling to gettext; Python
handles FVWM expansion and Script translation markers with locations and
escaped text. Unsupported literal escapes fail. `init` writes a new language
candidate; `update` preserves original files and disables automatic fuzzy
matching for review. Catalog builds exclude fuzzy translations by default;
`--use-fuzzy` reproduces the old helper's always-enabled fuzzy compilation.
`languages` reads Meson's `po/LINGUAS`, including Georgian. `build --all` and
`update --all` process that same list and publish only a complete output set;
single-language commands remain available. These explicit behavior differences are retained by the adopted workflow.
Normalized msgid/context/plural parity and staged catalogs pass on all six targets. POT creation dates remain gettext-owned; this is not a byte-identical
POT generator. Candidate strings from the current manifest are not a claim
that every new desktop string is translated.

The [translation maintenance checkpoint](../../dev-docs/verification/VERIFICATION-2026-10-08-I18N-MAINTENANCE.md)
records exact extraction/catalog parity and clean catalog-only Meson fixtures
on Linux/BSD ARM64. Its earlier pending adoption/removal gates are completed by the separate
[I18N-01 adoption](../../dev-docs/verification/VERIFICATION-2026-10-11-I18N-ADOPTION.md).
The clean committed source fixture is not a reviewed public release.

`source.py inventory` handles NUL-separated Git paths, file modes, hashes,
symlinks, deleted and optional untracked files without reading symlink targets.
It is an observation while other work may change files, not an atomic source
snapshot. The report also records vendored versions and workflow action references with
source locations; this text inventory is not YAML validation or a security
verdict. Dependency version strings need package/backport advisory evidence.
`archive` uses an immutable resolved Git commit, normalizes archive ownership
and timestamps, preserves executable bits with normalized permissions, and refuses traversal/unsafe links.
It excludes historical/developer metadata from the candidate archive without
deleting the originals. It keeps build-required tests and `.current-version`.
`--allow-dirty` explicitly makes a **development-only archive of the chosen
commit**, excluding working-tree changes; it is never a way to ship uncommitted
sources. There is no extraction, signing, tagging, pushing or publishing step.

Recorded adoption checks and limitations are in the
[tooling report](../../dev-docs/verification/VERIFICATION-2026-10-05-TOOLING.md).

## Migration crosswalk

| Current tooling | Parallel replacement | Remaining gate |
| --- | --- | --- |
| `build` | Adopted `build.py` with legacy options | Six recorded Linux/BSD profile matrices pass; Linux AMD64 Go compiler has an emulation blocker |
| `generate_ad_file.sh`, website helper, duplicated lists | Meson adopts `docs.py` / `manuals.json`; legacy helpers retained | Real Asciidoctor output and manifest-reconfiguration comparisons pass on six targets; public website migration deferred to DOCS-01, without blocking other adoption |
| `create-constants`, `create-commands` | `generate.py metadata` | PERL-PROTO-01 wire/decode/export/POD compatibility and generation integration |
| `fvwmscript-syntax.sh` | Portable wrapper for `generate.py parsers` | GEN-01 locally qualified: eight standard compiler profiles, staged identities, four default BSD servers and scoped feature/sanitizer checks |
| Retired `po/fvwmpo.sh` and unused GMO binaries | Adopted `translations.py`; Meson builds PO catalogs | Six standard whole-source profiles plus two Linux ARM64 feature profiles, exact installs,138-key parity, deterministic committed-fixture export and reference checks pass |
| `dist-tarball.sh` | `source.py archive` | Clean reviewed snapshot, source-release layout/build/install and license checks; Meson dist adoption |
| Ad hoc file/dependency searches | `source.py inventory` | Review reports per target; no automatic removal decisions |
| Current CI/release/Docker/Nix | CI-01 / REPRO-01 roadmap | Repository/publishing ownership and reviewed immutable pins; no active workflow switched |
| `conf-cmd.sh`, version helper | Retained | Already suitable for current needs; no gratuitous rewrite |

## Verification and limits

### Initial candidate evidence

The initial 2026-10-05 candidate checkpoint passed all 20 isolated tool tests
on macOS and Linux ARM64. Checks cover
verbose failure/argv handling, preserving existing data, missing/reversed manual
sections and parity with the old extractor, optional Prompt, renderer failure,
valid deterministic Perl candidates, version refusal, translation escaping and
failures, Git paths/links/deletions, and deterministic archive/refusal boundaries.
Real Linux gettext extraction and a checked French catalog succeed. An isolated
real Meson C fixture passes setup/compile/test/install into a prefix containing
spaces and configured-build reuse. The host's
Bison 2.3/Flex 2.6.4 generate candidate files outside runtime sources. Existing
runtime generated sources and build targets remain unchanged.

At that initial checkpoint, actual Asciidoctor rendering and Bison 3.8.2 were
unavailable; renderer tests used a deterministic subprocess fixture. Later
build/manual adoption results are recorded below. Modern parser runtime is now locally adopted under the
[GEN-01 report](../../dev-docs/verification/VERIFICATION-2026-10-11-PARSER-ADOPTION.md).
Clean source-release checks remain a separate migration gate. See the
[cleanup roadmap](../../dev-docs/CLEANUP-ROADMAP-2026-10-05.md) and
[verification report](../../dev-docs/verification/MAINTENANCE-VERIFICATION-2026-10-05.md).

### Accepted build and manual adoption

The adoption batch passes 25 tool tests on all six targets, including legacy option ordering,
space-containing paths, actual entry-point failure propagation and preserving
existing website output. Default/minimal/feature builds, tests and staged
installs pass the six recorded target profiles. Linux ARM64 Clang also passes.
Real Asciidoctor comparisons and manifest-triggered reconfiguration pass on all
six targets. The [adoption report](../../dev-docs/verification/VERIFICATION-2026-10-05-TOOLING.md)
records exact counts, retained failures and limits: Linux AMD64 Go-enabled
qualification, native Linux sanitizer CI and clean source-release gates remain
open. This does not qualify the other generator candidates or the current
whole-project release. Legacy extraction/website scripts are retained for comparison and
rollback until their corresponding replacement gates are accepted.

The native NLS integration fixtures additionally require Meson, a C compiler,
the gettext development library and msgfmt. All five fixtures pass on each of
the six targets; host skips are recorded separately from native execution.
