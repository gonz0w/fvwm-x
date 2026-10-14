# fvwm-menu-directory Go port — 2026-10-12

Owner: UTIL-01/RUNTIME-GO-01, execution stage 3. Status: affected local
checkpoint qualified. Source, installed identities and collected evidence
validate. No configuration migration or new mandatory dependency. The converter,
full XDG menu provider, examples and Perl retirement retain separate checkpoints.

## Source and implementation

Frozen source
`a7e65f389b5f25334a1d3afa65072e6c2bcf183225377001213b35bec77d3eab`
composes accepted xlock source
`9ab4aabcb9464370a4db898a1a4fc26282418f94602edd58108d7a946f0f743b`
with seventeen owning paths and eight added files (1439 files total). Only the
owned directory-menu registration is composed into the accepted shared test
Meson file. Other chats' registrations and C/header edits are excluded; current
checkout files and their evidence remain intact. All frozen C/header files and
existing Go implementations are unchanged.

- Go-enabled builds install the standard-library directory producer under the
  existing command name. Disabled-Go builds retain the configured Perl producer.
  The separate `directory-menu` install tag owns either executable. The existing
  optional Go 1.21 floor remains; actual SDK versions are recorded in evidence,
  not asserted as a separate minimum-SDK verification.
- Supported options, unique abbreviations, case-insensitive names, negative
  booleans, sorting orders, hidden files, icons, configured actions, submenu
  callbacks, special directories and byte-based formatting remain usable.
  Required `--title=` still differs from a separate empty argument, as in Perl.
  The recorded reference is unchanged; candidate help/version normalization
  cannot record a replacement baseline.
- Enumeration reads batches of 128 and refuses more than 100000 entries,
  including hidden entries. Entry metadata and format plans are cached; sorting
  does not perform filesystem probes in its comparator. Date/size conversion
  runs only when requested. No measured general speed or memory benefit is
  claimed. Slow filesystem calls still have no total deadline and can delay
  synchronous PipeRead; asynchronous file browsing remains FILE-DATA-01-owned.
- Reuse navigation replaces shell state reads/writes with a private helper.
  Reads require an owned regular file without group/world write permission,
  refuse final symlinks and special files, and stop at 64 KiB. Writes require
  an owned directory without group/world write permission, anchor operations
  to its checked inode and atomically replace state with a complete mode-0600
  file. Existing plain-text state remains readable. Hard-linked old inodes are
  not overwritten. The helper restores cwd on return in its isolated process.
- External labels are escaped once; filename format tokens remain literal.
  Filename line breaks become spaces and those entries receive Nop actions.
  Directory/menu names containing line breaks or NUL are refused. Generated
  reuse helper arguments preserve literal apostrophes, quotes, dollar signs and
  backticks through actual FVWM PipeRead. Configured command templates remain
  trusted FVWM commands; navigation still uses the shared command engine.
- The optional Go gate now builds/tests/stages this utility. The default-BSD-X
  driver adds a narrow `--directory-only` selection without repeating unrelated
  client checks. Man AsciiDoc, INSTALL and architecture navigation are updated.

## Affected qualification

All six existing local Linux/FreeBSD/OpenBSD AMD64/ARM64 environments pass,
using isolated build/evidence directories and offline Go settings. The accepted
native core/MFL binaries are reused for these X checks; installed ELF/script
identities, versions, executable modes and unchanged core/MFL hashes validate.
No new full C compiler/sanitizer matrix is claimed for unchanged C inputs.

| Coverage | Result |
| --- | --- |
| Registered tests | 36 successful entries across six targets; 30 Go package results |
| CLI/filesystem parity and controls | 624 cases across six built/installed Go utilities; another 104 cases execute Linux race-built utilities |
| Recorded utility contracts | 270 observations across build/installed Go/installed Perl runs; 108 apply to directory. Other observations retain the other utilities' baselines. |
| Pure driver/gate contracts | 84 cases: eight default-X controls and six native gate controls per target |
| Actual X | Twelve invocations: six nested-X built fixtures, two Linux installed Xvfb fixtures and four authenticated installed default-BSD-X fixtures. Forty-eight menu mappings and thirty-six dismissal operations. |
| Navigation | Real MissingSubmenuFunction callback and keyboard submenu entry, reuse parent display, literal-path state update, DynamicPopupAction reload and smaller child menu; all twelve fixtures pass |
| BSD authentication | Four clients without the private authority cookie are refused |
| Installed profiles | Twelve tagged stages; six Go ELF and six Perl identities match tested bytes and native architecture |
| Linux concurrency controls | Ten race-unit package results, two actual race-built utility suites and two vet runs |

CLI/filesystem cases include all sorting orders, executable files, hidden files,
symlinks, FIFO metadata, date/size/type formats, quoted and Unicode labels,
format widths/truncation, custom functions/actions, special directories and
reuse state. Native Go units also cover hidden-entry admission, output failures,
plain legacy state, private modes, atomic hard-link replacement, writable state
and directory refusal, symlink/FIFO/large-state refusal, invalid private requests
and cwd restoration. Live fixtures configure title/file Exec actions as Nop;
only submenu callbacks and owned navigation helper actions execute.

These are local/emulated affected results, not actual provider-native release
CI or physical BSD display/toolkit/compositor qualification. No publishing,
upstream notifications, credential changes or remote creation occurs.

## Documentation and retained failures

Linux AMD64 Asciidoctor produces the man page and static HTML from the same
source. Linux and FreeBSD ARM64 mandoc render them successfully; the transferred
BSD pages match the generated hashes. Active HTML loads no external scripts or
stylesheets. Mandoc lint exits 3 on inherited Asciidoctor `mso`/`LINKSTYLE`,
long-line and date/style diagnostics; this is not a clean-lint claim or a
renderer replacement checkpoint.

Initial source `fea5` included unrelated current shared-Meson registrations and
failed setup on missing launcher test inputs. Source `e4bb` composes only the
owned registration; native builds and units pass. Its broader fixture found
Perl-version diagnostic wording and an incorrect direct missing-submenu test.
The first was normalized only in the parity consumer; the second now enters
through a real parent menu. Intermediate `b9cf` then exposed a fixture action
extraction error with quoted paths. Focused recovery also caught an X inventory
race during menu destruction; a brief server grab makes the test inventory
coherent. Corrected navigation and dynamic reload controls pass the final
six-target checkpoint. Failed trees/logs remain preserved.

An evidence helper initially shadowed Python's standard `inspect` module and
was renamed. The first documentation text check rejected a wrapped `64 KiB`;
whitespace normalization fixes the check without changing the man source.
Initial rendered output and diagnostics are retained. These orchestration
failures are not hidden as passing production results.

Evidence: `builddir/codex-2026-10-12/directory-port/`, including `scope.json`,
`source.tgz`, six `*-evidence.tgz`, six `*-identities.tgz`, two
`*-manual-review.tgz`, retained attempts and `validation-summary.json`.
