# Scratchpad window lifetime and overview refresh bounds

The Ubuntu 24.04 ARM64 run reused the exact build from
[PR387's workspace qualification](VERIFICATION-2026-11-23-WORKSPACE-CLOSURE.md).
It ran the revised fixtures in an isolated container with networking disabled.
All 16 Slate workspace cases and nine mixed-output cases passed with clean
process retirement. Earlier theme captures carry forward.

The X server accepted destruction and recreation of the same window ID
on one test connection. The old and new windows had distinct allocation
identities. The stale overview action returned `stale_target` and the
replacement kept its geometry. The scratchpad dismissed the original
window and later claimed and summoned the replacement without launching
another client. Dismissing the replacement returned focus to the prior
window. A preliminary fixture expected summon on an already visible
window. The corrected sequence checks dismissal before summon.

The same overview stayed open across scale and primary changes, unplug,
and replug. Each transition settled with all work accounted for and
started at most 32 refreshes, the guard for this fixture workload.

| Transition | Invalidations | Started | Published | Rejected |
| --- | ---: | ---: | ---: | ---: |
| Scale and primary change | 9 | 4 | 2 | 2 |
| Unplug | 8 | 5 | 3 | 2 |
| Replug | 15 | 12 | 8 | 4 |

The same-XID gap for #221 is satisfied in Linux. Affected support-profile
acceptance stays open. #223 has a checked refresh count for this scene.
Its CPU/RSS envelope and remaining support-profile evidence stay open.
#222 still requires physical docking. No screenshots were taken.

| Receipt | SHA-256 |
| --- | --- |
| Workspace tools | `1c99874f0dd2ed85e8dec9aff54559d2cd438df0ab812d3a3ab74f2d9331c8de` |
| Mixed output | `67739eadac78d6d727e152d5664c951814f62535cb962879eed2ba0e5c65e574` |
