# Full XDG menu provider/frontend fixtures — 2026-10-12

Owners: RUNTIME-GO-01 / UTIL-01 / COMPAT-02; normative interoperability remains
XDG-01-owned. Status: **affected fixture checkpoint locally qualified**. This is
a baseline for the full menu service and its configuration frontend, not a Go
port, interpreter retirement, installed runtime acceptance or standards
conformance claim. The launcher and desktop controller have separate owners.

## Frozen source and oracle

Source archive SHA-256:
`efe859530d63d9aead353201566f93fec0e8aae1386ab3c2eb017bd5471ac5f0`.

The archive contains the unchanged
[Python provider](https://github.com/gonz0w/fvwm-x/blob/6d8db4199c207e22ddac86c9641bcea17262527e/bin/fvwm-menu-desktop.in), unchanged
[Perl Form frontend](https://github.com/gonz0w/fvwm-x/blob/6d8db4199c207e22ddac86c9641bcea17262527e/bin/fvwm-menu-desktop-config.fpl), new
fixture harness (`tests/test_xdg_menu_contracts.py` in the frozen `efe8` input),
recorded observations (`tests/fixtures/xdg-menu-contracts.json` in that same input), scope
hashes and the exact two-line owning registration from
[tests Meson](../../tests/meson.build). Both producers match the preceding
qualified Script checkpoint source byte for byte. Other shared Meson blocks,
desktop runtime files, builds and VM evidence are outside this source scope.

None of the six native environments had PyXDG installed. Full XML observations
therefore use an isolated, unmodified **PyXDG 0.28** source oracle; it is not
installed into a guest interpreter, vendored into the project, or made a new
runtime/build prerequisite. The existing provider already requires PyXDG.
Its [PyPI source distribution](https://pypi.org/project/pyxdg/0.28/) has SHA-256
`3267bb3074e934df202af2ee0868575484108581e6f3cb006af1da35395e88b4`.
The downloaded archive is checked before extraction and retained in each proof.
Archive members are checked for traversal and non-regular/symlink entries.
A checksum identifies the oracle; it is not a dependency security review.

Evidence is under `builddir/codex-2026-10-12/xdg-contracts/`: `source.tgz`,
`scope.json`, `oracle.json`, `dependencies.json`, `remote.py`, `dispatch.py`,
`validate.py`, `results.json`, `validation.json` and six `*-evidence.tgz` files.
The exact executed guest runner has SHA-256
`3ef8a10b87d3cb49c48869e03995343393f979d48a3105c33173375ede0bb630`.

## Acceptance and isolation

Each target passes **52 distinct observations**: thirteen base/paired frontend
observations and thirty-nine real provider/XML observations. Each also executes
the thirteen base observations again through the exact production registration
in an isolated fixture-only Meson project. This totals **390 observation
executions**, including 78 deliberate registration repeats, not 390 distinct
cases. Six Meson entries pass. The extracted test log and result JSON are checked
against source/runner/oracle identities; current owning source hashes still match.

| Native target | Architecture | Direct base / XML observations | Registered base repeat |
| --- | --- | --- | --- |
| Linux | ARM64 | 13 / 39 pass | 13 pass |
| Linux | AMD64 | 13 / 39 pass | 13 pass |
| FreeBSD | ARM64 | 13 / 39 pass | 13 pass |
| FreeBSD | AMD64 | 13 / 39 pass | 13 pass |
| OpenBSD | ARM64 | 13 / 39 pass | 13 pass |
| OpenBSD | AMD64 | 13 / 39 pass | 13 pass |

The Python harness owns a temporary HOME, FVWM_USERDIR, XDG configuration/data/
cache directories, menus, applications, directory entries and icon directory.
Python runs with site imports disabled and the supplied oracle path. The private
PATH has no image conversion executables. No generated application or FVWM
action is executed. The provider runs as its actual source, not a substitute
XML/parser implementation.

For frontend capture, an owned executable answers only the two documented
`--get-menus all` / `--get-menus desktop` calls and records their literal arguments.
Small Perl callbacks capture `unlock`, the generated file and the final Form
command; they do not contact FVWM or create a window. The legacy temporary file
is removed, and no preferences file is created. These are frontend generation
checks, not live Form save/export/button acceptance. No borrowed process is killed.

Only temporary-root paths, the explicitly absent conversion-tool diagnostic,
unordered discovery lists and independent Perl hash-iteration export/save rows
are normalized. Choice order, configuration fields, action text, repeated save
blocks, output bytes and statuses remain recorded. Error observations assert a
specific exception message without treating version-dependent traceback frames
as a contract. Recording requires explicit `--record`; candidate invocations
cannot replace the legacy reference.

## Captured contracts and gaps

| Surface | Captured in this batch | Required for the separate replacement checkpoint |
| --- | --- | --- |
| CLI and missing provider | Help/version, unknown and obsolete options, invalid enumeration arguments, absent PyXDG diagnostic/error menu, custom error title | Preserve supported options; explicit bounded diagnostics; test replacement-specific limits |
| Menu output and ordering | Titles, insertion, dynamic regeneration, terminal wrapper, custom regeneration, all four include modes, submenus, multiple/empty menus and list mode | Native nested-X popup/action checks, installed paths and paired Configure action |
| XML/application data | Filename/category/boolean inclusion, exclusion, merge file, deleted/moved submenu, unallocated entries, layout/separator/inline hint, AppDir precedence and DefaultAppDirs | Independent normative tests; merge directories/parent merges, default directory/merge rules, legacy directories, duplicates/cycles/large input and precise supported feature inventory |
| Desktop presentation | Hidden/NoDisplay/TryExec, OnlyShowIn/NotShowIn observation, French/UTF-8 labels, ampersand/percent escaping, empty/spaced argv and Exec field-code observation | Correct field-code/argument semantics, escaped labels/paths, environment visibility, literal launch arguments and absent capabilities |
| Saved preferences | Existing Form field names, title/terminal/include/menu selection, CLI override; off-title/spaced-title defect controls | Read existing data without migration; preserve configured values; private complete writes and actual Form save/regenerate/export controls |
| Frontend generation | Empty selection, selected menus, duplicate basenames, twelve-choice row grouping, all option/default/action/save/help/use-data rows, provider call order and temporary cleanup | Update provider/frontend/install/help/manual references together; test operation errors and installed default/minimal/Go-disabled behavior |
| Icons | Absolute SVG/native size suffix, StartupWMClass Style line and theme marker without converters | Inherited theme/size/fallback lookup, non-SVG data, bounded conversion/cache ownership and actual native rendering |

Current defects are baseline observations, **not requirements to reproduce**:
custom XDG directories without `/etc/xdg/menus` raise KeyError; an empty explicit
menu list and invalid integer size raise exceptions; saved `TitlesOff` is ignored;
saved spaced titles are truncated; the Exec regex removes all observed field
codes including `%%`; desktop visibility includes both conflicting FVWM filters
when the library's windowmanager is unset. The SVG suffix is emitted in a Style
line but fails the menu label's filesystem-path check. Duplicate frontend menu
basenames share a choice identifier. These fixes can retain the configuration
format and do not require a new compatibility approval under current policy.
The inline hint remains a submenu in the captured oracle; optional layout hints
need an explicit implementation policy rather than a conformance assertion.

Normative authority is the [Desktop Menu 1.1 specification](https://specifications.freedesktop.org/menu/latest/),
[Desktop Entry 1.5 Exec section](https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html)
and [Icon Theme specification](https://specifications.freedesktop.org/icon-theme/latest/).
Library output and legacy regex behavior do not establish normative correctness.
Startup notification, desktop actions, MIME roles and autostart are not qualified
by these fixtures; XDG-01 and SESSION-01 retain their existing scope/order.

## Reproduction and remaining gates

With the legacy Perl available, the dependency-free registered section is:

```sh
python3 tests/test_xdg_menu_contracts.py
```

For XML coverage, unpack the checksum-checked oracle into a private test directory
and pass that directory explicitly; the script does not download dependencies:

```sh
python3 tests/test_xdg_menu_contracts.py --pyxdg-dir /path/to/pyxdg-0.28
```

The local first XML attempt used Python 3.14.7, where unmodified PyXDG 0.28 calls
the removed `ast.Str` API. It failed before the first XML observation. Local
record/replay then passed using macOS `/usr/bin/python3` 3.9.6; native qualification
uses the guests' existing Python 3.12/3.13. This is an oracle limitation, not a
passing Python 3.14 runtime result. The failed output and interpreter identity are retained as `local-python314-oracle.*`. No third-party source was patched. Guest
extraction emitted Python's future-default-filter deprecation warning after the
member validation; it did not fail a test. Initial host proof validation assumed
unprefixed Meson names; it was corrected to allow exactly the production name
and its known fixture-project prefix. Guest source, execution and results did
not change and were not rerun for that host-only correction.

Upkeep checks resolve 772 local links in the five updated guides/reports and pass
the owning diff/whitespace checks. A whole shared-checkout diff check also reports
whitespace in concurrently changed generated Script scanner/parser files outside
this checkpoint; those files were preserved. The output is retained in
`unrelated-diff-check.txt`, rather than reported as a clean whole-checkout pass.

There is no production implementation change, new installed feature or manual
option in this checkpoint. No core build, sanitizer, default-X or installed
runtime matrix was repeated for these fixture-only additions. Full provider/
frontend Go adoption, bounded input/process/cache ownership, complete installed
consumer qualification and COMPAT-02 retirement remain open. Actual provider CI,
physical hardware and release publishing are separate gates. The
[tracker](../PROJECT-TRACKER.md#execution-order) remains authoritative.

The named fixture files describe the original `efe8` checkpoint and were
superseded by the coordinated port. Current provider fixtures live in
[test_xdg_desktop.py](../../tests/test_xdg_desktop.py); the dated frozen-input
paths above are not installed/current test entry points.
