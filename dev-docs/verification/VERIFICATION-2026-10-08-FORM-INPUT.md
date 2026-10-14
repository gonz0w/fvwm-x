# Form packet and text preparation

Status: four related stories are focused-qualified outside the frozen seven-story
Script/Form gate. Full qualification is deferred to its successor checkpoint.

| Story | Result |
| --- | --- |
| Declared packet bodies | Configuration, string and error messages require terminated text after three context words. Property messages require their three consumed words. Refusal returns to the input/timer owner; unused types remain ignored. |
| Empty lines | Shared newline trimming handles empty defaults, configuration, data and active messages without indexing before the buffer. Empty message text remains supported. |
| String ownership | Quoted strings and solid tokens stop at their terminator. Escaped characters/whitespace retain their established treatment; a trailing backslash is literal. Returned copies have explicit caller ownership. |
| Timeout preparation | Parse all required arguments before creating an item or occupying the timer slot. A refused incomplete timeout leaves a later valid timeout usable. Checked decimal-prefix conversion preserves the 0..99999 range. Raw quoted-command boundaries no longer depend on translated/unescaped output length. |

Final source: 1373 files, seven owned paths from module-ui1370.
SHA-256: a6aa669c8199ed28928303e438693731fe3e1dcef83bba11a86e36ae84e4d148.
Evidence: builddir/codex-2026-10-08/form-input-production.
Final normal/scoped profiles each pass seven pure and nineteen actual process/core
entries. Both native BSD ARM64 overlays pass the pure input unit plus ordinary
transport and actual packet/configuration recovery/timer controls. Four refused
body groups each precede successful ordinary Map/UnMap commands. Empty defaults,
configuration, active commands and message text recover. A valid escaped quoted
timeout command runs after preceding incomplete timeout directives are refused.

Complete final payload archives contain 137 Linux and 45 files per BSD overlay.
Strict validation checks hashes/modes/EOF, all 1373 developer source hashes, six
consumed native overlay source/header hashes, exact runners and actual records.
Pure units cover every declared text body length through the packet maximum,
property boundaries, owned string cases, cleared refusal outputs, escaped quoted
arguments and bounded decimal conversion. Existing descriptor-4096 controls pass
where permitted; OpenBSD inherited-hard-limit exclusion remains explicit.

Earlier source0e3 passes initial normal/scoped and FreeBSD controls. The subsequent
const-qualified wrapper (sourced4b01) fails native BSD and scoped Clang compilation
because the existing translation API declares a mutable input pointer. Original
source/logs and complete 44/8 native failure payloads are retained under
initial-legacy-gettext-signature; the scoped compiler failure/source remains under
initial-scoped-followup. No runtime sanitizer finding occurred in that failed
compile. The wrapper now documents the existing API's borrowed input contract;
no shared translation API change or warning suppression is made. Final checks
pass sourcea6aa. Prior successful source/history remains separately retained.

No supported configuration names, syntax or arguments require migration. Remaining
Form Input/Choice/Selection item preparation, geometry, command expansion and
partial packet-read ownership remain open. Existing timers do not preempt blocking
shell commands or unbounded queued-X work. Native release sanitizer CI and physical
hardware/toolkit/compositor checks remain separate.
