# Script packet admission and widget consumer cleanup

Status: four related stories are focused-qualified outside the frozen
readiness1366 checkpoint. Final normal/scoped profiles each pass six pure and
seventeen actual module/core entries; native BSD ARM64 actual overlays pass.
Packet-unit assertions are explicitly enabled for every build type.
No full gate for this component has been dispatched.

Script now checks the fields its handlers consume within the packet's declared
body. Configuration, command and Swallow name/class text require a bounded NUL
terminator after their three native words. Map requires its consumed window word.
Property changes require their three fixed fields; an active swallow change also
requires terminated text. Unused types retain their existing ignored behavior.
Short or unterminated bodies are logged and discarded; timer/session servicing
still occurs after that packet. ReadFvwmPacket remains the framing owner.

Fourteen empty widget ProcessMsg definitions and their repeated table assignments
are removed. The optional callback defaults to NULL, and the common dispatch
skips it; Swallow remains the one widget packet consumer. Its validated name bytes
are borrowed during comparison, removing an unnecessary allocation/copy. Existing
widget construction, input and drawing handlers remain unchanged.

Empty SendToModule commands and ChangeWindowTitle without a title are refused
without dereferencing a missing token. Empty quoted titles and existing title
matching syntax remain supported. No FVWM configuration rewrite, new mandatory
library or change to core X ownership is introduced.

Pure checks allocate exact body lengths and cover every permitted length for each
text type, the final-byte terminator, fixed records, conditional property text,
NULL inputs and the framing owner's maximum. Actual module checks send eight
groups of incomplete bodies or missing command arguments; each group must be
followed by a successful normal script handler. Existing periodic, empty/filled
Swallow, session, execution, storage and widget controls also run. No unsafe
previous-handler control or exploit claim is used.

Frozen candidate: 1369 files, twenty-three owned paths from readiness1366.
SHA-256: cd6b8e8d25d05ccf9a9e254448776d541be3bee676f2b034956cd26414858108.
Evidence: builddir/codex-2026-10-08/script-packet-production.
Strict focused validation checks all 1369 developer source hashes, thirty-seven
consumed native overlay hashes, exact executed runners and recovery records.
Compiled-symbol checks confirm all fourteen empty callbacks are absent while
Swallow remains. Three complete archives validate hashes/modes/EOF: 92 Linux
files and 142 per BSD target. Earlier source7307 and complete focused archives are retained under
before-assertion-build-flag. The first native overlay used pure-delay compile
flags for code including X headers; compilation failed before actual controls.
Its complete 88-file archive per BSD target and runner are retained under
initial-native-compile-template. The corrected overlay uses actual module
compiler/dependency flags, exact existing unit object keys, explicit assertion
flags and a copied live driver. Production code was unchanged by that runner fix.

Ten broader runners are prepared but wait for readiness1366 qualification and
predecessor archive validation. Required coverage is 1488 standard entries/eight profiles,
120 installed identities, 184 BSD default-X drivers (46 each), optional 191/171
and 78 scoped checks (28 pure leak checks enabled; 50 actual process/core checks
with leak detection disabled). These counts are requirements, not passing results.
Nonblocking transport, bounded X draining, total shutdown deadlines, arbitrary
high session admission and release CI/physical/toolkit/compositor checks remain
separate work.


The prepared Script-only full gate was never dispatched. After readiness1366
strict qualification, it joins three focused-qualified Form stories in the
[seven-story combined checkpoint](VERIFICATION-2026-10-08-MODULE-UI.md). The
Script-only future counts above are retained preparation history and superseded
by the combined requirements; component evidence remains valid for its own scope.
