# M2.2 settings policy and native UI plans

Status: **focused checks passed; ready for PR review**. Refs #23/#7/#15; broad
owners and M2 remain open. [Decision](../decisions/M2-SETTINGS-POLICY-2026-10-23.md),
[source-bound receipt](VERIFICATION-2026-10-23-M2-SETTINGS-POLICY.json).
Base is merged PR190 `2dbfa136f78e7ba70638b56b4b7b46305808daad`.

## Result and ownership

One reusable Go package owns the ordered preference registry, strict values and
scopes, original/schema-1/schema-2 decoding, schedule policy, saved/effective
candidates, query metadata and native settings/appearance/editor/schedule/preview
dialog plans. Superseded Python policy/builders are removed. Requests are pure
bounded data; the settings operation cannot read/write files or call backend
capabilities. This introduces no new toolkit, native C or module protocol.

The resident controller keeps private-file reads/locks/fsync/replacement,
revision checks, saved/effective generations, editor/draft and core preview
lifetimes, module replies and publication. File replacement remains its commit
point; no lost worker reply can turn a committed file into a reported refusal.
Twenty-four write/lifetime/capture/protocol owner implementations retain their
exact source text. Their delegated policy is qualified by the retained tests,
not inferred merely from equal owner text. Whole UI plans validate before any
publication or editor retirement; malformed plans preserve an active draft.
Only immutable scalar validation results use a 128-entry cache. No settings
snapshot, revision, generation or plan is cached.

FVWM3 configuration names/arguments, module override precedence, schema formats,
scoped controls and explicit Run/shell behavior remain supported. JSON object
key presentation order is not a byte-for-byte preference-file promise; stale
revision checks bind the actual serialized bytes. No new desktop feature or
complete Python retirement is claimed.

## Combined focused qualification

| Check | Result |
| --- | --- |
| Linux ARM64 Go normal policy | 20 combined cases / three packages; final composed coverage 21 |
| Linux scoped race | Same combined 20 / three packages; final composed 21, parallel result ownership exercised |
| Go vet | All three affected packages pass |
| Controller policy | Original 71 plus five new plan/failure/cache cases: 76 pass |
| Installed-worker process boundary | 12 cases pass; no policy/client matrix repetition |
| Native X | Full desktop/settings/preview/checker/services, directed settings IPC and operations: three accepted runs |
| Theme examples | Slate dark, Dune light and classic 9x15/borders/Form configuration; scoped editor/cancel and menu pass, six captures |
| Installation | Fresh paired tagged stage; adjacent lookup without override and private save/load revision round trip pass |
| Private test-output target | All 15 outputs compile after storage recovery, including desktopsettings.test |
| Offline cross-build | Linux/FreeBSD/OpenBSD ARM64/AMD64 workers compile; no six-native-execution claim |
| Manual and hygiene | Changed manual renders; final make check/lint and gofmt pass |

Independent cases cover strict scalar/text types, integer-token distinction,
schedule boundaries, legacy/schema rejection and nested duplicates, finite
request fields/depth, scoped drafts and literal callbacks, complete-plan
refusal before publication, bounded scalar cache, candidate/wallpaper/registry
ownership and inability to reach backend capabilities. Actual native checks
retain stale revisions, overrides, preview rollback/confirmation/save failures,
draft transfer, restart and native consumer cancellation. Power process tests
use mock backends. Observer unexpected X errors are empty.

Seven old/new command/metadata comparisons bind adoption to merged PR190:
settings menu 87 commands, appearance menu 17, scoped editor 36, schedule 14,
runtime preview 19 and saved preview 21 match exact order after replacing fresh
identity tokens. Query metadata matches. Expected policy values and failure
controls remain independently asserted; this comparison is additional evidence.

