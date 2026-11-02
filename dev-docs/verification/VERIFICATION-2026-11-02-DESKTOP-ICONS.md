# A2 desktop icons: source and affected-build checkpoint

THEME-02 #214 / ICON-01 #215 gains an independent native-tray theme preference,
atomic scoped adoption, a local icon acquisition fence, metadata/manuals and
schema/Go fixture upkeep. [Contract](../decisions/DESKTOP-ICON-PREFERENCE-2026-11-02.md) ·
[Exact source/build record](VERIFICATION-2026-11-02-DESKTOP-ICONS.json).
Base is merged PR346 `825f3d0156e06709570011e5bf1094522f73d68b`.

Empty desktop_icon_theme resolves to the reserved SlateTray hicolor profile;
application XSETTINGS icon_theme and explicit installed-menu profiles remain
independent. Unchanged effective selection does not reload. Pending resource
recovery retains the submitted tuple; guarded replay updates only the owned tray
configuration. Metadata does not attest rendered output. Old exact file schemas
1–10 load without rewriting; the existing store saves schema11.

The native batch retains a local theme/size/scale acquisition generation in
addition to item/menu identity. Admission rejects older acquisitions before
picture loading, including an ABA tuple. Menu close, stale/not-ready state,
item replacement and node bounds also refuse results. Exhaustion is terminal
across retry until that UI owner is recreated. No worker wire, external client
pixels, second resolver, shared cache unlinking, package or asset adoption.

## Observed compilation and cleanup

Five affected offline Go1.27.2 compile/link commands pass: desktopsettings,
desktopcontroller and desktopcheck test binaries plus FvwmDesktop and its worker.
Existing SDK, vendored sources and cache were reused. The three changed shipped
Python fixtures pass syntax compilation without executing their modules.
The host C attempt remains unavailable due to missing X11/Intrinsic.h.

Separate source review found two P2 paths: icon-only Reload applied stored
Visible Off to a live host, and the normal service order could admit a completed
batch before a changed tuple had advanced its generation. The corrections use
a native Refresh message over the existing visibility-preserving snapshot path
and synchronize a changed icon tuple before result admission even without bus
dirtiness. Existing explicit Reload still applies stored visibility. A new
visibility fixture uses changed padding geometry as its completed-refresh
barrier; hidden geometry uses raw attributes rather than mapped-only rect().
These are authored/source-checked corrections, not observed runtime outcomes.

The final separately reviewed finite Linux plan compiles the two corrected C
objects with the existing cached GCC profile/configuration and327 exact source/
header bindings. It does not link or execute a candidate. Build/export took1.84s
after readiness, with about0.53s summed C compilation. Both object hashes match
guest receipts; eleven export files are recorded. Container stop precedes export;
independent holder receipts confirm container and machine stopped. The earlier
one-object pre-correction build, host header failure and source-review findings
remain retained with their original inputs, not counted as corrected-source
qualification. Two build-only lab starts occurred, with no source/cache/SDK/
install stage overwritten. Host composition retains three unchanged artifacts
and rebuilds only the affected controller and executable after correction.

## Pending runtime and owner criteria

There are **zero runtime/test/fixture invocations**, installs or new packages.
Both lab starts served compilation only. Authored Go cases cover validation,
historical schemas, future-key substitution, independent choices/overrides,
submission facts, scoped apply/reset/no-reload and recovery barriers; native visibility checks are authored as well. They were
compiled, not run. Shipped MFL/native-settings schema assertions now expect11;
intentional historical migration fixtures remain historical.

The [native load-attempt design](TEST-DESIGN-2026-11-02-TRAY-ICON-ACQUISITION.md)
is pending fixture authoring/execution. It uses an independent held completed
batch and exact-path loader observation, because old admission can be followed
by menu reset before painting. No visible stale-paint or observed negative-control
claim is made. Runtime/race, actual core recovery/conflict, pending icon results,
fallback/disabled formats, three themes/enlarged text and screenshots join A1–A4.
C2–C5 clipboard qualification stays separate and capture stays disabled.

Separate integrated source/evidence review clears all31 paths after the three
source/fixture corrections; exact signed-delivery review follows. Full support/compiler/install,
asset evaluation, reference-safe disk retirement, file-change invalidation and
broader #214/#215/release acceptance remain open. No issue closes or speed gain
is claimed. Performance remains3/5 before integration; no comparison is due.
