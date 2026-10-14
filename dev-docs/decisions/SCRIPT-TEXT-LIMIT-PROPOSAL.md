# Interactive TextField limit — approved

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

The TextField/List maintenance batch repairs text offsets, atomic edits,
selection ownership and list drawing. It retains the existing representable
text range. A finite interactive text limit is a separate compatibility choice.

## Accepted behavior

Apply the existing `*FvwmScript: ValueLimit bytes` setting to the resulting
TextField title after a typed insertion, middle-button paste or keyboard paste.
The default is 1 MiB excluding the terminator. The existing configurable override
of 1–2147483647 bytes remains available; no new setting or runtime library is
needed.

For example, with `ValueLimit 128`, a title of 127 bytes accepts one ASCII byte.
A further byte is refused. A paste that would produce 129 bytes is refused as a
whole. Replacing a selection credits the removed bytes before checking the
result, so a replacement that fits is accepted.

Refusal keeps the title, insertion point, selection and viewport unchanged,
reports a diagnostic without including pasted content, and does not send a
successful `SingleClic` notification. Later input still works. Deletion and
edits that shrink an already oversized title remain possible. Checked integer
representability applies in all cases.

`MemoryLimit` continues to cover the constant/variable table. This proposal
adds a per-title limit; it does not count widget titles in that table's aggregate
budget or cap whole-process memory. Clipboard transport keeps its existing
32 KiB bound. Multiple widgets, rendering cost and font/image resources remain
separate work.

## Compatibility decision

The original approved [value-storage scope](SCRIPT-VALUE-LIMITS-PROPOSAL.md) explicitly
excludes widget allocations. Before this extension, interactive typing could grow a TextField
past ValueLimit even though parser and interpreted string assembly are bounded.
Applying the same limit closes that gap but can reject previously accepted
large edits.

[AGENTS.md](../../AGENTS.md) says: “A specific compatibility break requires an
explicit user decision.” The user approved this extension on 2026-10-05. ValueLimit now applies to
interactive TextFields with the refusal behavior above. Implementation is
complete; full cross-platform and installed default-X verification passes. See
the [report](../verification/VERIFICATION-2026-10-05-SCRIPT-TEXT-LIMIT.md).

## Implementation and verification gate

Reuse the checked replacement helper, and publish cursor/selection changes only
on success. Check exact size, first excess, selected replacement credit,
shrinking, aliased insertion and unchanged state after refusal in native policy
tests. Exercise both legacy and opt-in keyboard modes with configured small
limits in the real module on Linux and every default BSD X server. Large-title
rendering still needs measured algorithm work; a size limit alone does not
establish responsiveness.
