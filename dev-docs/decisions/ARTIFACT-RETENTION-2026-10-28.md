# Retain current work artifacts

Cam approved this storage-policy change on 2026-10-28: keep the latest build and
anything relevant to the next run instead of accumulating old test/build data.
The previous blanket preservation of host archives, source/build trees, install
stages and historical VM copies consumed shared-lab space without a current use.

The canonical [retention rule](../WORKFLOW.md#current-artifacts-and-retention)
now keeps current reusable profiles, active review/qualification evidence,
unresolved diagnostics and the next performance reference. Compact versioned
results and published screenshots remain; superseded raw artifacts can retire
without first producing a full archive. A short continuation record explains
why any older artifact is still needed and when that reason ends.

This replaces earlier retention wording, including the 2026-10-20 guest-copy
rule and blanket OpenBSD predecessor preservation. Earlier reports and the
finite PR297 two-copy exception remain historical records, not continuing
requirements to preserve their raw archives forever. Review, qualification,
signature/DCO, permission and other-owner boundaries remain in force. This
policy PR performs no bulk deletion, VM operation or disk expansion.
