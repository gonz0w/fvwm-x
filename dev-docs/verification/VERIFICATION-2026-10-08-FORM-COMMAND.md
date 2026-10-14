# Form command construction and refusal ownership

Status: four related stories are focused-qualified outside the preceding
argument/data matrix. Final code source1375:
`35670a58f9c908d974ce39601958d4bd60fb4120a17e292f3bb1e74d64cd2262`.

Variable identifiers are matched directly through their validated source view;
the fixed 256-byte temporary is removed. Existing whitespace normalization and
escaped-name spelling remain. Scanning stops at NUL and refuses incomplete
variable expressions. Output growth uses checked geometric character capacity,
passing element count/size once and retaining the existing int representation.
Allocation uses project helpers. No new output quota or substitution escaping
changes supported configuration bytes.

Refusal returns NULL with cleared length and a source position. Button handling
skips a refused command and continues following commands. A timeout constructs
its command before withdrawing the frame, so refusal leaves the form available.
Expansion, variable scans and item walks observe the existing termination flag;
button processing returns on cancellation. Recursive conditional expansion,
blocking shell callbacks and partial pipe reads remain separate owners.

Pure controls cover twelve ordinary byte results, normalized/escaped names,
1024-byte identifiers, 256-KiB literal output followed by small output, incomplete
expressions, representability refusal/retry and cancellation. The twelve bounded
ordinary cases also pass against the prior parser in normal and leak-enabled
scoped builds. No prior long-name or incomplete-input path is executed. Existing
unselected-choice and unknown-conditional byte results are preserved explicitly.

An actual Form module uses the long identifier, skips an incomplete button
expression, emits the valid preceding/following commands, and keeps its mapped
frame after refusing an incomplete timeout expression. Active commands and EOF
remain usable. The control sends only Nop commands through its fake producer.

The initial three-story source3814 passes Linux normal/scoped eight C policy,
two Python harness and 25 actual process/core entries plus both native BSD ARM64
overlays; complete 203/100/106-file archives remain under three-story-preparation-focus.
The fourth cancellation story gets affected final normal/scoped checks only:
two C policy and eight actual Form scenarios per profile. Both native BSD ARM64
final overlays compile Form/parser/helper owners, run both units and expanded
Form/harness controls. Final 142/100/106-file archives strictly validate every
payload hash/mode/EOF, 1375 developer identities, ten consumed native identities,
exact runners, bounded prior-source comparisons and recorded outcomes. Policy
leaks are enabled; actual module leaks are disabled. No Python instrumentation
is claimed.

The manual-only follow-up documents refusal and corrects earlier claims about
automatic input escaping and unselected-choice output to describe existing
bytes. Existing renderer/mandoc checks retain eighteen messages/status3 with the
new text visible. This is paragraph upkeep, not a full manual rewrite or clean
lint certification. Evidence: builddir/codex-2026-10-08/form-command-production.

These four stories join five storage/Input stories for one nine-story combined
checkpoint; no separate geometry/command matrix is dispatched. Wider qualification,
native release CI and physical hardware/toolkit/compositor checks remain separate.
Full layout, editing history/growth, recursion, shell and partial reads remain open.
