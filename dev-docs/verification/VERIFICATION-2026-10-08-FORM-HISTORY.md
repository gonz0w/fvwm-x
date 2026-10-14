# Form recall and editing ownership

Status: four related stories focused-qualified. Normal/scoped Linux two C and
nine actual checks per profile pass; complete native BSD ARM64 controls pass.
Strict 131/110/117-file archives validate source, runner and outcome identities.
Full qualification remains pending. No configuration migration is required.

1. A bounded recall-ring cursor replaces backward searches that could run past
   an empty table. Restart compares each field's own prior value, including when
   its ring wraps, rather than consulting the currently selected field.
2. Recall and Restart share owned value replacement. Byte size and capacity stay
   separate from the initialized legacy UTF-8 character count. Adequate storage
   is reused; a larger replacement is prepared before the previous value is freed.
   This removes the old recall allocation leak and retains room for a longer
   initial value after recalling a short history value.
3. Clear and truncate update byte size alongside the existing character state.
   Truncating at End leaves the value intact. Following insertion uses the actual
   remaining string length.
4. Insertion prepares representable byte/character totals and checked geometric
   storage before publishing counts or reallocating. Refusal preserves the field.
   This introduces no new text quota or configuration syntax.

The frozen developer source has 1375 files and eight owned paths, SHA256
`d93a25cb175dc3ba2ba29d92eb7de39c770ea9a3e806c464fa4d8bffd2fd1ee4`.
It stays outside the running nine-story processing acceptance sourceba7b.
The later manual correction is recorded separately. Artifacts:
`builddir/codex-2026-10-08/form-history-production`.

Affected Linux scope is two C checks (`form-input`, `form-command`) and nine
actual Form drivers per normal/scoped profile. Policy leak checking is enabled;
actual module leak checking remains disabled. No whole-module leak claim follows.
The history driver observes multibyte initial text, clear/truncate/append,
57 restarts, separate field rings, wrapping, 60 repeated recalls and following
insertion. The native overlay compiles Main, ReadXServer, Parser and input helpers
from the frozen source before linking donor libraries; eleven consumed source
files are checked. Acceptance/release CI and physical coverage remain separate.

## Retained development attempts

Initial source5f0a and observed sourceef25 passed the existing eight actual drivers
but failed the new recall assertion. The second fixture observed every saved
value. An isolated diagnostic copy confirmed the second ring reached count7 after
saving v54; the first ring remained at count1. The generated shifted dedicated
less-than key selected greater-than, so the fixture navigated forward to v6.
The corrected fixture uses Shift-comma/period and checks XLookupString resolves
less-than/greater-than before sending them. Production code did not change for
this fixture correction. The source archives and initial/observed raw logs remain.
A short-lived key-structure import typo during preparation is also retained; the
fixture uses the existing Key structure. A separate existing DEBUG-macro diagnostic
build exited during the first observation; it is not production acceptance evidence.

The existing UTF-8 recognizer is retained. Complete Unicode validation, broader
layout arithmetic, recursive command expansion, shell callbacks and partial-read
ownership remain open. This report does not close the entire Form audit.

The final d93a source passes strict focused validation. The later manual renders
with Linux Asciidoctor and FreeBSD mandoc; both sources retain the same 18
preexisting diagnostics/status3. This is a comparison, not a clean man lint gate.
