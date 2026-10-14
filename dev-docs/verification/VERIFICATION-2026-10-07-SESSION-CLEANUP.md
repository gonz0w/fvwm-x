# Repeated terminal authentication cleanup

Status: focused Linux normal/scoped sanitizer and native BSD ARM64 checks pass.
Combined platform qualification belongs to the next module-client checkpoint.
Existing FVWM configuration remains usable.

Terminal authentication cleanup now clears its owned entry array and pathname
pointers after release. Repeated calls skip the already completed external helper
operation. Successful first cleanup and the existing listener/session lifetime
remain unchanged; this is not a full session shutdown/reinitialization redesign.
Source ownership review identified retained released pointers. No prior repeated
release or exploit demonstration was executed.

The actual session probe calls terminal cleanup twice. Its driver requires the
completion marker and exactly one authentication-install and one removal helper
call, while preserving registration/disconnect, borrowed-watch, inheritance,
literal-path and file-cleanup controls. Linux normal and scoped ASan/UBSan each
pass the affected real-session test. Both native BSD ARM64 overlays pass those
controls. OpenBSD's initial core cannot open :0 before reaching cleanup; its
original logs remain retained and three sequential unchanged affected checks
pass at original inner deadlines. A separate display-allocation reproduction
is recorded in the display-owner report; exact identities from this initial
failure were not retained.

Three complete focused archives validate hashes/modes and compressed EOF:
eight Linux files, 58 FreeBSD and 122 OpenBSD files including original diagnostics
and affected recovery. The first focused source has 1349 files/three owned paths,
SHA-256 `009f5ff7184b00615b78e0298c56318798b104cb27499042000b3bad9201f2af`.
Evidence: `builddir/codex-2026-10-07/session-cleanup-production`.
The later combined source also passes affected session checks. Native release
CI/physical evidence remains separate.
