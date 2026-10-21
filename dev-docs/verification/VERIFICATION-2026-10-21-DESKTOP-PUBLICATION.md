# Desktop root publication — 2026-10-21

EWMH-01 R07/R09/R10/R12, with DATA-01 / INPUT-01. Base: merged PR145,
`1af7d380a1e3457c2b317cf1378cccba132f0764`.

Three related repairs validate root pager indices/formats before conversion,
prepare accepted count changes before publication, and replace mismatched
256-entry viewport/work-area arrays with one bounded effective-count snapshot.
Count requests that would shrink past the current or populated desktop are
refused; no client action/relocation or configuration migration is introduced.

[EWMH 1.5 root properties](https://specifications.freedesktop.org/wm/1.5/ar01s03.html),
sections 3.3/3.5/3.6/3.9, define dependent lengths and allow refusing count
requests. Timestamp zero remains accepted. Pure policy validates each record
count against 1024 desktop records and the server's XChangeProperty header and
payload capacity before allocation/indexing. The bound applies to publication,
not configured FVWM desktop numbers; an unrepresentable state retains the
preceding root snapshot and reports refusal. No request becomes a multi-property
atomic transaction. Existing global geometry remains; reserved-edge/page/dock
semantics and multi-monitor root authority are not qualified by array lengths.

Focused Linux GCC ARM64 and FreeBSD ARM64 pass one pure policy and three
independent real-X pager groups. Preserved PR145 installed bytes fail count
adoption, valid root current-desktop requests and initial dependent-array
lengths independently. The first focused configure failed on a new Meson
include-variable typo; source/logs are retained and only affected checks rerun.

Combined six-target/compiler/install/default-X qualification is pending.
Scoped Linux guest ASan/UBSan passes the pure policy and all three pager
groups, with sanitizer refusal exits distinct from expected startup failure.
The complete combined receipt will be recorded here.
The PR144 performance reference has one subsequent merged implementation
checkpoint (PR145); no comparison is due. Performance tuning remains deferred.
Native-CPU sanitizers, physical input/GPU/toolkits/compositors and final release
acceptance remain separate, as do broader EWMH/selection role requirements.
