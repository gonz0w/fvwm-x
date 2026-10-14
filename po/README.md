# FVWM translations

Meson builds and installs catalogs from the eleven `.po` sources named in
[`LINGUAS`](LINGUAS). Generated binary catalogs are build outputs; maintain and
commit the `.po` sources instead. [INSTALL.md](../INSTALL.md#native-language-support)
explains automatic, required and disabled NLS builds.

## Maintain translations

Run the Python standard-library tool from the repository root. Gettext supplies
`xgettext`, `msguniq`, `msgmerge`, `msgfmt` and `msginit`; Python handles FVWM and
FvwmScript translation markers. Each operation publishes complete output to a
fresh directory and preserves the existing sources.

```sh
python3 tools/maintenance/translations.py languages
python3 tools/maintenance/translations.py pot --output builddir-maint/pot-1
python3 tools/maintenance/translations.py update --all --template builddir-maint/pot-1/fvwm3.pot --output builddir-maint/po-review-1
python3 tools/maintenance/translations.py build --all --output builddir-maint/catalogs-1
```

Review candidate updates before applying them. `--language LL_CC` selects one
catalog instead of `--all`. Updates disable automatic fuzzy matching. Builds
omit fuzzy translations, matching Meson; `--use-fuzzy` is available for explicit
comparisons with older catalog builds. Do not treat fuzzy entries as reviewed
translations.

To add a language, generate a candidate with `init --language LL_CC --output DIR`,
review its metadata and messages, then add its `.po` file and identifier to
`LINGUAS`. Use a valid gettext locale identifier. Language discovery rejects
missing catalogs and duplicate identifiers. Run a complete build before
committing the source changes.

## Extract messages

[`POTFILES`](POTFILES) is the maintained extraction list. It covers C calls to
`_(string)`, configuration expansion `$[gt.string]` and FvwmScript
`WindowLocaleTitle`/`LocaleTitle` markers. Add a new source path there before
regenerating the template. FvwmScript uses the existing `UseGettext` option.
Unsupported literal escapes fail extraction instead of silently changing a key.

The tool preserves source locations. Gettext owns POT timestamps, so compare
message keys and context/plural forms rather than requiring byte-identical POT
files. Extraction of the maintained list does not mean every new desktop string
is translated. Translation coverage and manual/configuration language polish
remain separate project work.

## Check translated UI

Preserve leading spaces, layout spaces and substitution syntax. Menus can use
`&` to identify a shortcut; place it at the intended translated character.
FvwmForm and FvwmScript layouts can have fixed sizes, so test longer labels in
the actual module. Test font coverage, text input and layout on the target X
server; successful catalog compilation alone does not establish UI quality.

The [maintenance guide](../tools/maintenance/README.md) explains the tools and
historical checkpoints. The old shell helper and committed GMO removal remain
subject to the owning I18N-01 adoption checkpoint in the project tracker.
