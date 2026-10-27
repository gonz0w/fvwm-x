# Browser continuity and action closeout

Owner: Cam. R1 FILE-ACTION-01 #65 / FILE-VIEW-01 #69. Reuse the accepted
collection, directory/action provider, installed browser handoff, native Form
layout and committed bindings. This checkpoint finishes in-place view switching,
keyboard discovery and missing-program failure evidence. It does not implement
file mutations, persistence or a second browser/service.

## Native layout publication

`SendToModule ALIAS ListLayout NAME EXPECTED NEXT CELL_UNITS MAX_COLUMNS`
uses the existing Form string-command channel. The exact prepared versioned
viewport must have EXPECTED published, no pending content transaction, and NEXT
above every admitted serial. Existing ListViewport cell units and 1..8 column
bounds apply; row layout remains within the existing 32-row bound. Parse and
prepare candidate state/geometry before publication. Refusal changes nothing.

Content identity, selected bitmap, focus, range anchor and old top entry survive;
row alignment/edge clamping may place that entry in the first visual row. An
intentionally offscreen focused entry does not force scrolling. The existing
Form/input/list windows stay owned and unchanged, so unsent input remains native.
Drawing, pointer hit testing and child geometry share the accepted reflow owner.
Fixed personal Forms do not opt in automatically.

The layout's new minimum must fit the actual current frame. WM_NORMAL_HINTS is
not an acknowledgement that the WM resized a parent. Refuse a too-small frame
before mutation; after real enlargement the caller may retry. Ordinary accepted
resize clamping remains unchanged. Minimum hints preserve other original flags.

The browser requests a new native generation and then observes actual
`ListStatus ... selection`. Only NEXT with no pending/refused update commits the
new observed mode. An exact unchanged-generation/no-pending reply is an explicit
normal refusal: keep the usable old mode, drafts and binding, report enlargement
needed, and allow retry. Other malformed/stale/lost observations use existing
explicit recovery. Provider actions cannot claim `applied:true` for a refused
view. Content refresh commits before a layout request if both change together.
Old callback generations cannot act after an accepted switch.

## Native keyboard actions

Additive Form `Button ... Alt-X` accepts one ASCII letter/digit and normalizes
case. The existing X11 Mod1 convention identifies Alt; Control/Mod4 combinations
are excluded. Shift/Caps variants normalize letters. A configured match invokes
the first defined matching button through the existing command path before
input/list editing. Unmatched events, existing caret/function shortcuts and
personal Forms without Alt shortcuts preserve their prior behavior. This does
not remap keyboard modifiers or redesign button traversal.

The browser displays unique Alt mnemonics for filtering, sort/reverse, selection,
Home/Up/Back, Go and Cancel, plus Alt-1..4 for configured actions. F2..F8, Enter
and Escape remain. A static View button and updatable observed-mode label avoid
a general mutable-caption API. Literal action arguments and target revalidation
remain with the existing file/action owner, not labels or shortcut text.

## Acceptance and remaining owners

One combined affected Linux incremental build and policy/race/native checkpoint,
with Slate dark, Dune light and enlarged text; retain original failures and real
screenshots. Scoped Form policies run with ASan/UBSan/leak detection; existing
native input/button/fields/geometry clients cover compatibility. Actual missing
and non-executable opener/terminal/configured-program cases must report failed,
reap workers, avoid duplicate side effects and recover explicitly.

#65 closes only when all five frozen rows link accepted source/profile receipts
and current affected results. #69 remains open: the actual automatic module-scale
acceptance under #51 is not established by enlarged fonts. Keyboard, clipping,
scale and optional assistive/hardware claims stay distinct. #66's storage/backend
comparison and other R1 owners retain their finite work. Existing installation,
platform and dependency contracts do not change; full release/default-X gates
are not repeated or silently counted as fresh passes.
