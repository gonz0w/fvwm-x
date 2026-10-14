# File-icon owner preparation — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: implemented; focused Linux checks pass;
combined activation and Linux/BSD qualification remain pending.

File icons now hold a pending request separately from their immutable published
picture. Completion resolves the current client through its X context, validates
slot/source identity and the current icon monitor scale, and uses the existing
icon redraw and EWMH update paths. Superseding styles, application icon priority
and client destruction cancel the old interest. Failed replacement retains old
pixels; a failed first request can fall through to application hints.

Icon-window resize copies have explicit ownership and their intermediate pixmap
is released. Cache palette ownership stays with the immutable picture. EWMH
sampling pins both the restored published source and its temporary sample; it
releases allocated colors only after color queries, and skips pending donation.
Native module packet layout is unchanged.

## Source and evidence

Frozen source: `builddir/codex-2026-10-06/svg-file-icons/source-file-icons-final.tgz`.
SHA-256: `5b0076e28ef3f678f1ad2d0523936218d08dbf20ca5728aa316f34b3e6155fc9`.
All 1238 file contents/modes match both tested guest source trees. The archive
extends the previous qualified owner preparation with the seven file-icon owned
files and scoped test registration; concurrent desktop/tooling edits are excluded.
Build identities and options are in each archived metadata file. They are build
identities, not installed-artifact qualification.

Evidence archives: `evidence-normal.tgz` and `evidence-sanitizer.tgz` in that
same directory. The final normal log is `file-icon-observer-repair-tests.log`;
the sanitizer log is `tests-observer-repair.log`.

| Profile | Focused entries | Result |
| --- | --- | --- |
| Ubuntu Linux ARM64, Clang 18.1.3, private PNG 1.6.59 | mini-icon regression; file-icon 24-bit; file-icon 8-bit | 3/3 pass |
| Same source, scoped ASan/UBSan core/MFL | same entries | 3/3 pass |

Each actual-core entry runs at 1× and 2×. Observations include absent first load,
old image and donation retention, successful replacement, refused replacement,
Adjusted/Stretched/Shrunk resize, higher-priority native application icon window,
destroyed client cancellation and a subsequent successful owner. Actual icon
window pixels and EWMH dimensions/pixels are checked. Leak checking is disabled
for the actual core/MFL processes; the finite-memory synthetic decoder is the
matching unsanitized worker. This is local development evidence, not native Linux
release sanitizer CI.

## Failed and corrected checks

Initial compilation needed the owning PictureUtils/System declarations. The added
runtime MonitorScale test was invalid: this command is startup-only and explicitly
refused the change. That test and its success claim were removed; live mixed-monitor
movement remains a combined RandR gate. An EWMH replacement test encountered the
existing donated-icon hint classification behavior; native WM_HINTS icon priority
now exercises pending cancellation without changing that unrelated policy.

The icon observer initially raced WM window replacement. It now briefly grabs the
server for one tree/attribute/pixel snapshot. The first repair omitted ungrab;
that task-owned run was stopped and the finally cleanup corrected. Failed logs
are retained. No passing result from these failed fixtures is carried forward.

## Remaining gates

Production asynchronous activation remains off. Cursor/root owners, derived X
allocation/error acknowledgement, actual enabled-WM slow-load responsiveness,
live monitor movement, combined frozen Linux/BSD builds/install/default-X/optional
profiles and native release sanitizer CI remain pending. No BSD result is claimed
for this file-icon continuation.
