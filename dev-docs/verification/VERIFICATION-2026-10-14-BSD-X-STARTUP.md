# Default BSD X startup diagnostics — 2026-10-14

Owner: LIB-07 #93, with CI-01. Status: four-story diagnostic/comparison
checkpoint locally qualified. The original VESA/DDC startup fault remains
open; no cause or server repair is established. [Machine receipt](VERIFICATION-2026-10-14-BSD-X-STARTUP.json).

## Coherent scope

One PR contains four related stories and their checks:

1. Server-only startup/authentication checks need no WM build. Repetition runs
   1–32 independent attempts and fails the aggregate if any attempt fails.
2. Attempt results retain the failing phase, error and server status. Logs are
   collected after owned children stop; an unchanged vendor log cannot be
   attributed to a new failed start. Installed executable/module SHA256 and
   OpenBSD patch inventories identify the observed dependencies. FreeBSD's
   launcher, privilege wrapper and actual server are recorded separately.
3. Typed cancellation unwinds repetition without starting the next attempt.
   Shared harness callers still catch its existing base error. Real teardown
   checks retain bounded termination, child reaping and signal restoration.
4. Fresh OpenBSD preparation applies signed vendor updates, waits finitely for
   existing kernel reordering, then explicitly reboots. The owned QEMU runner
   requires a changed validated native boot identity. Unchanged boots, malformed
   output, failed requests and expiry are refused. Existing jobs keep their
   boot lifecycle unless this option is selected.

No core/module/first-party Go implementation, compiler baseline, application
library or FVWM configuration changes. The new recipes use existing developer
Python and OpenBSD base tools. Existing tested WM/client binaries are reused.

## Affected local qualification

| Check | Result |
| --- | --- |
| Final startup policy | 16 per target, 96 passes across Linux/FreeBSD/OpenBSD AMD64/ARM64 |
| Final QEMU policy | 31 per target, 186 passes across the same six targets |
| Actual shared-harness termination | Six passing owned-child/signal-restoration probes |
| Intended retained-guest server-only controls | Eight per BSD target, 32 passing startups/refusals |
| Extra retained-guest controls from dispatch mistake | Another eight per BSD target, 32 passes; retained separately |
| Fresh updated OpenBSD AMD64 | 16 passing server-only startups/refusals after verified reboot |
| Existing client modes, final driver | Normal Command/Prompt on four BSD targets; Script mode on both OpenBSD targets; ten application fixtures over six authenticated server starts |
| Reused executable identities | 36 core/module/installed-client observations unchanged before/after client-mode checks |
| macOS `make verify` | 166 passes, eleven explicit native-Meson skips |

The current diagnostic runs observed 80 successful server-only starts and six
successful client-mode server starts. This does not erase the two original
failed starts in the [Go checkpoint](VERIFICATION-2026-10-14-GO-BASELINE.md),
prove reliability statistically, or identify a vendor patch as their fix.
Application outcomes and server readiness remain separate in every receipt.

The final candidate uses fourteen frozen helper/test/recipe/pin files. Its
six-target policy and actual client-mode checks use the final helper. The fresh
image uses the earlier fourteen-file v3 archive; the later helper change adds
only FreeBSD launcher/executable identity collection and its unit control.
The affected native FreeBSD reads and final six-target policy pass separately.
V1–v4 source hashes, modes, artifact hashes and precise coverage are in the
machine receipt. Runtime builds and the broad distribution matrix were not
repeated for this developer-harness checkpoint.

## Fresh image comparison and retained failures

The retained lab guest is OpenBSD 7.9 GENERIC.MP#449 AMD64 with no installed
syspatches, Xorg 1.21.1.21 and VESA 2.3.4. The independently expanded pinned
provider image starts at GENERIC.MP#8 with fourteen installed patches. Signed
syspatch advances it through 034, including the current X-server update; the
verified reboot loads GENERIC.MP#16. The VESA module bytes remain unchanged.
Exact before/after server hashes, patch lists, boot identities and QEMU arguments
are retained. The original lab guest and its disks/settings are preserved.

The [vendor errata](https://www.openbsd.org/errata79.html) and
[server patch 032](https://ftp.openbsd.org/pub/OpenBSD/patches/7.9/common/032_xserver.patch.sig)
identify server-extension fixes. They do not establish a VESA/DDC startup repair.
The provider also uses a different machine profile from the retained lab guest;
this is a fresh updated-image comparison, not a single-variable causal test.
The existing explicit disposable VESA aperture opt-in is preserved; no new host
or production security default changes. Process cores and authentication files
are excluded from collected evidence.

Two intermediate driver/preparation failures are retained:

- The first fresh preparation correctly refused syspatch while kernel reordering
  was still running. The final recipe waits at most four minutes for that task,
  refuses inventory failure, and leaves syspatch's protection intact. Its second
  fresh preparation, reboot, checks and cleanup pass.
- A local dispatch path mistakenly invoked the earlier unchanged test driver.
  All checks passed, but collection looked for the wrong archive name. The
  extra 32 startup attempts and failed dispatcher logs are preserved. Corrected
  dispatch runs only the intended affected policy checks; no WM rebuild follows.

A final progress read raced completed cleanup: the job key was already removed
and its VM stopped. That refused SSH read is not a gate result. Runner status,
complete gzip streams, frozen inputs, JSON outcomes and executable observations
were validated directly afterward.

## Remaining owner work

#93 keeps the original failure open. A matched hardware/vendor-source control
or a new retained failing startup is needed to narrow its cause and qualify an
actual repair. Passing repetitions alone are insufficient. Broader LIB-07
vendor/transitive mapping, native-CPU sanitizers, physical hardware and release
acceptance retain their separate gates. Historical patched-image and Go client
results retain their original scope; no blanket server/dependency certification
or new FVWM configuration migration is claimed.

Ignored local evidence: `builddir/codex-2026-10-14/bsd-x-startup/`, including
`validate.py`, source scopes/archives, six-target policy/teardown/client archives,
original dispatch errors and both fresh-image jobs. All review and execution
are local; Actions stays disabled.
