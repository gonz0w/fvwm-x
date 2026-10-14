# Font shadow work — approved

Owner: SCRIPT-01 / resource lifetime and rendering, with shared font consumers.
Status: approved, implemented and qualified on 2026-10-06.
The user approved 4096 positions by default with adjustability up to 65536.

The accepted Script geometry policy checks X11 representation. It does not
limit how much work a representable font can request. Font shadows are shared
by the core and modules through Flocale.

## Concrete evidence

A bounded, non-rendering Linux ARM64 profile uses the production font loader
and shadow-position iterator. The ordinary `fixed` font remains 13 pixels high
with a 48-pixel `Geometry` label in all three cases:

| Center shadow size | Observed small-case shadow positions | Square iterator field |
| --- | --- | --- |
| 16 | 1088 | 1089 |
| 64 | 16640 | 16641 |
| 32768 | Not iterated or rendered | 4295098369; exceeds signed int |

The large case only loads the font and checks the arithmetic in a wide type.
It does not execute the overflowing iterator or paint a large image. Small
cases stop after at most 20000 iterations. Evidence and reproducible source:
`builddir/codex-2026-10-06/font-shadow-review/`.

The accepted geometry baseline calculated the Center square in signed
int counters. This is distinct from the checked font metric additions and
Xft/core-font width accumulation in the geometry batch. No complete font safety
or responsiveness claim is made.

## Approved and implemented contract

- Check shadow arithmetic before loading/committing the font. Preserve ordinary
  directional and Center effects, offsets and rounding.
- Default to at most **4096 shadow positions per text draw**, excluding the
  ordinary foreground pass. Use a finite build-time override from 1 through
  **65536**. Zero must not disable the limit.
- Count actual directional steps and Center border positions with wide checked
  arithmetic. Avoid a quadratic scan of skipped inner positions. Do not narrow
  a large square or cumulative step count to int.
- Refuse a font whose shadow exceeds the work limit with a useful diagnostic.
  Preserve a previous usable font on a runtime failure. Startup consumers must
  handle refusal cleanly through their existing startup/default-font policy.
- Audit every affected font-load consumer before enabling the shared limit.
  The core's menu/default font setters already check load failure. FvwmForm and
  several module setters require explicit ownership/failure review; introducing
  a limit must not turn a recoverable refusal into a NULL dereference.

For example, Center size 31 without an offset uses 3968 shadow positions and
fits the recommended default. Size 32 uses 4224 and needs an override. Center
size 64 needs at least 16640. Ordinary small theme shadows remain supported.
Configurations above the default would change behavior. AGENTS.md requires an
explicit user decision for that compatibility change.

This bounds repeated shadow positioning, not total glyph work, font matching,
font caches, individual library calls, X server storage, or large window pixel
areas. Those budgets, wider Unicode/font-set processing, asynchronous SVG wait
integration and modal servicing remain separate tracked work. No new mandatory
library or recursive core event dispatch is proposed.

## Required gates

Pure count/boundary tests and representable legacy-position comparisons; native
font-load/refusal/ownership tests; core menu/default-font and affected module
integration/recovery checks; scoped sanitizers; full six-target Linux/BSD gates,
staged binary identity and default BSD X. Native Linux release sanitizer CI
remains a separate gate.

## Accepted checkpoint

Checked counting, direct border iteration and shared consumer refusal/ownership
are qualified: 600 full-suite executions, 105 staged identities and all installed
Linux/default BSD X checks pass. Limit extremes 1/65536 and scoped local
sanitizers pass. See the [verification report](../verification/VERIFICATION-2026-10-06-FONT-SHADOW.md).
Broader font/resource work and native Linux release sanitizer CI remain open.
