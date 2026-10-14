# Parallel maintenance-tool verification

Date: 2026-10-05. Scope: new standalone developer tools and documentation.
The current build wrapper, Meson graph, CI/release hooks, installed utilities
and runtime C sources were not changed by this maintenance batch. Concurrent
runtime work remains in the checkout and is outside these verification claims.

## Completed checks

- 20/20 isolated tests pass on macOS with Python 3.14.7 and on Linux ARM64 in
  the existing `slate-review-oct04` container. No running desktop was restarted.
- Real Meson setup, compilation, test and installation pass using a tiny C
  fixture in `/tmp/fvwm-maint-real-build-20261005`. The install prefix contains
  spaces; a second invocation successfully reuses the configured build.
  The fixture never builds or installs FVWM and changes no system files.
- The verbose failure fixture exits 37; the wrapper returns 37, retains binary
  diagnostics and never starts compilation after failed setup. Dry runs create
  no build or log, and invalid jobs are rejected before work.
- Fresh-output tests preserve existing evidence and reject destinations inside
  runtime/config/build source components. Failed rendering/extraction removes
  only the tool-owned temporary stage; originals remain unchanged.
- The manual manifest matches the current Meson list, including FvwmDesktop.
  Three extracted sections match the legacy helper byte-for-byte. Fixture
  rendering verifies HTML/man output, Prompt in man-only mode and failure
  handling. It does not establish real Asciidoctor layout/link parity.
- Metadata generation produces valid deterministic Perl syntax for Constants
  and all 166 command entries, using `.current-version`. A separate source
  comparison confirms the ten installed extended constants differ from the C
  header. Installed metadata is unchanged; packet-decoder compatibility remains
  PERL-PROTO-01 work.
- The host's Bison 2.3/Flex 2.6.4 generate candidate `script.c`, `script.h` and
  `scanner.c` under `builddir-maint/`, with recorded versions. Wrong generator
  versions are refused. No generated runtime file is replaced or compiled.
- Real Linux `xgettext`/`msguniq` produce a candidate POT, and `msgfmt --check`
  produces a French catalog: 88 translated messages, 37 fuzzy translations,
  52 untranslated. The new default excludes fuzzy translations; comparison
  with the old helper's `--use-fuzzy` behavior is an explicit migration gate.
- Fixture Git archives are byte-repeatable with normalized ownership/time,
  preserve executable bits with normalized permissions and `.current-version`, omit agreed historical release
  content and retain originals. Dirty source, traversal and unsafe links are
  refused. These are fixture source archives, not a newly verified FVWM release.
- Read-only source/dependency inventory records paths, hashes, modes, deletions,
  links, vendored versions and workflow action-reference locations. It is not
  a vulnerability scanner, YAML validator or atomic working-tree snapshot.

Local evidence is retained under `builddir-maint/verification-20261005/`:
`macos-tool-tests.log`, `linux-arm64-tool-tests.log`, `real-meson-wrapper.log`
and `linux-dependency-inventory.json`. Candidate metadata, parser output and
the ten-value protocol comparison are also retained under `builddir-maint/`.
The Linux temporary candidate POT/catalog directories remain separate from
FVWM's source and active build.

## Observed Linux dependencies

This is one verification environment, not a release-wide inventory. Package
security backports and the actual loaded library paths have not been audited.

| pkg-config entry | Reported version |
| --- | --- |
| x11 / xrandr / xext | 1.8.7 / 1.5.2 / 1.3.4 |
| xft / fontconfig / xt | 2.3.6 / 2.15.0 / 1.2.1 |
| ice / xtrans / xkbcommon | 1.0.10 / 1.4.0 / 1.6.0 |
| libevent | 2.1.12-stable |
| libpng / librsvg-2.0 / cairo | 1.6.43 / 2.58.0 / 1.18.0 |
| freetype2 / fribidi | 26.1.20 / 1.0.13 |
| sm / xcursor / xfixes | 1.2.3 / 1.2.1 / 6.0.0 |
| xpm / xrender | 3.5.17 / 0.9.10 |

FreeType's pkg-config version is not necessarily its upstream release number.
The vendored cJSON header reports 1.7.17; Go pins are recorded from go.mod.
Library security/upgrade choices and primary sources are in the
[cleanup roadmap](../CLEANUP-ROADMAP-2026-10-05.md), including LIB-07 for the XPM/X11
platform baseline. Nothing was automatically upgraded in the running system.

## Remaining migration gates

Actual Asciidoctor and modern Bison 3.8.2 were unavailable in these environments.
Perform actual HTML/man/website comparisons, generator runtime/grammar checks,
BSD maintenance checks and stable-source default/minimal/optional-feature
build/test/install coverage. Build a clean reviewed FVWM source release and
verify all build-required untracked work has entered its source snapshot.
Reconcile translation manifests, msgid/context/plural sets and clean NLS catalog
regeneration. Review release ownership, immutable action/image pins and external
notification behavior before changing active CI/publishing workflows.

Keep each predecessor until its replacement has passed the relevant gates and
entry points have migrated with a rollback path. This report does not close
runtime hardening, native Linux sanitizer CI, or the six-platform release gate.
