# Font measurement and Script modal servicing

Accepted implementation contract, 2026-10-11. Owner: SCRIPT-01 / LOOP-01.
The user requested completion of remaining font/modal work, preserving FVWM
configuration files and avoiding new external dependencies. The configuration-only
[compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md) applies.

MainLoop owns one packet reader, monotonic periodic clock and session continuation.
Modal widgets borrow this state. Each service pass reads at most one module
packet and runs at most one due periodic callback. An executing script block or
servicing callback cannot reenter the reader. Normal X dispatch and typed expose/
configure collection have finite event budgets. Mouse waits wake within 100 ms;
held pointer/repeat fallback uses 20 ms and preserves its elapsed-time deadline.
An Xlib call or an explicit script command remains indivisible; these wake bounds
do not claim to preempt such a call or a slow external X server.

Changes to an active widget's font, title, geometry, values, visibility or colors
cancel the old interaction before its cached state is reused. The operation
snapshot compares an explicit font/title resource revision
as well as pointer/geometry/value state: allocator/cache address reuse cannot
prove unchanged ownership. Each successful resource replacement advances the
widget revision before releasing the old resource. Non-input X work
uses the existing dispatcher, without recursively invoking another widget.
Escape, termination and module EOF cancel waits. Existing command/directive names,
arguments, bindings and script functions remain usable without migration.

Module selection acquisition keeps a private requestor, matching reply predicate
and total deadline. Optional service checks permit cancellation while preserving
unrelated queued X events. Existing callers without service retain the original
API. Script callbacks are not interleaved inside an executing instruction block.

UTF-8 offsets count full Unicode scalars. Malformed bytes advance individually;
core STRING16 presentation uses U+FFFD for malformed bytes and supplementary
scalars. Stored strings are unchanged. Font-set measurement accumulates bounded
wide-character runs using persistent locale conversion state. Non-ASCII bytes
must pass configured charset conversion, even for a one-byte measurement.

This adds no GUI library, worker thread, Unicode database, configuration directive
or arbitrary font-count ceiling. Grapheme editing, shaping, IME and broader
resource budgets retain their separate owners. Qualification status is in the
[font/modal report](../verification/VERIFICATION-2026-10-11-FONT-MODAL.md).
