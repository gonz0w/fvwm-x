# Editable Form input continuity — 2026-10-27

Owners: #51/#69; Cam. Base: merged PR286,
`8554ff704cb4c186f4eaf06a8106598e8b2f2760`. This finite editing slice
passes focused Linux qualification; both broader owners remain open.
[Exact receipts](VERIFICATION-2026-10-27-FORM-INPUT-CONTINUITY.json) bind
compiled inputs, drivers, actual compiler profiles, binaries, commands, archives,
scale transitions, callbacks and cleanup.

## Result and scope

Long editable Inputs keep the cursor visible after Home, End and arrow movement.
Every field transition reconciles destination cursor/scroll state before editing;
forward/backward and last-to-first traversal now enter at Home consistently,
including read-only Inputs. This changes incidental wrap positioning without
changing supported configuration syntax, bindings, button priority or list focus
ordering. The man page documents the behavior.

A pure character-index policy clamps logical cursor/scroll state separately from
UTF-8 byte offsets. The shared painter fits the visible prefix to measured pixel
width and clips/clears the complete interior. Keyboard, expose and reflow use that
same painter; the duplicate editable-key drawing path is removed. Borrowed font
regions and GC clipping retain balanced ownership. Production C/header changes
remove 38 net lines. No toolkit, process, dependency or configuration migration
is introduced.

## Reproduction and source identity

The retained PR286 Form (SHA256
`4febe8484a6058e47c84c41e9e003cc0388e395f4feba6a6ae913f7b2b7fe119`,
1,795,656 bytes) reproduced two actual-X defects: Home painted the tail of a long
editable value, and Down-wrap into a short field followed by Backspace/End/insert
produced `abxc` instead of `abcx`. The latter exposed stale character counts
through observable edit results; the fixture does not introspect internal counts.
Both original failures remain in the verified baseline archive.

The final 1,546-file compiled-input inventory has aggregate SHA256
`961f86d236ebfee31c2d047973f2ccc88526345483cbf53b5b35d4ca1ee7a5a3`.
No product source changed after the first freeze. Driver versions are bound
separately. Final Form SHA256 is
`ee65826bc8b2836a9fee5bad0f53da63ecdfb750a0ab79ee4c4c50111e960b6f`,
1,797,528 bytes: +1,872 bytes including debug data. Core/browser binaries remain
unchanged. This establishes no runtime speed/RSS gain. PR281's performance
reference is 3/5 after PR286 and 4/5 on this merge; no comparison is due.

## Combined checkpoint

Use the retained Ubuntu 24.04.5 ARM64 container on the local Linux 6.18.10 ARM64
Podman VM. Meson's actual compiler records identify GCC 13.3 normal
`debugoptimized` and Clang 18.1.3 address/undefined `debug` profiles, with selected
flags and generated-config hashes recorded. This is actual-X local VM/container
execution, not physical or native-runner release qualification.

Twelve named phases pass: affected Form/unit incremental build; form-input unit
policy; editable cases with `fixed`, `fixed,*` and
`xft:DejaVu Sans Mono:size=12` font requests; existing input and field cases;
three read-only scenes; sanitizer Form/unit build; sanitizer policy; actual
sanitizer editable cases; and the updated Form manual render. Font requests are
recorded, but actual font backend identity was not independently introspected.
The sanitizer policy uses leak detection; the native editing run disables it for
Xlib/font caches and does not establish leak freedom.

Coverage includes long Home/End/arrow navigation, exact edit bytes, delete,
Backspace, Ctrl-K/U, forward/backward and wrap transitions into short/empty values,
valid multibyte boundaries, full viewport restoration, actual Expose equivalence
and unchanged borders/neighbors. Prefix comparison preserves positional ink and
internal background. Successful unchanged phases were retained; only concrete
affected/pending phases repeated. No Go/concurrency change justifies a race suite;
no all-OS/install matrix was rerun for this portable control slice.

Three actual browser scenes use Slate 1.25x, Dune 1x and enlarged Classic 2x.
Path/filter draft bytes and logical cursor survive resize out/back, F7 list/grid
switches and real RandR scale changes out/back. Observed factors are respectively
1.25→1→1.25, 1→1.25→1 and 2→1.25→2; current/desired epochs advance 1→2→3,
monitor atoms remain stable and resource generation stays 1. Scale factor and
resource generation occupy separate font-cache keys. Native children/list
state survive reflow; exact callbacks are checked. Six PNGs are retained,
[three unmodified captures published](../../doc/development-preview.md#editable-input-continuity).
Lead and separate reviewer inspected saved images. All nine actual module Wait
results and twelve direct-child exits are zero; no unexpected observer X errors
or cleanup failures occur.

## Preserved fixture corrections

Three initial fixture expectations failed; none required a product edit:

- The initial fontset prefix rectangle extended past the short reference into
  legitimate continuation glyphs. A reference-derived rectangle compares the
  full prefix and internal background. Offline comparison against retained raw
  pixels gives zero Home mismatches for the candidate core/fontset scenes and
  rejects the PR286 Home and End scenes with 232 mismatches each. The candidate
  End scene also differs by 232, proving the oracle distinguishes the viewport.
- The old read-only wrap case expected inherited-tail positioning. The new
  documented destination-Home contract requires the head immediately after wrap;
  an explicit End must restore the original tail, with bytes unchanged.
- The first Slate scale scene incorrectly required resource generation to rise
  when only the quarter-scale factor changed. The corrected oracle requires the
  actual factor, advanced current/desired epochs, stable monitor and unchanged
  positive resource generation on both transitions.

Original driver bytes, failed receipts and images remain archived. Core-request
success is in `native-fontset-first`; corrected fontset/Xft/input/fields successes
are in `native-readonly-transition-first`; final read-only/sanitizer native
successes are in `legacy-final`; all final browser themes are in `native-final`.
`checkpoint-final` preserves the complete command history, including the two
failed transport expectations. `native-scale-generation-first` preserves the
third failure. The JSON records each immutable archive hash/size/member count
and the offline proof identity.

## Upkeep and remaining acceptance

Canonical host ledger:
`builddir/codex-2026-10-27/r1-form-followthrough`. Seven archives (335 files) were
retrieved and verified against member hashes, sizes, modes and gzip CRC before
retiring only exact guest duplicates. Source, incremental builds, caches, VM disks
and the performance reference remain. The owned container and VM are stopped and
the controller released. Host full compilation lacked X11 headers and host
Asciidoctor was unavailable; the isolated pure helper passed there, while complete
compilation, unit, manual and actual-X checks passed in Linux.

Named Temper workflow/analyst/implementer/reviewer dispatches requested their
configured models; the loader accepted the requests and no unavailable role/model
was observed.
Backend identity is not independently attested. Separate final source/evidence
review is required before integration.

Broader consumer/six-target, physical display, assistive technology, multilingual,
IME/grapheme/shaping and release acceptance remain open under #51/#69. Paste-Tab
fallthrough, clipboard storage/services, work-area generation/race extensions and
Choice/Button focus redesign are outside this checkpoint. #257/#258 stay parked.
