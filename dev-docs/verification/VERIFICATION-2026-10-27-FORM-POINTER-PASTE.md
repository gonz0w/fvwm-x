# Form pointer and paste continuity — 2026-10-27

Owners: #51/#69; Cam. Base: merged PR287,
`35f37736806bfb9c8766231328bc363d4b0aa618`. Three related Input outcomes
pass focused Linux qualification. The broader owners remain open.
[Exact receipts](VERIFICATION-2026-10-27-FORM-POINTER-PASTE.json) bind sources,
profiles, binaries, commands, original failures, archives and native callbacks.

## Completed behavior

Pointer placement uses the nearest measured character boundary in the painted
viewport, with ties to the right and clamping to the usable caret interior.
The pointer and painter share actual-pixel bounds; nominal configured Input width
still sizes the control but no longer forces a clicked visible position to scroll.
Logical indices remain separate from UTF-8 byte offsets. The pure hit policy
requires nonnegative, nondecreasing prefix advances and uses bounded binary search;
unit cases cover unequal advances, ties, edges, zero and extreme counts.

A pasted Tab traverses once with multiple fields, or inserts one space in a single
field without activating a Tab-bound button. Existing Return/newline/button
priority and read-only refusal remain. Departed fields and the final paste
destination use the shared painter immediately; no follow-up key or Expose is
needed to reveal the inserted text/caret. The Form manual documents these rules.
No configuration migration, new selection protocol, clipboard service/storage,
resident, toolkit, dependency or core-state authority is introduced.

## Baseline and frozen candidate

The retained PR287 Form SHA256
`ee65826bc8b2836a9fee5bad0f53da63ecdfb750a0ab79ee4c4c50111e960b6f`
reproduces four actual-X failures: an exact first-glyph-edge insertion produces
`xabc` instead of `axbc`; pasted `A\tB` skips the intended next field; single-field
Tab unexpectedly executes its configured action; and final destination pixels
differ from a subsequent actual Expose. All original failures are preserved.
The first pointer attempt failed a fixture scanline assumption before clicking:
y=2 was background while the actual caret began at y=3. The corrected oracle
derives the first contiguous caret row from native pixels and then reproduces
the intended product failure. It does not assume a font's advance width.

The 1,546-file compiled-input inventory aggregate is
`55a1feda29279b02e07a11d5a3087236320d1f9bd68614451683a0633d8222b9`.
No product source changed after freeze. Final normal Form SHA256 is
`889289decc71b773b6c4f76591018b29130f20f15bcf3347bcc70e605d88d998`,
1,800,376 bytes including debug data (+2,848 over PR287). Core/browser binaries
are unchanged. Final source, three drivers and normal/sanitizer binary rechecks
pass. Driver identities remain separate from compiled inputs.

## Combined verification

One retained Ubuntu 24.04.5 ARM64 container on local Linux 6.18.10 ARM64 supplies
GCC 13.3 debugoptimized and Clang 18.1.3 address/undefined debug profiles. Actual
Meson compiler/config/option records are retained. This local VM/container run is
not native-runner or physical release qualification.

All 13 named phases pass: affected Form/unit build; form-input policy; editable
Home/wrap/pointer/paste/paint cases for `fixed`, `fixed,*` and
`xft:DejaVu Sans Mono:size=12` requests; single-field paste; legacy input, history
and three read-only scenes; scoped sanitizer build and policy; actual sanitizer
editable cases; and manual generation. Font requests do not independently attest
the backend used. Sanitizer policy enables leak detection; native editing disables
it for retained Xlib/font caches and establishes no leak-free claim.

Three actual browser profiles pass: Slate 1.25x, Dune 1x and enlarged Classic 2x.
They retain exact draft bytes/cursor through resize, list/grid and real RandR scale
out/back transitions; pointer insertion yields `axbc`; one pasted Tab from Path
wraps into filter and yields local drafts `A`+Path and `Bdef`. Immediate field
pixels match real Expose, with unchanged native identity and checked nine-action
callback order. Six captures are retained; [three unmodified final captures](../../doc/development-preview.md#pointer-and-paste-continuity)
show drafts before submission, so the applied filter label still reads `axbc`.
Lead and separate reviewer inspected saved PNGs. Nine actual module Wait results
and twelve direct child exits are zero, with no unexpected observer X errors or
cleanup failures.

Two failed Slate fixture preconditions are preserved. The extended scene initially
required ready provider state after intentionally submitting an invalid Path.
Typed recovery then exceeded the native callback payload bound because the
synthetic valid folder path exceeds 1,000 bytes. The corrected fixture uses normal
F5 Refresh over the retained valid snapshot and still requires exact path bytes,
loaded 128, ready/current state and native identity. Separate source review
confirmed this recovery. Retrospective inspection of canonical raw records in all
three final runs observes Refresh → Loading → Refreshed → complete 128 → ready,
then render/sync generation 10 before later pointer actions. This is an observed
sequence, not an explicit fixture intent/publication freshness fence. Only affected
Slate scenes repeated; pending Dune/Classic ran once. All 13 successful phases and
compiled product bytes were retained unchanged.

## Costs, cleanup and remaining acceptance

The [due periodic comparison](PERFORMANCE-2026-10-27-FORM-POINTER.md) is complete;
timing ranges overlap and no speed/RSS improvement is claimed. Preserve its exact
next reference and reset to 0/5 on this merge. Accumulated changes remain portable
Form policies/rendering/input, with no changed ABI, OS API, build/install inputs
or unresolved product failure justifying an automatic all-OS matrix. Full #51/#69
consumer/six-target, live scale-configuration reload, physical/assistive/multilingual
and release acceptance remains pending. IME/grapheme/shaping and broader module
adoption are not inferred. #257/#258 remain parked.

Canonical ledger: `builddir/codex-2026-10-27/r1-form-next`. Seven archives containing
443 files pass member hash/size/mode and gzip CRC validation. Original and corrected
failed-driver bytes remain there. Only exact verified guest duplicates were retired;
source, incremental builds, VM disks and measured references remain. The owned
container/VM are stopped and controller released. Host isolated pure policy tests,
repository and whitespace checks pass; full host X11 compilation/manual tooling
remains unavailable, with complete affected checks supplied by Linux.

Named Temper workflow/analyst/implementer/reviewer dispatches requested configured
models; no unavailable role/model was observed. Loader acceptance does not attest
backend execution identity. Separate agent review examines the exact final tree
before integration and is not independent human approval.
