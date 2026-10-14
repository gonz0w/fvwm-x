# Prompt, protocol metadata and utility foundations — 2026-10-11

Owners: PROMPT-01, PERL-PROTO-01 and MAINT-02; fixture preparation under
UTIL-01/RUNTIME-GO-01. Status: strictly locally qualified at final `397c`, with combined
baseline `adc5` retained. This is not a release certification.

## Source and scope

The combined archive is
`builddir/codex-2026-10-11/prompt-metadata/source.tgz`, SHA256
`adc5441867ddbbb4eaed4cbe24e84d11ccfc7ef0947788acf502fa9cc8d65dfc`.
It contains 1414 files and 27 owning paths, composed onto the accepted final69af
font/modal snapshot. The shared tests/Meson file includes only this batch's new
prefix plus that accepted test graph; unrelated shared-workspace changes are
not claimed as qualified. Hashes, modes, owning diff and executed runners are
retained beside the archive.

The JSON/peer-fixture three-path correction is frozen in `json-final-v2/source.tgz`, SHA256
`90d5ffaaa0b48e536b33ace2dde22de533bc3157b11d9fe0abfc2bb1815597f4`.
It changes only the transport implementation/test and controlled-peer fixture.
JSON validation now preserves the original record and accepts only JSON's ASCII
whitespace; a pre-validation Unicode trim previously accepted invalid records.
The fixture accepts an oversized stream's peer-side broken pipe/reset only with
the exact client size-refusal diagnostic. The intermediate `d5b8` archive and
OpenBSD fixture failure remain retained. Full results for unchanged native code
are reused; final Go binaries and affected checks are qualified separately.

The four related stories are:

1. Prompt transport and lifetime repair. A standard-library Go package owns
   one reader, serialized complete newline writes and idempotent shutdown.
   Writes have a five-second deadline; invalid local commands are refused
   without closing an otherwise usable session. JSON records are incremental,
   bounded and checked for valid UTF-8/object syntax. EOF interrupts the UI.
   One-shot commands complete the write before exit; ordinary MFL commands have
   no execution acknowledgement. Interactive history remains, and Quit uses
   local confirmation instead of the absent ConfirmQuit script.
2. Native/Perl contract repair. All ten extended event values match Module.h.
   MAX_XMSG_MASK covers the named event bits. Echo decodes three native words
   before its string. Handler matching strips the shared extended category
   marker; real Echo/Reply integration exposed the previous cross-delivery.
3. Checked metadata adoption. Committed Constants.pm/Commands.pm come from the
   authoritative native header/command table and .current-version, with strict
   Perl, warnings, preserved exports/POD and deterministic epoch zero. JSON
   companions and input hashes support later Go/Lua consumers. Old developer
   entry points delegate to the same generator and require a candidate output
   directory. Normal builds use committed metadata and need no generation.
4. Utility parity preparation. Fifteen byte/output observations cover CLI
   help/version/error behavior, ordering, hidden entries, quoted/Unicode names,
   symlinks, executable classification, xlock formatting and conversion
   preservation/overwrite refusal. Generated menu actions are not executed.
   These record current behavior; they do not certify every legacy output as
   desirable. No utility or FvwmCommand runtime port is claimed complete.

No FVWM configuration migration or new mandatory dependency is introduced.
Existing Go requirements/vendor versions remain unchanged. Perl retirement,
remaining Go ports and optional Go dependency/advisory review retain their
separate owners.

The closing startup correction is `handshake-final/source.tgz`, SHA256
`397cd39eea71bd7059e03d1948f85ec0299fbed7931569d85c79635bd09ecc32`.
Its 1415 files add one main-package regression test and fix the classification
of clean EOF when the profile channel is already closed. The reader can publish
both profile and EOF before startup selects; either selected channel now gives
the same result. Missing profiles and later incomplete records still fail.
Only five paths differ from combined baseline adc5; native C/Perl/utility inputs
are unchanged. All nine affected Go profiles, Linux race/vet controls and four
default BSD servers are repeated for this source, without another full matrix.
The complete accepted 90d5 evidence and `completion-json-v2.json` are retained.
A host test initially used a sandbox-inaccessible default Go cache; the explicit
workspace cache passes the main-package and transport race checks.

## Focused evidence

