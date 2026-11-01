# Content lifetime foundation — source checkpoint

Owner: Cam. Scope: #63 / #71 native metadata policy; #74 default decision.
Base: merged PR327 `2c26920ee2354b0d68e16eea8d5e72a38ff84b6f`.
Branch: `codex/r1-content-retention-foundation`.

## Observed checks

- `make check`: pass; repository foundation/link checks only.
- Working and staged whitespace checks: pass at the recorded source checkpoint.
- Native macOS ARM64 C11 compile/link: pass with Apple Clang 21.0.0
  (`clang-2100.3.20.102`, target `arm64-apple-darwin27.2.0`). Two objects and
  their linked `test-content-policy` executable, with `-Wall -Wextra -Werror
  -pedantic`; assertions enabled. No test executable was run.
- This feature-independent pure-C compile uses a private minimal `config.h`
  with no feature definitions. It is not a configured Meson build, Linux/BSD
  support acceptance, install qualification or proof that registered Meson tests
  ran. No VM was started, no clean rebuild or new dependency was needed.

Private owner receipt:
`builddir/codex-2026-11-01/r1-content-retention/compile02/result.json`.
The receipt contains exact compiler argv, exits, logs and pre/post source hashes.
Its three compiled input hashes are:

| Input | SHA-256 |
| --- | --- |
| `libs/content_policy.h` | `e006c6c2b6c604e491c9ba1653d2a3d45a999aeb0cf29892743d22dbe717567a` |
| `libs/content_policy.c` | `5d7abdaf94ba7b1b74a0a4b773254f85c93d11220e20716e28db13c4533b2747` |
| `tests/test_content_policy.c` | `9f769d78a8f956f8ff5b30fc00beea1b8be89f0d5a5bcb2a957637e534bff6a4` |

Source review found that refused expiry reads must also advance the clock floor
to reject later stale time samples. The correction and authored regression are
compiled in compile02; compile01 is historical. These hashes are copied from
the actual compile02 receipt. Compilation is not
state-machine execution.

## Authored, pending execution

`test-content-policy` links the production implementation and authors admission,
overflow, hint refusal, immutable metadata, cancellation/late completion,
overlapping replacement, exact cleanup/lease reuse, expiry without a sweep,
pin/cap precedence, backward clock, stale epoch and nonwrapping-counter cases.
Meson registration is source-reviewed; runtime waits for the named clipboard
major-feature/shared checkpoint. Keep policy/sanitizer, real native owner/X
adoption and relevant support qualification pending. No capture/protected-memory/
payload/transport/UI service is implemented or qualified by this record.

No new runtime unit, native-X, sanitizer, theme, install, performance or
six-platform suite ran. No new UI requires screenshots. Performance cadence is
2/5 after PR327, 3/5 if this implementation merges. Existing Script Fontconfig
clean-exit failure, manager runtime gates and Q67 D1/D2 remain separate pending
or failed acceptance; no historical result qualifies these new sources.

## Roles and review

The lead implemented under the recorded fallback after two implementer agents
reached account usage limits; no retry/model substitution. The selected analyst
reviewed finite native-authority/ABA/retirement design. Separate reviewer source
review clears freeze03: all nine hashes match, no unresolved source findings.
Production C SHA-256 is `5d7abdaf94ba7b1b74a0a4b773254f85c93d11220e20716e28db13c4533b2747`;
authored test SHA-256 is `9f769d78a8f956f8ff5b30fc00beea1b8be89f0d5a5bcb2a957637e534bff6a4`.
Final signed delivery review remains required before merge. Requested Temper
routes remain as recorded in policy; actual backend model/effort metadata is
unavailable, so routing is not claimed as verified. An agent review is not an
independent human or GitHub approving review.
