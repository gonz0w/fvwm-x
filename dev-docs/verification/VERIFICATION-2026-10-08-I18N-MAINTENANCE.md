# Translation maintenance and catalog regeneration

Tracker owners: MAINT-02 / I18N-01. Three related candidate-tool stories are
implemented and focused-qualified on 2026-10-08. The initial checkpoint preserves
translation sources and the legacy helper. A separate metadata follow-up below
corrects four empty language headers.

## Changes and exact source

The candidate tool discovers languages from `po/LINGUAS`, the same list used by
Meson. The legacy helper's separate ten-language list omits Georgian; the current
maintained list contains eleven languages. `languages` prints that list without
creating output. `build --all` and `update --all` preserve its order and publish
one complete candidate directory. A later catalog failure discards partial
output. Existing single-language commands remain available.

The 34-file focused source archive is
`builddir/codex-2026-10-08/i18n-maintenance/source-i18n.tgz`, SHA256
`368d184a855fdcaffe1580b10c88834604498bb9c6512ceb30e34b3553fffa24`.
It contains the actual extraction inputs, eleven PO sources, maintained manifests,
legacy helper and changed candidate/test owners. Exact fingerprints, runners,
command logs, generated candidates and comparisons are retained alongside it.
Documentation updates are separate from this tool/source archive.

## Passing checks

- macOS: all 30 maintenance tests pass, including five new language-list and
  complete-publication checks. Actual gettext generates a template, builds all
  eleven catalogs in both fuzzy modes and updates all eleven PO candidates.
- Linux ARM64: five focused tool checks pass. The unmodified legacy helper runs
  in a disposable source copy. All 138 normalized message entries match the new
  extractor. Each of eleven `--use-fuzzy` catalogs matches the legacy helper's
  binary output; each default catalog matches a direct gettext build.
- Native FreeBSD/OpenBSD ARM64: five focused tool checks pass per target and
  all 34 source fingerprints match. Actual native gettext extraction matches
  the 138 saved Linux legacy message entries. Both compilation modes for all
  eleven catalogs match their native direct-gettext output. Eleven update
  candidates are generated on each target. The legacy GNU-sed-dependent shell
  helper is not rerun on BSD; comparisons distinguish that scope.
- Clean catalog-only Meson fixtures on Linux/FreeBSD/OpenBSD ARM64 use the actual
  `po/meson.build` and `LINGUAS`, with no committed GMO inputs. All eleven catalogs
  regenerate and install on each target: 33 staged catalog identities match the
  candidate default builds. This is not a clean whole-FVWM release build.

The extraction scan found no additional literal `_()` C consumer outside the
existing manifest: Form's unmatched call translates runtime configuration text,
and the shared header defines the macro. This does not establish coverage of
new Go/Lua desktop strings or every dynamic configuration string.

Default compilation excludes fuzzy translations, matching normal Meson behavior.
The old helper always includes them; `--use-fuzzy` reproduces that policy.
Updating uses no automatic fuzzy matching and preserves source PO files.
Gettext retains template date/header ownership; template bytes are not asserted
identical after metadata normalization. Existing locale-header warnings for four
catalogs are recorded; no translation text or header was changed silently.

## Verification environment changes and remaining gates

The current ARM64 guests initially had only gettext-runtime. FreeBSD received
gettext-tools 1.0, libtextstyle 1.0 and libunistring 1.4.2. Its package manager
unexpectedly upgraded itself from 2.7.5 to 2.8.4, a prerequisite omitted by the
preview; this completed before the targeted stop check. No rollback or other
system upgrade was performed. Record this change when reusing that guest.

OpenBSD's clock was about 22 hours behind the host; the first package lookup
failed OCSP verification. Correcting the owned guest's UTC clock resolved it.
Normal verified package installation supplied gettext-tools/libtextstyle 1.0.
TLS/package verification stayed enabled. These changes concern developer-tool
environments, not new FVWM desktop dependencies.

AMD64 generation and whole-source NLS/default/minimal/feature release/install
qualification remain migration gates. Keep the old helper and committed GMO
files until those gates and distribution/reference checks pass. The frozen
runtime checkpoint `3c37` is independently locally qualified; this candidate
does not replace or repeat its runtime matrix. Native release sanitizer CI,
clean committed source-release qualification and new translations remain open.

## Language metadata follow-up

Arabic, Russian, Swedish and Simplified Chinese PO headers had empty `Language`
fields. They now identify `ar`, `ru`, `sv_SE` and `zh_CN`, matching their catalog
paths. Comparison against the original frozen archive proves those four header
fields are the only PO changes; translator credits, dates and message text remain.

The separate six-file `source-language-metadata.tgz` and its fingerprints retain
the four updated PO files and unchanged candidate owners. Actual Linux/FreeBSD/
OpenBSD ARM64 checks compile all four in both fuzzy modes. All 24 catalog
comparisons preserve every translated message and expose the corrected language
metadata; the previous header warnings are absent. macOS also compiles all eleven
default catalogs without those warnings and preserves the four catalogs' message
maps. Earlier 34-file/33-install results remain evidence for their original source,
not the four updated binary catalogs. No committed GMO file is replaced here.

See the [current tracker](../PROJECT-TRACKER.md#current-checkpoint-status),
[maintenance guide](../../tools/maintenance/README.md) and
[translation workflow](../../po/README.md).
