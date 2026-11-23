# Workspace scratchpad and overview qualification

Ubuntu 24.04 ARM64 ran both focused scenes against product source
`8ef2b1844ad6a9eac0525f55bfddec05e675e585` with the two fixture changes
in this delivery. The build used the pinned Go SDK and private PNG source
inside a disposable container. The scenes used Xvfb and Xorg dummy
respectively. Earlier theme and platform receipts carry forward.

- The 16-case scratchpad and overview scene verified that the launched
  terminal process was xterm. The retired window and its same-title
  replacement had distinct allocation identities. An action using the
  retired row returned `stale_target`, and the replacement kept its
  geometry. The test did not force reuse of the X window ID.
- The nine-case mixed-output scene observed automatic overview publication
  while it stayed open through scale and primary-output changes, unplug
  and replug. Its final rows agreed with committed monitor roles. From the
  initial settled view to the final settled view, diagnostics recorded 31
  invalidations, 19 refresh starts, 10 publications and nine rejected
  results. These are observations from one run. A workload limit and
  CPU/RSS envelope remain open.

Both scenes returned zero, retired their owned processes and saved no
new screenshots. Physical docking remains open under #222.

| Receipt | SHA-256 |
| --- | --- |
| Scratchpad and overview | `f02a71fd4d4ee0c2206cb33342fa630299a19587ab9fb19c5b639b98c15346a8` |
| Mixed output | `ea40290d45e253eaee0afbf62ca10ab7908cd9be883769e37e2dc926a2707927` |
