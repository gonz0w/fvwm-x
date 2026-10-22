# Atomic grouped button updates — BUTTON-02

The complete bounded per-module update contract is locally qualified for
maintainer review. `ChangeButtons` validates an entire group, prepares its
strings and icons, commits all values, then reconciles focus and repaints once.
Refusal leaves button state and pending activation intact. Legacy `ChangeButton`
keeps its incremental behavior. This completes BUTTON-02 on merge; CFG-01 is next.

This checkpoint stays together because parsing, resource ownership and final-state
focus repair must form one transaction. It does not add settings, layout changes,
a new toolkit, a compositor or a configuration migration.

| Contract | Accepted behavior |
| --- | --- |
| Bounded validation | At most 8192 bytes, 32 leaf buttons and 128 changes; exact values/selectors, existing palettes, no duplicate properties or aliased targets. Grid bounds are checked before cell-index arithmetic. |
| Prepared resources | Strings and picture-cache references are operation-owned until commit; a late refusal releases all prepared resources. Explicit new normal palettes are resolved before icon loading, independent of option order. |
| Startup recovery | Replacing a failed normal/active/press startup icon releases its still-owned filename. The freed legacy dash placeholder is cleared, preventing a stale pointer during later replacement. |
| Final-state application | Commit every target before repairing focus/held activation, then repaint at most the 32 targets plus a replacement focus. IDs, actions and module/window identity remain stable. |

The syntax, quoting, limits and examples are in the primary
[FvwmButtons manual](../../doc/FvwmButtons.adoc). Atomicity is logical module
state, not a simultaneous X display frame or a whole-WM transaction. Resource
loading remains synchronous inside the isolated module under existing image
limits; a work budget cannot preempt a blocking resource call.

The tested candidate is `6cd5c588f0477650f20f2f68660c354bb2e7d888`, tree
`1c49c6aab3b9af01d37d0e546765f0e611d79ca9`, based on merged PR149
`aa0f4b99e880dcc81fede493ca3f295d90241b89`. Its 2152 source inputs are bound to archive
SHA256 `ae1128f714e8d3daaace89e634ce1b048cf02b388d0e0975e4ca6e524224862f`. Subsequent publication changes only
README/tracker/report/index metadata; compiled source, tests, build definitions
and the manual retain these qualified bytes. The
[machine receipt](VERIFICATION-2026-10-22-BUTTONS-ATOMIC.json) records each target,
profile, source/archive, configuration, selection and installed identity.

| Qualification | Result |
| --- | --- |
| Fresh affected checks | 106 passes: Linux GCC/Clang on AMD64/ARM64, FreeBSD and OpenBSD 8.0 on both architectures, plus Linux ARM64 software/minimal profiles. All registered Buttons consumers are included; minimal omits four unavailable session/service/transport registrations. |
| Retained foundation | 2332 unaffected PR149 standard entries retained after exact configuration, source-scope and unrelated installed code/data checks. Composed standard total 2420; selected optional total 18. These are not 2438 freshly executed checks. |
| Install identity | Ten fresh stages; 150 ELF, 77 compiled catalogs and 700 other data targets verified. Every installed consumer except FvwmButtons remains identical to PR149, including all ten core WM executables. |
| Actual X behavior | Ten staged sessions and four private authenticated default BSD displays each pass ten named feature groups. Invalid first/middle/last groups, aliases, palettes, resource refusal, startup recovery, final focus, held keys, stable actions/window, legacy behavior and unrelated-client typing pass. |
| Sanitizers | Five source-bound guest ASan/UBSan entries pass with leak detection and no diagnostics. An unchanged instrumented control reproduces 81 leaked bytes in three allocations; the corrected startup recovery passes. |
| Server/VM cleanup | Four default servers exit 0 and refuse unauthenticated clients. Both final OpenBSD QEMU jobs exit 0; temporary private keys are removed. |
| Portable/manual | Portable foundation: 177 passes / 14 environment skips (191 registered), plus final documentation checks. Asciidoctor man generation and mandoc rendering pass; lint diagnostics exactly match merged main. |

Original failures remain preserved: the initial command-prefix collision, the
optional-XPM fixture assumption, the OpenBSD auxiliary PNG directory collision,
AMD64 per-pixel Python/FFI fixture timeouts, and the unchanged-module ownership
control. The packed-image comparison preserves every rollback pixel while
avoiding roughly 1.6 million per-pixel FFI calls. A fixture-only pending
coordinator was stopped before dispatch to include the coordinate-bound fix.
Incomplete attempts were not installed or promoted to qualification; final
corrections reran affected checks rather than unrelated full suites.

Canonical captures and signed-source maps are under
`builddir/codex-2026-10-22/buttons-atomic-updates/`, with final acceptance in
`release/`. Fully passed guest capture/archive duplicates are retired only
after canonical payload/hash/mode checks. Source trees, incremental builds,
install stages, performance references, VM disks and canonical evidence remain.
Automatic approval review declined deleting failed-run guest copies because
those diagnostics might still be relevant; those copies remain too.

The due [PR144→149 performance comparison](PERFORMANCE-2026-10-22-PR149.md)
uses existing binaries. Latency ranges overlap, RSS is similar, and code/data
costs are recorded without a universal speedup claim. PR149 is the next reference
at 0/5 subsequent merged implementation checkpoints; this feature PR is unmerged.
Further performance tuning and unselected general maintenance remain deferred.
Native-CPU sanitizer, physical hardware/toolkit/compositor and release acceptance
remain separate. Inherited manual lint is still DOCS-01-owned.
