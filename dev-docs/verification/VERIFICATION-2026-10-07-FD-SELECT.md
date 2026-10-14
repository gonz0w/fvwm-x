# Checked ICE/session readiness admission and watched records

Status: focused Linux normal/scoped sanitizer and native FreeBSD/OpenBSD ARM64
checks pass. This source joins argument growth, shared-library cleanup and finite
descriptor width in the next combined batch. Existing FVWM configuration remains
usable; no new library or configuration limit is added.

ICE listeners, accepted ICE connections and the core session connection now use
one shared preparation helper: validate the descriptor against fd_set's range,
then apply checked close-on-exec. Refusal retains caller ownership so the existing
constructor cleanup closes its endpoint/connection. Generic inheritance protection
continues to support descriptors above FD_SETSIZE for poll-based consumers.

The library's watch callback may report borrowed connections beyond those owned
by these constructors. Readiness construction/dispatch skips unrepresentable
watched descriptors without closing the borrowed connection inside its callback.
The existing close callback still releases the watch record and pending entry.
Source review found these range assumptions; no old out-of-range FD_SET/FD_ISSET
operation was executed and no exploitability claim is made.

The new native unit checks negative/upper/extreme descriptors before any flag
operation, both representable endpoints, each injected flag failure, preserved
errors and a real owned pipe. The generic helper's high-descriptor policy remains
covered separately. The actual session probe captures the installed ICE watch
callback and supplies three bounded synthetic connection-number samples. It
checks readiness sets stay unchanged, dispatch performs no borrowed connection
operation, and closing each synthetic watch restores the original set. These
samples are metadata controls, not high-numbered requests sent to an X server.

Final Linux normal/scoped Clang ASan/UBSan profiles each pass sixteen pure and
twenty-five actual-core checks. Pure units enable leak checking; actual core runs
retain the documented leak-disabled scope. Real XSMP registration, early/normal
disconnects, inheritance and authentication cleanup still pass. Both native BSD
ARM64 overlays compile six changed production owners and pass the range/flag unit
and four real-session/watch outcomes. The link audit verifies every requested
core/probe/unit object replacement against the actual Ninja context and retains
the executed runner. Three complete archives validate hashes/modes and compressed
EOF: twenty-four Linux focused files (including the preceding policy-only scope)
and fifty per native overlay. Final checks needed no production repair or
deadline extension.

Frozen final source: 1347 files, eight owned additions/changes from width1346.
Archive SHA-256:
`c166c863186375177ef7cdc08ea700ae9912eef20ad54fa3c837ff42949cfecc`.
Evidence: `builddir/codex-2026-10-07/fd-select-production`.
The initial policy-only source/results remain separately retained. This covers
shared range/flag policy, watched records and ordinary real-session ownership;
forced high-numbered live ICE/core connection admission and broader X/readiness
owners remain separate follow-up work. Combined qualification and native release
CI/physical/toolkit/compositor checks remain pending.

Combined local qualification now passes under descriptor1349: 1336 standard
entries/eight profiles, 120 staged identities, 132 default BSD X drivers,
optional 172/152 and 55 scoped checks (22 pure/33 core). Seven archives validate.
The [descriptor report](VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md) retains
the original OpenBSD display refusal and Linux AMD64 Xorg teardown failure with
three unchanged affected passes each. Native release CI/physical evidence remains
separate; this does not claim every initial run passed.
