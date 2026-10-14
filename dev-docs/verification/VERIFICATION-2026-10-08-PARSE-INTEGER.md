# Shared integer, rectangle and percentage parsing

Status: three related stories are strictly focused-qualified on normal/scoped
Linux and native FreeBSD/OpenBSD ARM64. Full qualification is pending behind the earlier
frozen module checkpoints. Artifacts:
builddir/codex-2026-10-08/parse-integer-production.

An isolated executable linked to the unchanged actual shared library observed
four entry points accepting values above INT_MAX: integer count1, any-base
count1, rectangle count2 and percentage count1. Exact baseline source/library
identities, runner and observations remain under baseline-evidence. These are
unrepresentable numeric conversions, not valid supported configuration values.

The common integer/suffix parser now checks strtol conversion and C int bounds
before publication. It keeps token boundaries, quotes, partial-success counts,
consumed-token continuation, decimal signs and suffix rules. Any-base parsing
uses explicit C11 decimal/octal/hexadecimal selection to avoid newer libc binary
prefix differences. Suffix character classification uses unsigned bytes.

Rectangle parsing prepares both integers before publishing either output. It
preserves the existing arbitrary one-byte separator, signed second numbers and
ignored trailing text. Failed preparation leaves both outputs unchanged.

Percentage parsing preserves decimal-prefix input and a final p/P pixel unit.
It refuses out-of-range values, resets the value to zero on failure and keeps
prior units. It no longer mutates the borrowed token buffer. Shipped core cursor,
geometry, style, schedule and module consumers continue to use these helpers.
No command/directive or configuration format migration is proposed.

The new pure unit covers representable integer endpoints, hex/octal, quotes,
suffix case and historical suffix acceptance, rectangle separators, percentages,
partial token continuation, range refusal and previous-output preservation.
Focused scope is10 C +22 actual entries per normal/scoped Linux profile and10 C
+20 actual entries per native BSD ARM64 profile. Linux-only scheduler/interaction
harnesses remain in the Linux scope. Native Linux release sanitizer CI and
physical/toolkit/compositor checks remain separate. Complete259/119/137-file payloads validate exact1403-file source hashes/modes,
runners and passing outcomes. Source SHA256:
`9f175352c96863f4148407b548993aa917e3eeed89017bb9f86085617fd5fa54`.
The source changes only Parse.c, its new pure unit and the owning Meson test
block from a653; broader qualification is not yet claimed. Other numeric consumers and pixel arithmetic remain open.

A single three-story checkpoint is prepared at required1784 standard entries/
eight profiles,120 installed identities,284 BSD default-server invocations,
optional228/208 and scoped114 (39 pure leak-enabled,75 actual leak-disabled).
It follows qualification of the combined monitor/startup a653 source. The counts
are requirements, not passing extensive evidence; the focused source is unchanged.

The undispatched three-story parser checkpoint is folded into the related Pager
grid/scale batch. Final source6e01 is focused-qualified and requires one combined
1800/120/288 optional230/210 scoped116 matrix behind sourcea653. Earlier parser
focused evidence is retained; no separate9f17 matrix is dispatched. See
[PAGER-GRID](VERIFICATION-2026-10-08-PAGER-GRID.md).
