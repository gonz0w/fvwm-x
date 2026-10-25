# M5 POSIX handler installation cleanup

Status: focused verification and independent source review pass; integration
pending. Owner: M5 / CLEAN-01 #34 and FD-01 #7. Broader obligations stay open.
Authorized PR240 merged as `8602b3c42631ec16c313893b8dffc7962efdb8c5`;
canonical signature/DCO and reviewed tree
`89518690e32c2d0cc73370448d51f7f8d6d8b499` verified. Branch retired and
Project/labels Done. The open count after merge is 87; #35/#36 stay completed.

## Complete bounded family

Ten installers now retain their existing POSIX body directly: core, Animate,
Auto, Buttons, Event, Form, IconMan, Ident, Pager and Script. Remove only the
HAVE_SIGACTION outer guards and inactive signal/siginterrupt fallback paths.
Preserve every active body, lexical block, handler, mask, flag and installation
order. Inner SA_RESTART/SA_INTERRUPT guards, core deinstallation signal calls
and Buttons' separate SIGPIPE-ignore call remain unchanged.

Remove the exclusive sigaction/siginterrupt optional probes. A required native
Meson link check includes signal.h with POSIX feature visibility, initializes
struct sigaction, and calls parenthesized sigemptyset/sigaction. Parentheses
require declarations even when implicit-function warnings are nonfatal; calls
also require successful linking. Missing APIs produce the explicit required-POSIX
diagnostic. This makes existing supported-platform requirements explicit, with
no replacement selector, new dependency or configuration migration. INSTALL.md
states the native API requirement. No handler error-policy or shared-installer
rewrite occurs.

## Compiler and configure evidence

Private evidence: `builddir/codex-2026-10-25/m5-signal-install`. Capture actual
production compiler commands and preprocessing for all ten consumers before
editing and after the grouped configure/build. All nonblank generated-config
directives match exactly except removal of HAVE_SIGACTION/HAVE_SIGINTERRUPT.
Nine consumers have identical active C tokens. IconMan differs only in the
source-position argument of its unchanged globals.managers assertion, from
line 303 to 287. The exact condition, filename and every other token match.
Line directives and whitespace outside literals are excluded; no meaningful
handler token or flag is normalized away.

Five real native Meson controls extract the exact production requirement block:
ordinary declared/linkable APIs pass; unavailable signal.h, missing declarations,
a renamed missing sigaction symbol and a renamed missing sigemptyset symbol each
fail with the required diagnostic. The missing-declaration header supplies
matching types/constants without functions, proving the parenthesized calls do
not rely on warning promotion. These are controlled compiler/linker refusals,
not runtime vulnerability findings. All five controls pass in 0.730 seconds.
The full native repository configuration also passes the same requirement.

## Focused runtime checks and retained passes

One existing Linux ARM64 GCC group build compiles core and all nine modules
together. The shared fvwmsignal unit and actual FvwmEvent transport test pass.
Event's retained native-pipe controls cover configuration/active SIGTERM,
EOF/shutdown and normal command processing. One actual Script X fixture passes
open/closed stdout termination and cache recovery. Its open/closed exits take
0.128/0.125 seconds in this fixture; cache payload remains old::old:recovered
with one original execution. No UI rendering/geometry change needs screenshots
or a fresh theme set. No speed improvement is claimed.

The first parity harness expected the two retired definitions to disappear
without their adjacent blank lines. Meson removes those blank lines as well,
so that assertion fails. Preserve its log; compare exact nonblank directives
and the unchanged active token streams instead. No source or build change follows.
Only the comparison and previously unrun runtime checks continue. The five
configure controls and successful group build do not repeat.

Twelve changed source/build/test inputs match host/guest hashes and modes;
eleven selected executable identities are retained. The canonical private
archive is read through all 62 payloads and gzip EOF/CRC.
[Machine receipt](VERIFICATION-2026-10-25-M5-SIGNAL-INSTALL.json) records exact
source, config/token differences, commands, retained failure, runtime and archive
identities. Repository/link/whitespace checks and separate final evidence review
complete signed author-DCO delivery.

## Scope and qualification limits

No active memory, callback or process-lifetime code changes. Retain
[PR240's sanitizer ownership evidence](VERIFICATION-2026-10-25-M5-POSIX-CONSUMERS.md)
and [PR239's BSD shared-primitive evidence](VERIFICATION-2026-10-25-M5-WAIT-OWNERSHIP.md)
where source is unchanged. Those are historical prerequisites, not fresh passes.
No automatic fresh sanitizer/BSD/architecture/install/renderer matrix, native-CPU
or release acceptance is claimed for this token-preserving cleanup.

Supported Linux/FreeBSD/OpenBSD behavior and FVWM configuration syntax remain.
Broader #34/#7, interaction, redraw, resource and release owners stay open;
do not reopen #35/#36 or #93's superseded unsupported-7.9 cause investigation.
No new feature, generic signal installer or unrelated portability retirement.

Requested Astra/high analysis, Sol/high implementation and separate Sol/high
review follow project routing; runtime model metadata remains unverified.
Periodic reference is PR238 at 2/5 after PR240, 3/5 on this merge; no comparison due.