Host Go transport race tests and vet pass. The controlled-peer CLI fixture
passes ten cases, including fragmented/coalesced JSON, peer EOF while input is
held open, malformed/incomplete/oversized records, local refusal with a usable
session, full one-shot writes and Quit confirmation. Native Linux ARM64
configuration/build succeeds; transport, client, metadata, utility and MFL
stream controls pass. Corrected actual-MFL integration, native Go race/vet and
two affected metadata-generator tests pass.

Retained development attempts include a host test invoked outside the Go module,
a sandbox refusal of the fixture's temporary Unix socket (the authorized local
fixture then passed), a build-tree executable path mismatch, the configured
Perl wrapper's non-executable pre-install mode, and the actual extended-handler
cross-delivery finding. The integration fixture resolves paths and applies the
wrapper's documented 0755 install mode to an exact-byte temporary copy.
Initial BSD tool validation used version-suffixed Python platform names;
normalization was corrected and extracted files verified before reuse.

## Combined gates

Eight standard compiler profiles and Linux ARM64 minimal/software profiles run
against the combined archive. All six targets pass strict source, mode, runner, registration and install
validation. The combined baseline passes 1872 standard and 404 optional entries,
with 159 installed code identities and 99 catalog comparisons. The final
correction passes 24 standard and three optional affected checks, four Linux
Go race/vet controls and nine exact installed Go identities. Eight installed
fixtures pass on the four default BSD X servers: FreeBSD scfb on both
architectures, OpenBSD ARM64 wsfb with the root fallback, and OpenBSD AMD64 vesa.
The three scoped C sanitizer checks also pass. Twelve complete acceptance archives
validate 1414 baseline and 1415 final source hashes/modes and installed bytes.

The exact counts, source composition, archive hashes, manual hashes, metadata
input provenance and owning-checkout checks are in `completion.json`, with
`validation-summary.json` and `handshake-validation-summary.json` beside it. The
evidence directory is `builddir/codex-2026-10-11/prompt-metadata/`. Initial
collection/validation before a transfer finished exposed an incomplete archive;
final collection uses atomic publication, and all final gzip streams validate
through EOF. No test or source result is inferred from an incomplete transfer.

Private Go verification toolchains for five guests are bootstrapped from this
machine's existing Go installation, hashed and run natively. No guest system
package is installed or source requirement raised. Linux ARM64 retains its
existing Go 1.22.2; the other five guests use isolated Go 1.27.0. The first private
SDK omitted vet: BSD ARM64 full suites passed the other 227 checks. The SDK was
completed and only the failed test retried at its unchanged registered deadline,
using the production build cache. Original logs, retry commands and log hashes
are retained. All three AMD64 cold Go tests also timed out without output;
cache preparation and affected retries preserve that observation, rather than
claiming it was a test failure or silently increasing the deadline. Linux's
Clang profile reuses its completed native default test cache.

Linux AMD64 Clang's full suite exposed one inherited Script text-list failure:
the first snapshot after an injected key still contained the pre-edit value.
The fixture crosses X11 input and module-command channels without an execution
acknowledgement. This is a timing hypothesis, not a proven cause. The initial
full log, stderr and artifacts are retained, and exactly three bounded isolated
controls use the unchanged source, registered command and deadline. All three pass; no source fix, unlimited retry or broader suite repeat is
inferred. The observation remains a CI-01 test-reliability follow-up.

Scoped native Linux ARM64 ASan/UBSan passes protocol-metadata, mfl-stream and
actual-MFL Prompt integration. Leak checking is enabled for the first two;
legacy whole-core leak reporting is excluded for the third. These are C-module
checks, not Go ASan evidence. Final Go race/vet controls are recorded separately.
The scoped runner's initial ambiguous Meson target failure is retained; it now
selects `fvwm3:executable` explicitly. Source behavior did not change.

The affected Prompt man page and optional HTML render using the existing Linux
Asciidoctor; native FreeBSD mandoc produces UTF8 presentation. Exact source and
output hashes are in `manual-result.json`. This is not a clean mandoc-lint claim
or the separately deferred manual/website redesign.

Native Linux release sanitizer CI, physical hardware/composed toolkit checks
and broader desktop/release requirements remain separate. No whole-project
security certification, utility port or Perl retirement is claimed.

The maintenance guide crosswalk is refreshed after qualification. Its two
status-cell changes are recorded in `documentation-delta.json`; production and
test inputs still match final397c. The archived guide retains its original
running-checkpoint state. No further platform run is required for status text.
