# Bounded directed core state queries — IPC-02

Status: implemented; combined affected cycle qualified, pending PR review. Base: merged PR156,
`45a164ac22b6278a490b233c421ceaaf44190d41`.

Batch committed topology, per-monitor workspace/cache observations and paged
configured global bindings. Add internal module-only `Send_Query` through the
shared command engine, using existing directed MX_REPLY packets. Request data
is reduced to fixed validated tokens; reply JSON is hex-fragmented and bounded.
No packet layout, new endpoint, dependency or configuration migration. Shipped
Perl command metadata follows the additive command; packet constants do not change.

Core state remains authoritative and single-threaded. The topology query reuses
the committed snapshot and existing serializer, with a lossless string epoch.
Workspace queries sample the retained core monitor collection, viewport/grid,
scale and static/dynamic work-area caches. They perform no X query, window walk,
recompute or publication. Ambiguous all-zero areas are explicit null/unknown,
and freshness is cached. These views are not one atomic X-server transaction.

Global binding pages copy at most 16 records after at most 4096 skips. Revision
changes conservatively on every binding command; the 48-hex token includes a
core incarnation and never-reused revision. Exhaustion disables generation
queries rather than reusing a token. Menu/placement and per-event matching stay
outside this subset. No pointers are retained across commands or configuration
mutation. Literal actions are data only and preserve configuration bytes through
existing JSON UTF-8 presentation policy.

A core reply is at most 32768 decoded bytes/128 ordered chunks, sharing the
existing eight slots and absolute five-second deadlines. Settings retain their
8192-byte/32-chunk bound. One assembly implementation owns both lifetimes, with
separate producer prefixes and packet-type admission; timeout/cancel/disconnect
free once and late serials cannot complete reused IDs. No domain execution or
rollback claim is introduced by cancelling a wait.

The cycle also reproduces a provider startup race on slower targets: requests
can overtake initial module configuration. A typed readiness fence returns
`provider_not_ready` before END_CONFIG_INFO, preserving all state; only the
fixture's read queries retry with a bounded deadline. This is a named IPC-02
correctness prerequisite, not a new general maintenance pickup.

This third feature batch closes the shared integration cycle with accumulated
PR155/156 settings and lifecycle changes. Run one affected six-target checkpoint,
relevant alternate/minimal profiles, local portable checks and scoped sanitizers.
Full install/default-X/milestone/release and native-CPU/hardware/toolkit checks
retain their own boundaries. No visible UI is selected; later consumers keep
actual-X screenshot review. IPC-02 remains open for broader owners/actions,
service operation protocols and shared-core generations/preview recovery.
