# Workspace qualification follow-up

PR374 merged as `d74b839072c2ab7e2901f354edb38c4345adc1cb`. Its signed squash
tree matches the reviewed candidate.

The Slate scene passed all 16 cases using the retained Linux ARM64 build.
F8 moved the selected window to workspace 5, page (1,0), preserving the frame
size. Enter on the selected workspace row activated workspace 5, as observed
in the core workspace query. Setting WM_TRANSIENT_FOR on the managed
application made the scratchpad command return unsupported. The application
kept its allocation identity and geometry and its launch count stayed at one.

The first attempt confirmed the page move and exposed a missing workspace-row
selection in the fixture. The second attempt used the same keyboard selection
sequence as window activation. The owned guest shut down cleanly.

[Source hashes, cases and observations](VERIFICATION-2026-11-11-WORKSPACE-FOLLOWUP.json) ·
[Three-theme results](VERIFICATION-2026-11-09-WORKSPACE-UI.md) ·
[Enlarged Classic layout](VERIFICATION-2026-11-11-WORKSPACE-LAYOUT.md)

Both issue maps stay open. #221 still needs a real terminal provider and the
remaining identity-reuse and mixed-monitor/scale qualification. #223 still
needs overview update bounds during topology and scale changes. Reuse the
palette collection evidence for the underlying virtualization. Carry the
shared monitor and placement checks into the #220/#222 feature batch and
record the terminal case at the next suitable shared checkpoint.
