# Enlarged Classic overview layout

PR374 places Mark window and Pointer Move on their own row below the marked
window label. Shorter help text leaves room for the existing controls.

The incremental Desktop build, focused overview unit tests and eight enlarged
Classic interaction cases passed. The live fixture checks the decorated frame
against the X root bounds before capture. At (99, 247) the 1249 × 742 frame
fits within the 1440 × 1200 display. Visual inspection confirms that the title
bar, search, movement buttons and page inputs are fully visible.

[Fresh enlarged Classic screenshot](../../doc/screenshots/workspace-overview-compact-classic-large.png) ·
[Hashes, frame bounds, cases and shutdown receipt](VERIFICATION-2026-11-11-WORKSPACE-LAYOUT.json)

The owned guest shut down cleanly. The earlier Slate, Dune and core results
remain the evidence for their respective checks. PR374 is ready for review.
The broader #221 and #223 qualification maps remain open as recorded in the
[previous UI checkpoint](VERIFICATION-2026-11-09-WORKSPACE-UI.md).
