# STACK-01: preserve Layer state on restack storage refusal

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-07. Production adoption with explicit
buffer ownership is locally qualified from the 1328-file checkpoint.
This is separate from the approved deep parent-walk invoked-target fallback.

## Concrete behavior and candidate

A finite actual-core control has four synthetic managed records with real X
frames and a parent/child/grandchild group. A Layer command needs a small
restack array. Forced refusal of that allocation makes the current core exit
with status 1 through its existing fatal allocator. Ordinary allocation passes.
This is a resource-failure control, not a demonstrated desktop compromise.

The private candidate prepares one fallible array before Layer changes even
its temporary planning state. Capacity comes from a checked full-ring slot
count and covers the selected layer/subtree. On failure it logs and returns,
keeping existing layers, authoritative links, server order, flags and scratch
state. No module/EWMH layer change has been published at that point. A later
normal Layer command succeeds. Ordinary selected-record ordering, visible icon
rules, parent resolution, rank planning, decoration/EWMH publication and focus
remain in their existing owners. Layer owns the prepared array, lends it
synchronously to the existing restack consumer and frees it exactly once; an
empty selection frees it early.

The change adds one linear full-ring count and can allocate more slots than the
selected subtree needs. The temporary array stays operation-local, shares the
existing frame/icon policy and adds no registry, cache, thread, library, timing
limit or new configuration. Same-layer commands remain allocation-free.
Raise/Lower/Restack and new-window allocation handling remain separate future
owners; this proposal changes only Layer's restack storage failure.

## Recommended contract

If Layer cannot prepare its restack storage, refuse that Layer operation,
report the failure through existing diagnostic logging and preserve the current
layer/stack state. Keep the WM running and permit subsequent commands/retry.
The alternative is to retain the current fatal allocation behavior.

[AGENTS.md](../../AGENTS.md) requires: “A specific compatibility break requires
an explicit user decision.” Existing process termination becomes a failed
command with a living desktop. The prior parent-walk approval establishes a
different failure target and does not decide Layer array refusal.

## Review and adoption

Candidate and exact patch: `builddir/codex-2026-10-07/layer-storage-decision`.
Normal/scoped Clang ASan/UBSan controls verify baseline exit, candidate preserved
public/internal state, unchanged real-server order and successful retry. All 36
ordinary icon/layer/new-window/ring cases pass with the private candidate in
both profiles. Focus/decor/EWMH wrappers leave those policies to existing real-
client regressions. Approval precedes production adoption, native ownership
coverage, full live-client lifetime/publication checks and a coherent combined
platform checkpoint. Private results are not production acceptance or native
release CI.

Production uses `fvwm/stack_window_buffer.c/h` with explicit prepare/finish
ownership. Focused and combined Linux/BSD/default-X/optional/scoped sanitizer coverage passes;
[production verification](../verification/VERIFICATION-2026-10-07-LAYER-STORAGE.md)
records the exact source and remaining native release/physical checks.
