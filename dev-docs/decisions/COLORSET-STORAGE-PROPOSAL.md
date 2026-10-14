# Colorset storage and limits proposal — 2026-10-05

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: user approved 4096 distinct sets per process and a shared build-time
override up to 65536, and copying set zero for undefined module IDs, on
2026-10-05. The implementation and final local matrix are complete; see the
[final verification report](../verification/VERIFICATION-2026-10-05-COLORSETS.md). No production colorset
storage change is included in the completed MiniScroll release pass.

Before this migration, core and module allocators grew a dense array through
the highest colorset ID. Defining Colorset 100000 allocated 100001 slots; the
core also initialized each preceding slot with color resources and X calls. The manual
explicitly permits large IDs, so a small maximum ID would break valid themes.
Both allocators also compute the requested count using signed integer addition.
These are source findings, not a high-index allocation or crash experiment.

## Recommended policy

Preserve existing numeric theme IDs and use sparse storage: allocate only a
colorset actually defined or referenced. Use a shared checked API for lookup,
creation and iteration. A sorted index of IDs with separately owned records
provides binary-search lookup, stable record addresses and bounded insertion
work without another runtime library. Broadcast and cleanup iterate existing
records in ID order instead of walking unused gaps. Audit all direct indexing,
transparency macros and native/Perl packet consumers during implementation.

Bound each process to **4096 distinct colorsets**, including its default set.
Defining or referring to an already allocated ID does not consume another slot.
At capacity, reject a new ID with a diagnostic before replacing an assignment
or releasing its prior resources. Existing sets remain usable and editable.
Use one build-time setting for the core and bundled modules; allow a larger
limit up to 65536 for unusually large palettes. Keep invalid/overflowing IDs
rejected before allocation. No runtime environment override is proposed.

Preserve core black/gray defaults and module cloning of set zero when an
undefined ID is first materialized. Preserve monochrome defaults based on the
requested ID. The migration needs coverage for these behaviors, root
transparency, theme updates, module startup/broadcast, and resource lifetime;
this proposal does not claim the migration is already compatible or complete.

## Decision

| Default distinct-set limit | Base record storage on the measured Linux ARM64 ABI | Tradeoff |
| --- | --- | --- |
| **4096 (recommended)** | 851968 bytes, about 0.81 MiB | Smaller finite default; themes using more distinct IDs need a higher build setting. |
| 65536 | 13631488 bytes, 13 MiB | Allows much larger palettes; permits more allocation and update work. |

The measured colorset_t is 208 bytes. Figures exclude index/allocator overhead,
images, gradients, pixel arrays and X-server resources; this is a table-count
limit, not a whole-process or rendering-memory limit. The probe used sizeof
only and did not execute large colorset commands. Evidence is saved under
builddir/codex-2026-10-05/script-release/colorset-size-policy.json.

A finite count changes existing behavior for configurations that materialize
more distinct sets than the chosen default. AGENTS.md requires an explicit user
decision for that compatibility change. Large isolated IDs remain supported
under either option; the limit concerns how many IDs are used.

## Implementation and verification after the decision

Implement checked common storage policy with native unit coverage for sparse
IDs, existing-set updates at capacity, overflow, ordered iteration, stable
addresses and refusal without state loss. Migrate core and module callers in
reviewable batches; validate before committing caller state. Replace the
manual's dense-allocation warning with the actual count policy only after the
migration is complete.

Run theme/default-resource and module packet checks under nested X on Linux,
then Linux GCC/Clang and FreeBSD/OpenBSD AMD64/ARM64 local gates and default BSD
X checks. Include a high isolated ID and a small test-configured count limit
without allocating an enormous table. Native Linux sanitizer CI remains a
separate verification responsibility. Broader image and rendering resource
budgets remain open.

## Additional compatibility decision: undefined module IDs

A real-X comparison of the previous installed core/module and the sparse
candidate confirms an additional appearance change. With `Colorset 0 bg red`,
`Colorset 100 bg green` and a module widget using an undefined `Colorset 50`,
the previous widget reports gray (`bebebe`); the sparse candidate reports red
(`ff0000`). Before, the dense core materialized set 50 as a side effect and sent
its gray defaults at module startup. Sparse startup sends actual records only,
so the module's ordinary undefined-set fallback copies set zero.

User approved consistently copying set zero for undefined module IDs. This keeps
the implementation and module startup simple, preserves explicitly defined
colorsets, and removes the dependency on unrelated larger numbers. A theme
that depends on the implicit gray gap would need to define that set explicitly.
Core references continue to materialize their existing black/gray defaults.

The alternative is preserving legacy implicit-gap colors with additional
startup default metadata and fallback handling, including monochrome defaults
and separately built module compatibility. That work is not implemented in
the current candidate. Both the 4096 count and this appearance decision are approved under
AGENTS.md's instruction: "A specific
compatibility break requires an explicit user decision."

Local evidence: builddir/codex-2026-10-05/colorset-storage/gap-default-control.json.
The comparison records both core/module binary hashes and verifies those
binaries remain unchanged. It uses a separate temporary X server for each
variant. Its first report attempt used an invalid literal filename in the
script; that fixture is corrected to a filename variable. The observed failure and
correction are recorded in gap-default-fixture-correction.json; its original
helper log was overwritten by the successful comparison. No production binary is replaced by the comparison.
