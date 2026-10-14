# Module packet read progress, counts and termination

Status: focused Linux normal/scoped sanitizer and all five QEMU target overlays
pass. Combined platform qualification belongs to module-client1351. Existing
FVWM configuration and valid native packets remain usable.

Module-side reads now retain partial progress across nonterminal EINTR. A shared
read wrapper uses the existing atomic termination mechanism so retry cannot
reenter a blocked read after termination. The ordinary blocking module read
contract remains; no new blocking call is added to the authoritative core loop.
The core's incremental receiver is a separate owner.

Declared packet word counts are checked against header/minimum and maximum
before conversion to body bytes. A bounded prior-source owned-pipe control
confirms that one interrupted read returns NULL while a complete packet remains
available. A second header-only control observes an excessive word count
accepted after byte arithmetic wraps. It inspects only the returned header;
no consumer or body access follows. Both normal and leak-enabled sanitizer
controls pass. These are parser/transport observations, not exploitability claims.

The permanent unit checks fragmented start/header/body reads with three
nonterminal interruptions, exact payload bytes, a real owned pipe, minimum and
maximum valid packets, truncated EOF, preserved hard errors, preexisting
termination and five invalid word counts. An owned child blocked in a real read
cancels through the common termination handler; parent observation is finite.
Linux normal and scoped Clang ASan/UBSan each pass three pure checks with leak
checking and three affected actual-X checks with the established scoped
leak-disabled policy. Actual session, ordinary configure/picture packets and
independent display ownership remain covered. Five QEMU overlays compile and
link the actual reader/signal owners and pass the unit on Linux AMD64 and BSD
AMD64/ARM64; object replacement is checked before linking.

Seven complete focused archives validate hashes/modes/EOF: twelve Linux focused
files, thirty per QEMU overlay and twenty-one prior-source control files. Initial
test-name compile diagnostics are retained; the fixture identifier conflicted
with an existing typedef and was corrected without production repair. Earlier
interruption-only source/results remain retained. Evidence:
`builddir/codex-2026-10-07/module-client-production` and
`builddir/codex-2026-10-07/module-read-baseline`.
The final four-story source has 1351 files/ten owned paths, SHA-256
`d1968f4e70729d63266b6cacc8b94b8151d7d51811b83756f5af874b218091db`.
Native release CI and physical/toolkit/compositor evidence remain unrun.
