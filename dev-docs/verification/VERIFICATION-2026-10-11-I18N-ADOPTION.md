# Translation maintenance adoption — 2026-10-11

Owners: I18N-01 / MAINT-02 / ARCHIVE-01. Status: locally qualified and adopted.
Existing FVWM configuration names, arguments and grammar are unchanged.
Translation text is unchanged. This is a maintenance/source-distribution trial,
not a published release or native release-sanitizer CI result.

## Change and source scope

NLS detection, catalog build/install selection and the configuration summary now
agree. Automatic NLS needs the gettext library and msgfmt; explicitly enabled
NLS requires both. Disabled NLS creates no catalog targets or installs.
INSTALL.md documents the dependency and feature behavior. The maintained Python
translation tool replaces the old shell helper, uses shared LINGUAS discovery
and publishes complete candidates without overwriting sources. The ten unused
committed GMO binaries and old helper are retired after qualification; an exact
hash/mode-verified backup and their predecessor archives remain.

Final candidate sourcef183 contains1400 files, derived from accepted parser
source2514 with only owning build/translation inputs changed. The four approved
PO Language-header corrections retain their earlier scope. All1317 core,
library, native module, test, manual, default-config, binary and Perl source
inputs match2514. Another chat's newer desktop configuration is excluded.

Two deterministic archives from a clean committed isolated fixture match.
Distribution source2663 has SHA256
`2663773a84da88243d8696db1130fa64d68eada78e15d9c4659b4e1d728fe3e2`
and1376 files after24 declared developer-metadata exclusions. All retained
hashes/modes, eleven PO sources, COPYING and .current-version match the
candidate. No active runtime/build script refers to the retired helper. No
project Git commit, publication or installed-system change is made.

## Combined qualification

| Native target/profile | Registered entries | Staged code identities | Catalog identities |
| --- | ---: | ---: | ---: |
| Linux ARM64 default / GCC | 233 | 15 | 11 |
| Linux AMD64 default / GCC | 233 | 15 | 11 |
| FreeBSD ARM64 default / Clang | 221 | 15 | 11 |
| OpenBSD ARM64 default / Clang | 221 | 15 | 11 |
| FreeBSD AMD64 default / Clang | 221 | 15 | 11 |
| OpenBSD AMD64 default / Clang | 221 | 15 | 11 |
| Linux ARM64 minimal / disabled NLS | 181 | 15 | 0 |
| Linux ARM64 explicit NLS / software rendering | 212 | 15 | 11 |
| Total | 1743 | 120 | 77 |

All profiles build the whole source, run their full registered suites and stage
all native code. Exact installed bytes match tested code after the expected
Meson RPATH transform; staged --version and patched-PNG linkage checks pass.
Each installed catalog matches direct native msgfmt output. Minimal installation
contains no catalog. OpenBSD AMD64 retains its established TCG allowance only
for verification-profile:60-second outer deadline,15-second internal deadline;
all other test deadlines are unchanged. Go/man/HTML remain disabled in these
native profiles; the unchanged accepted manual rendering is retained.

All thirty native NLS fixtures pass: enabled/default failure selection, disabled
build/install behavior and exact regeneration/install of eleven catalogs.
Thirty existing translation-unit passes are retained. Native whole-source
extraction on all six targets matches the retained138-message key set; the old
shell helper is not rerun here. Current manifest parity does not mean all later
desktop strings are translated. The macOS maintenance suite passes33 checks and
skips five native Meson fixtures because Meson is absent on the host.

Six full source/install archives and six extraction/metadata companions validate
compressed EOF/CRC,1376 source hashes/modes each, executed runner identities,
registered test sets, complete logs, configuration/compile metadata and exact
staged code/catalog bytes. Twelve focused archives retain initial and corrected
checks. Exact commands, source snapshots, package inventories, failed attempts,
validation summaries and the retirement receipt are under
`builddir/codex-2026-10-11/i18n-adoption/`.

## Retained preparation and collection failures

All six initial attempts pass existing translation units but fail the new
fixture: summary spacing varies and Meson native-file overrides can identify
future executables. The fixture instead tests real lookup through a private
compiler/build-tool PATH without msgfmt. Only the five affected checks rerun;
production source is unchanged by this correction.

Both current BSD ARM64 overlays need gettext developer programs. Signed package
preparation adds gettext-tools1.0/libtextstyle1.0 on both and libunistring1.4.2 on
FreeBSD. FreeBSD pkg also updates2.7.5 to2.8.4; no other existing package changes.
OpenBSD has no existing package change. Normal signature checks stay enabled;
clocks, base disks and accepted build trees are preserved. Earlier AMD64 package
preparation retains its separate report.

An initial archive validator incorrectly expected excluded developer metadata;
the corrected validation checks the declared exclusions. The first whole-source
upload uses the wrong runner filename and stops before checker execution; all
six logs remain. Linux ARM64 metadata collection initially assumes colon-
qualified test names, while its Meson uses slash-qualified names. Its successful
POT output is reused and only metadata collection recovers; no test, timeout or
extraction reruns. Later AMD64 collection accepts both observed formats.

## Remaining owners

Native Linux release sanitizer CI, actual physical/toolkit/font/input checks,
representative themes and public release preparation retain their owners.
New UI extraction/translation coverage and eleven-language manual/configuration
polish remain separate I18N/TEXT/DOCS work. Source archive tooling and Meson dist
workflow adoption remain separately gated; this clean fixture is not a reviewed
public release. No unrelated runtime or configuration-reader audit is closed.
