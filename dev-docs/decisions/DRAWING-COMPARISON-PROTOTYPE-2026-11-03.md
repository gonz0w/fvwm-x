# Private drawing comparison prototype

Status: G2 source draft for GRAPHICS-02 #211; evaluation and a separate adoption
decision remain pending. The [tracker](../PROJECT-TRACKER.md) owns the bounded
work and acceptance. This prototype does not select a production renderer.

The default-disabled `graphics-eval` Meson feature defines non-installed native
and Cairo variants of FvwmForm and FvwmButtons using their existing source lists.
The private Cairo target checks the Cairo Xlib API by linking directly, independently
of SVG. This is a link check, not execution or backend adoption. It adds no package
installation or mandatory dependency to ordinary targets.

The operation-local painter compares bounded lines, rectangles, ellipses and a
linear gradient. It initially accepts only the root default TrueColor depth-24
RGB8 visual, at most 16 operations, a 2048-pixel side and 262144 scratch pixels.
It borrows the display, drawable, visual, clip and colors synchronously, retains no
item, font or picture, and uses one scratch pixmap, GC and Cairo surface. It
preserves destination pixels, finishes and checks client-side Cairo status before
an ordered copy, then retires scratch resources. Unsupported inputs or preparation
failure require the existing native draw at the same site. Submission and client
status do not acknowledge asynchronous X server success.

Private adapters cover selected Form list/choice outlines and Buttons focus and
indicator shapes; existing owners retain text, pictures, geometry, layout and
resources. Each module has one local color transition, limited to eight paints in
120 ms, with no idle timer. Reduced motion is the default; only the private
harness explicitly requests full motion with `FVWM_GRAPHICS_EVAL_MOTION=full`.
`FVWM_GRAPHICS_EVAL_REFUSE` is a private refusal-injection control. Interruptions
and invalidated ownership cancel or replace pending work. Neither control is a
product configuration contract. There is no core renderer, text/layout engine,
compositor or general renderer adoption in this scope.

The retained unit01 receipt reports that the pure policy compiled and ran on
native macOS ARM64 with exit 0; it did not exercise X, Cairo or a Linux/BSD
target. Its `result.json` SHA-256 is
`6cbd2db5697b1d3d4b44f7ae05da4ae91dfa7ef3c12155573c74c80f79f93399`.
The later [affected Linux compilation](../verification/VERIFICATION-2026-11-03-DRAWING-EVALUATION.md)
passes for ordinary/private targets; current policy source differs from the host
unit input by subsequent whitespace/comment reflow only. GUI runtime remains
queued. No visual result follows from compilation or the earlier policy pass.

The frozen #211 acceptance remains: repeated profile comparisons and startup,
latency, CPU, RSS and idle costs, source/ELF and dependency inventories, real-X
fallback/lifecycle observations and six-target costs. Runtime work is queued for
the G2–G5 evaluation checkpoint. `NextRequest` reports issued-request deltas;
it is not a roundtrip count. A complete implicit-roundtrip observation remains a
measurement prerequisite. Source, compile or unit success alone cannot close
#211 or decide adoption.
