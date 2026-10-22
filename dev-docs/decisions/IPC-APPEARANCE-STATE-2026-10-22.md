# Appearance policy and conflict observations — IPC-02 / CFG-01

Selected after merged PR161 (`9f95beda2217a4d69b76d8965b6a86a334127713`).
Batch three related observations: shared appearance-configuration freshness,
menu-style policy pages and decoration-definition pages. Include the reproduced
CopyMenuStyle opacity omission exposed by the actual-X consumer: a source at 73
was copied at 100; copying now preserves the source opacity for new and existing
destinations, including the shipped launcher. Configuration syntax stays usable.

Keep authoritative state in existing owners. A process-local conservative
revision tracks Style/WindowStyle/FocusStyle/removal, decoration definitions and
title/button/border/ButtonState, menu-style parsing/copy/removal, DefaultFont and
existing colorset activity. Hook the owners used by recursion/continuations;
do not add a second state authority, background scan, X acquisition or work queue.
Exhausted revisions disable tokens. Read-only queries do not advance them.

Borrow at most one fixed page of records during synchronous serialization.
Bound ordinal traversal at 4096, pages at 16, names/templates and existing native
reply budgets. Return configured values, including normalization units and
literal templates; no rendering, resource IDs, window assignment or face image
snapshot. Menu/decor pages share the conservative token, so covered unrelated
changes can invalidate them. Per-resource binding/colorset tokens stay separate.

A matching query is not atomic conditional application. Do not enable preview or
restore guessed defaults: conditional_application and preview_supported remain
false. These observations support conflict diagnosis; owned snapshots and safe
compare/apply/recovery remain the next CFG-01 handoff. SESSION/file/clipboard
acceptance stays with those domain owners. No general preview framework or broad
style rewrite is selected.

Use one reused Linux GCC checkpoint for policy, allocation-failure and actual-X,
plus scoped leak-enabled policy/MFL sanitizers. Include the existing native
desktop consumer/screenshots for the opacity fix. No new X API, installed path,
packet ABI or OS contract requires an early BSD/install gate. Shared platform
coverage is due at cycle closure; full owner/release acceptance remains separate.
PR160 is integrated reference; PR161 is 1/3 merged and this becomes 2/3 on merge.
Performance PR159 is 2/5, 3/5 after merge; no comparison or tuning is due.
