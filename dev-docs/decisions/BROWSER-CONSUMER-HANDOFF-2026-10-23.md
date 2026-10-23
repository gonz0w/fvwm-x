# Portable browser consumer handoff

Status: implementation after the PR176–178 accumulated platform checkpoint is
closed. Owners: FILE-ACTION/DATA/VIEW-01, portable FILE-INTEGRATION-01 and shared
IPC-02 transport. Four related capabilities: reusable correlated request client,
opt-in matching-browser folder/target handoff, core-owned presentation and an
optional installed desktop folder entry. Existing supported configuration syntax
and independent launches remain usable.

Move the command client's existing bounded JSON/correlation policy into the
shared transport package; retain exact response formatting and domain refusal
behavior in FvwmCommand. Each request owns one reader and closes/joins it on
all exits. A caller context and six-second per-request cap bound the wait.
These are ordinary metadata/control records, never private clipboard payloads.

--reuse accepts a folder (including one positional local URI/path) or target set.
Pure shared target planning validates lexical parents without filesystem work.
Discover at most eight instances, skip busy/nonmatching/older-capability views,
and submit only to an idle matching folder. Larger-than-wire requests fall back
to the existing independent consumer without truncation. A stale precondition
may retry once; an ambiguous submitted outcome never launches a duplicate.
Total IPC budget is ten seconds. Reuse does not override a running consumer's
programs or presentation/observer settings; explicit customization flags are
refused with reuse. Raw bytes retain their existing schema and worker resolution.

The action receipt is admission, with the existing work identity and lifecycle.
Present is separately generation-checked/native-published, then uses only the
provider's opaque identity in core All/Iconify/Focus/Raise commands. No filename
or label becomes FVWM syntax. It never calls Xlib to alter managed-window state.
Normal core desk/viewport policy makes the selected window visible without a
pointer warp. Presentation refusal does not retract admitted work.

Go-enabled builds install a development desktop entry for inode/directory, with
TryExec, no bus activation, and `Exec=fvwm-file-browser --reuse -- %u`. The optional
URL also allows a no-argument Home launch. No default association or existing
configuration is rewritten. Adopt Desktop Entry Specification 1.5 sections 6
(recognized keys) and 7 (single URL field expansion):
https://specifications.freedesktop.org/desktop-entry/latest/recognized-keys.html
https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html
Use the project's existing independent field-code policy plus actual URI/CLI
and staged-install checks. Full default-provider/toolkit/FileManager1 acceptance
is not implied by installing this optional entry.

Reuse the three theme fixtures and capture target handoff/presentation and folder
refresh. Run focused Linux plus early FreeBSD for socket/context/CLI/install
changes. Preserve the closed six-target cycle; new-feature integration starts
at PR178 0/3. The due periodic comparison uses preserved PR174 and this batch's
qualified Linux binaries, with no extra benchmark build. File jobs, encrypted
storage/SQLite, private content and broader consumer/release gates remain open.
