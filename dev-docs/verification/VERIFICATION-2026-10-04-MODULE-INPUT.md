# Module input and descriptor hardening — 2026-10-04

Subsequent fresh-snapshot verification found stale inherited Form objects and
an output-completion weakness in the earlier Form fixture. Those specific
assertions are superseded by the complete-rebuild and fresh-output checks in
[the numeric/string follow-up](VERIFICATION-2026-10-04-SCRIPT-VALUES.md).
The historical counts below describe their saved runs.

FvwmScript selection replies now use one shared module-only reader. It accepts
only complete format-8 properties of the requested type, at most 32 KiB, and
returns owned, terminated storage. Clipboard paste and ReceivFromScript use a
one-second monotonic deadline with poll, preserving unrelated X events. Every
request creates an unmapped private InputOnly window, which is destroyed after
completion or failure; late replies cannot become a later request's input.
This helper requires a responsive X server and is never called from the WM
main loop. Refused/invalid pastes leave text unchanged; missing script messages
retain the existing "No message" result. INCR remains unsupported.

FvwmForm reads one complete STRING/8 cut buffer, capped at 64 KiB, before
changing input fields. This also avoids splitting UTF-8 characters at the former
4096-byte chunk boundary. All eight FvwmScript root cut buffers use the same
bounded reader, transferring ownership instead of leaking Xlib buffers. Its
conditional title lookup is bounded and frees the reply. FvwmBacker now frees
both root-pixmap property replies while retaining its existing delete/kill
semantics and fixed-record validation.

## Regression coverage

The new native/real-X probe checks valid, empty and maximum-sized selection
text; oversized properties; wrong types and 16/32-bit formats; INCR rejection;
refusal; owner death; mismatched notifications; unrelated-event preservation;
silent owners under repeated EINTR; and late-reply isolation. Where the OS
permits it, the X connection is opened above FD_SETSIZE before closing the
filler descriptors. The probe logs the actual descriptor and ceiling.

The real-widget fixture launches actual FvwmScript and FvwmForm processes.
It checks parent/child script messages with custom selection targets, all eight
cut buffers, valid text, malformed/oversized replies, a silent owner, subsequent
recovery, unchanged form values after rejection, and multibyte text crossing the
old cut-buffer boundary. Its IPC barriers discard earlier heartbeat replies
before a selection owner answers. It checks long form values through a local
fixture file, preserving the core's existing 1000-byte module-command cap.

A saved pre-batch FvwmScript passes the valid paste and then aborts with the
fortified buffer-overflow diagnostic on a format-32 reply. The fixture fails
waiting for its next heartbeat. The saved executable, focused control script,
trace and failure log are retained. This establishes the old string-copy defect
without claiming an AddressSanitizer run.

Six injected descriptor setup failures cover pipe creation, both endpoints'
F_GETFD/F_SETFD operations, and lock-file fstat. Tests verify closed handles,
reset output descriptors, and original errno even when cleanup changes errno.
The glibc build intercepts its actual large-file aliases; BSD/musl use POSIX
names. Existing inheritance and unsafe-lockfile tests remain in the gate.

## Executable hardening

Inspection found that GCC distribution defaults supplied immediate binding,
while the Clang/lld build lacked BIND_NOW. Linux builds now explicitly request
RELRO, immediate binding and a non-executable stack where supported. A new
Linux test reads every linked C executable from Meson's target inventory and
checks its ELF headers/program headers/dynamic tags. It verifies the produced
binary rather than relying on flag presence. PIE, stack-protector and available
fortify settings remain enabled. Optional Go executables are outside this C
inventory; the matrix builds disable Go.

## Matrix and provenance

Final counts and archive/source hashes are recorded in
`builddir/codex-2026-10-04/module-input/matrix-summary.json`.

Linux ARM64 GCC, ARM64 Clang/lld and AMD64 GCC each pass all 55 tests.
FreeBSD and OpenBSD on ARM64 and AMD64 each pass all 44 tests, plus the
actual-module selection fixture on their default X servers. All seven final
full-gate archives contain only passing results. Independent source checks
match 51 files across the frozen source and all six platform workspaces.
The reader snapshot and its fingerprint map are retained separately from
subsequent script-file work as `source-reader-final.tgz` and
`verified-reader-fingerprints.json`.

The subsequent script-message/file follow-up has separate evidence and source
fingerprints; these full-gate counts do not certify later edits. Its final
verification and filesystem compatibility decision are recorded in the tracker.

The source snapshot extends the previously verified frame-deferral baseline.
Affected shared module files include the existing opt-in keyboard-focus,
parse-error and fire-and-forget changes already present in this checkout; these
were preserved and tested, not introduced by this hardening batch. Later
parallel desktop configuration/rendering work is outside this frozen baseline.
Independent fingerprints cover the inherited hardening files plus this batch's
owned changes and verification graph. Staged installs are checked separately;
no host installation or running desktop replacement is performed.

## Harness findings

Initial compilation identified a missing logging declaration and required
FVWM event wrappers in the probe. Fixture corrections include quoted form
labels, parent/subwindow event routing, startup ordering of script communication
tables, and avoiding oversized commands when inspecting long input values.
Initial failures remain saved rather than being reported as passing gates.

The OpenBSD AMD64 full gate twice timed out in the existing desktop appearance
fixture; its module input checks passed. A bounded diagnostic run measured
appearance barriers at 5.124, 5.688 and 5.710 seconds under QEMU/TCG, exceeding
that fixture's five-second socket wait. The cosmetic setup barrier now uses a
15-second wait and restores the prior timeout afterward. Interaction/service
latency assertions and production processing deadlines were not relaxed. The
original failures and measured passing diagnostic are retained. These emulated
setup timings are not physical-desktop responsiveness measurements.

## Remaining scope

This closes the reproduced malformed-selection overflow and module text-reader
bounds, not the full security audit. Remaining work includes window/resource
continuation lifetimes, helper/session constructor failure injection, other
select-based helper descriptor limits, tree walks, image decoder isolation and
stress, module interpreter string/index/recursion bounds, and native Linux
ASan/UBSan/fuzz execution. The subsequent [script-file pass](VERIFICATION-2026-10-04-SCRIPT-FILE.md)
replaces WriteToFile's fixed argument buffer; the user approved its strict
file policy on 2026-10-04, choosing secure defaults. Existing synchronous waits
and blocking command handlers still need their own compatibility-preserving
work. No sandbox or new runtime library was added.
