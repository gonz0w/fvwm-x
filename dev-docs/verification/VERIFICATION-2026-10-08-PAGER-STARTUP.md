# Pager desk range, startup defaults and desk lookup

Status: four related stories are strictly focused-qualified on normal/scoped
Linux and native FreeBSD/OpenBSD ARM64. Full
qualification remains pending behind the frozen canonical text and numeric
checkpoints. Artifacts: builddir/codex-2026-10-08/pager-startup-production.

The user approved 4096 displayed desks per instance, with a finite build-time
allowance through 65536. `pager-max-desks` exposes that option. Checked range
preparation preserves reversed endpoints, negative-to-zero correction, one
argument, current-desk mode and high desk IDs. It refuses unrepresentable or
excessive counts before masks, X acquisition or per-desk allocation. The manual
records the inclusive limit and override. This is the approved configuration
change; no additional configuration migration is proposed.

New Pager monitor records start with a positive, root-sized one-page desktop.
If initial configuration lacks DesktopSize, viewport/highlight creation can use
valid page dimensions. Complete core geometry replaces these defaults normally.
A defensive one-pixel fallback handles unavailable initial extents.

A shared desk-index helper subtracts in a wider type and returns an out-of-range
sentinel before indexing. Pager grid, visibility and move entry paths use it,
including updates between INT_MIN and INT_MAX desk IDs. It does not bound every
other geometry operation or repair nested drag lifetimes.

The actual startup control reproduced an additional high-ID style bug: both
startup loops requested FindDeskStyle(i), so Desks[0] used the style of desk zero
when a high desk was selected. Both now use the absolute selected desk ID.
Initial Linux and native BSD ARM64 failures, exact runners, frozen source,
policy passes and available actual logs remain under
initial-high-desk-style-failure. The initial fixture did not reach the missing
DesktopSize case because its earlier high-ID mapping assertion failed.

Focused scope: five C units and five actual Pager entries per profile. The new
actual driver checks three early refusals, ordinary mapping, INT_MAX desk mapping,
missing DesktopSize mapping, reflected INT_MIN/INT_MAX/current desk updates and
normal EOF. Pure controls cover exact default/override boundaries and desk-index
arithmetic. Scoped pure leaks are enabled; actual X-process leaks are disabled.
The complete693/65/73-file payloads validate source hashes/modes, exact runners
and passing outcomes. The approved65536 build override and default restoration
also pass; restored default Pager bytes are identical. The source contains1402
files/fifteen owning paths, SHA256
`a653e54fe77d7f4b6d7f0a32472acc657c305019f0d1c47f419d0866c28b38ed`.
One prepared combined checkpoint requires1776 standard entries/eight profiles,
120 installed identities,284 BSD default-server drivers,optional227/207 and
scoped113 (38 pure leak-enabled,75 actual leak-disabled). Those extensive counts
remain requirements until dispatched and validated. Native Linux release
sanitizer CI and physical/toolkit/compositor checks remain separate.

The undispatched monitor/numeric30be and startupa653 candidates now share one
combined twelve-story owner checkpoint, replacing their separate proposed
extensive matrices. The complete final a653 source passes normal/scoped Linux
and native BSD ARM64 nine C +eighteen actual checks per profile; complete837/
137/157 payloads validate exact source/modes/runners and outcomes. The source
changes34 owning paths from canonical text3c37. Previous startup-only and numeric
focus remains retained separately. The combined1776/120/284 optional227/207
scoped113 matrix follows canonical text3c37 qualification. No30be full matrix
is dispatched. This consolidation follows the user’s batched cadence and does
not replace the required acceptance or release coverage.
