# DATA-01 tree-query ownership — 2026-10-06

Status: narrow repair implemented; focused policy and scoped sanitizer checks
pass. Full qualification remains pending for the next batch. This change is
excluded from the frozen FIFO/stack/output source.

`GetEqualSizeChildren` promised NULL output on failure but returned without
initializing it. It also acquired the XQueryTree child list before rejecting a
parent depth, visual or colormap mismatch, leaking the allocated list. Its actual
consumer is FvwmButtons' swallowed-window background update.

The controlled baseline reports one outstanding child array for a rejected
parent depth. A separate original-helper run fails the stale-output assertion.
The repair initializes the output before any failure and validates the parent
before acquiring the list. Valid matching children, wildcard selectors and their
XFree ownership stay unchanged; no new cap or configuration rule is introduced.

`tests/test_window_children.c` exercises failed parent attributes/tree queries,
each parent mismatch, filtered/valid children, wildcard selectors, a disappeared
matching child, empty results and an empty tree. Allocations are counted by owned
Xlib test wrappers. The independent Linux ARM64 development build passes; Clang
ASan/UBSan with leak detection passes; focused native FreeBSD/OpenBSD ARM64 probes
pass without modifying the frozen guest source. Permanent Meson registration is
added and passes in the independent Linux development build. Broader platform
and actual-X qualification is pending at the next coherent checkpoint.

Inputs, before/after hashes, logs and BSD focused archives are under
`builddir/codex-2026-10-06/tree-query/`. Linux baseline and scoped artifacts are
retained in the independent guest. The baseline uses one instrumented call, not
an end-to-end latency or exploitability claim. Existing actual-X FvwmButtons
coverage will run at the combined checkpoint.

The remaining tree-walk audit is open. In particular, the recursive
`fvwmlib_client_window` currently has no caller in the repository search; that is
an inventory observation, not proof of a runtime defect or approval to remove an
external API. Existing transient-loop validation was found and must be preserved
in STACK-01's identity optimization.

The independent repair is now locally qualified in the [2026-10-07 combined
deadline/tree checkpoint](VERIFICATION-2026-10-07-MODULE-SYNC-TREE.md), with exact
source identity and Linux/BSD/default-server/optional/sanitizer evidence. Earlier
focused statuses above describe that report’s original scope.
