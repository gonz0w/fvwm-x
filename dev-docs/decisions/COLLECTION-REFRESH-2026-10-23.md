# Atomic metadata refresh and collection recovery

Status: implemented; focused Linux policy/race and three real-theme native-X
checks pass. Broader acceptance remains open.

Owners: FILE-VIEW-01 / FILE-DATA-01 / FILE-ACTION-01. This batch follows the
closed PR167–169 affected platform cycle.

A shared metadata collection admits generation-checked transactions with at
most 256 removals/upserts, at most 4096 resulting entries and one explicit
complete resulting ID order. Validate the whole transaction before publication:
unknown/duplicate removals, duplicate upserts, remove/upsert conflicts, invalid
entries, duplicate/missing order IDs, over-budget work and stale/exhausted
model generations refuse changes without altering focus, selection, filter or
data. An identical snapshot can succeed without consuming another generation.
Surviving IDs retain selection. An optional opaque metadata revision (64 bytes)
lets an adapter signal changed observations without replacing stable identity.
No domain content belongs in this model or in revision tokens.

An admitted immutable snapshot can reconcile through a small transaction; a
larger change publishes one bounded validated replacement. Identical metadata
and order do not advance the model generation. Neither path performs filesystem
work or changes native module transport. The native Form remains a bounded
whole-view rebuild; in-place native updates, file watching and streaming provider
transport are later contracts.

When focus disappears, prefer the next surviving visible old neighbor, then the
previous one; otherwise focus the first new visible item. Preserve the filter
and surviving selection, including filtered-out IDs. Navigation to another
folder still clears domain state. Browser manual refresh adopts this policy and
reports added/updated/removed entry counts (not filesystem operations). Metadata
revisions derive from observed mode, size and modification time; they neither
contain file data nor replace the worker's action revalidation. An unchanged
refresh says so. Replaced folders and lost selections stay honest.

The ordinary Go metadata path is not protected clipboard content delivery.
CONTENT/RETENTION/STORE keep native protected memory and private transfer
ownership. SQLite, persistence, file mutations and watchers are not added.