The full UI fixture captures the existing grouped editor, previews and reports;
three extra settings profiles capture menus and effects editors at 624×229,
624×229 and 804×221. The classic profile applies controlled font/border/Form overrides to the
frozen shipped baseline. Exact profile commands and unmodified PNGs are in the
canonical host capture; these are profile deltas, not an external theme audit.
Visual inspection confirms readable controls within the display. This is a
representative smoke set, not wider theme/scale/accessibility acceptance.

## Corrections, provenance and cadence

The first 2385-input checkpoint passes policy/race/vet, 76 controller cases,
installed boundary cases and existing desktop workflows. Its new theme-capture
step fails because an existing PNG-path local shadows the capture function.
Rename only that fixture local. Repeat affected UI and finish skipped checks;
three themes and both native IPC consumers pass. No product repair is required.

The private all-package test-output link then exhausts guest disk space. A
serial memory-backed temporary attempt still needs final-output space. Preserve
both failures and all earlier passes. Retire only hash-matched completed source
archive/capture duplicates after full host archive/payload verification: about
106 MB from current captures and nine older source archives. Source trees,
incremental builds, caches, installed stages and disks remain. Serial linking
with memory-backed temporaries finishes all 15 outputs, then the skipped manual,
installed lookup and paired observations pass. No passing product/UI suite is
repeated for storage. Command time is about 35 + 46 seconds for the two runtime
attempts and under two seconds for the storage-only attempts.

Final diff review catches a bounds-check ordering slip in the duplicate-check
cleanup: directory count was checked after copying/scanning values. Restore the
original early count check and remove only the later duplicate. An independent
old-helper control and a direct-Go regression require count refusal before value
inspection. Repeat only the ten-case affected worker package in normal/race,
helper build, final adjacent stage and invalid-count CLI check: about two command
seconds, all pass. Six final workers cross-build offline. No settings/UI, file
commit or lifetime logic changes; their qualified results remain valid. Final
composed normal/race coverage is 21 top cases per profile; reruns are not new
cases. Private all-package output compilation stays bound to its earlier source;
final affected worker tests/helper compile the corrected source separately.

Final Go literal layout is whitespace-only. Scanner token/literal streams,
including implicit semicolons, are identical for the three formatted source
files; no source directives/cgo are involved. The receipt maps tested and final
hashes explicitly, retaining the qualified runtime/cross-build evidence. Final
status/report changes use repository/link/diff checks. All five canonical
captures pass gzip EOF/CRC, payload hashes/modes and expected source checks;
failed attempts remain represented. No private artifacts are committed.

Portable maintenance harness inputs are unchanged. Retain PR190's 181 passes /
15 environment skips (196 total); do not rerun that suite for this product
policy batch. Native C binaries and config.h keep their exact identities.
No new C/sanitizer, BSD default-X or native-CPU execution is inferred.

## Cost and remaining work

Seven alternating paired prepare/query/editor observations use fake publication,
not full-session or repaint timing. Median is **0.041 ms before / 1.970 ms now**,
about **1.93 ms additional process/JSON cost**. This is a migration/ownership
checkpoint, not a speed or total source-size improvement. The future resident
Go controller can import this package directly. Full-session startup/RSS/idle
wakeups/input comparisons remain required before #23 closes. Periodic browser
comparison PR186 is 3/5 after PR190, 4/5 on this merge; it is not due here.

This will be the fifth implementation merge since integrated PR178. Coverage
review retains M1's six-target utility/compiler/install gate and PR190's affected
FreeBSD transport/worker evidence. New policy has no OS-specific I/O, native C,
new dependency or rendering primitive. Linux native/theme coverage and offline
six-target compilation address this batch; a counter does not trigger a matrix.

Next: move resident configuration/dispatch/generation/editor/preview lifetime
coordination together, then the configuration source walker and separately
reviewed native screenshot capture. Complete Python-free installed desktop,
full native support/default-X and session acceptance at the owner/milestone
boundary. Features remain paused; #23/#7/#15 are not closed by this PR.
