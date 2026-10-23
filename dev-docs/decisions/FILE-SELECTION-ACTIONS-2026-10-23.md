# File selection actions and observed selection properties

Status: implemented; focused Linux and early FreeBSD policy/process checks,
Linux race and three real-theme native-X drivers pass. Broader acceptance remains
open.

Owners: FILE-ACTION-01 / FILE-DATA-01 / FILE-VIEW-01; shared metadata/action
support. This checkpoint follows merged PR170. Configuration syntax is additive;
no shipped default/theme, mandatory dependency or generic transport changes.

A configured action has a bounded user label and one literal executable. At most
four buttons are configured per browser instance; no action runs on startup.
The exact rendered-view callback captures 1..256 selected IDs, including hidden
selections. No path or command is reconstructed from a label. The existing
one-active-child owner refuses a new action while busy rather than deferring a
side effect. Cancellation retains its slot until reap; cancellation before child
startup is reported as cancelled rather than failed-to-start.

The isolated worker admits the whole ordered argument list before filesystem
work: known unique IDs, file/directory kinds, literal direct-child names, 4096
bytes per absolute path and a 32 KiB total argument budget including terminators.
It checks the parent and every selected observation before starting one process
with separate literal arguments and the observed parent as working directory.
A stale, missing, symlink or special entry refuses the whole launch. The count
and result mean process creation, not application completion or opened pixels.
Executable/environment/platform limits can still refuse launch. This path
observation is not a descriptor lease; mutation jobs keep their own contract.

Properties admits one to 256 selected entries independently of visibility. One
entry retains the old detail view; multiple entries show type counts and a
checked sum of regular-file sizes. No recursive folder, link-target or content
access runs. Summary metadata stays with the admitted snapshot; missing or
invalid IDs and overflow refuse without partial totals. Any disappearing
inspected identity retires the old panel. Repeated --properties targets use the
existing atomic same-parent locate worker and initial selection ownership.

This is file metadata and a configured executable handoff, not protected
clipboard content or encrypted storage. Future clipboard consumers reuse typed
opaque selection/action contracts while CONTENT/RETENTION/STORE retain native
protected memory and private delivery ownership. No clipboard payload enters
this ordinary Go model, Form callback, general JSON or Echo channel. SQLite,
external streaming providers, in-place native updates, watching and file mutation
remain separate. Default single-file opener behavior stays intact.

Verify policy/race and real process argv on Linux with an early FreeBSD check,
then actual native list/grid/summary/action controls under Slate dark, Dune light
and larger-font classic configs. Capture screenshots from those runs; reuse
qualified native binaries. The accumulated platform cycle is 1/3 after PR170;
this batch becomes 2/3 only on merge. Native-CPU/hardware/full acceptance remain
separate; no full install/platform matrix repeats solely for PR creation.
