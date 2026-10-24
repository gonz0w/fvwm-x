# Utility and example ownership — 2026-10-12

Owners: UTIL-01, RUNTIME-GO-01, COMPAT-02 and TOOLING-GO-01. Status: source classification with the installed Script example qualified and the full XDG provider/frontend fixture baseline qualified;
remaining rows are not qualified ports or runtime retirement.

The three small standalone Perl utilities have separate Go checkpoints. This
inventory distinguishes the qualified installed communication example from
development generators, optional recipes and the full XDG menu service. It
does not move the desktop/runtime queue ahead of the tracker execution order.

| Consumer | Installation and dependencies | Owner and next work |
| --- | --- | --- |
| [fvwm-script-ComExample.pl](https://github.com/gonz0w/fvwm-x/blob/6d8db4199c207e22ddac86c9641bcea17262527e/modules/FvwmScript/Scripts/fvwm-script-ComExample.pl), [FvwmScript-ComExample.in](../../modules/FvwmScript/Scripts/FvwmScript-ComExample.in) | Script Meson installs the helper and configured declarative example into FVWM data. The configured template selects the optional Go helper or the retained Perl fallback. Explicit copied Perl invocations remain supported. The helper speaks the existing Script FIFO request/reply format; it is not a native module-packet client. | UTIL-01 / RUNTIME-GO-01: [final6fcc checkpoint](VERIFICATION-2026-10-12-SCRIPT-COM-PORT.md) qualifies bounded stdlib Go transport, paired install selection, count/Parse/exit, lifetime/file controls and actual native widget interactions across six targets. The legacy file remains byte-identical. Explicit interpreter retirement/migration stays under COMPAT-02. |
| [FvwmForm-RootCursor.pl](../../modules/FvwmForm/FvwmForm-RootCursor.pl) | Development generator reads a hard-coded cursorfont header and emits [FvwmForm-RootCursor](../../modules/FvwmForm/FvwmForm-RootCursor). Form Meson installs only the emitted configuration, not this generator. | TOOLING-GO-01 / GEN-01: fixture-backed development tooling cleanup. It is not an installed Perl runtime prerequisite. Preserve the emitted Form/configuration contract. |
| [read_status.pl](../../contrib/status/read_status.pl), [panel](../../contrib/status/panel) | Optional source recipes with Perl JSON::XS, lemonbar, conky and xrandr. No contrib Meson subtree installs these status scripts; [recipe prerequisites](../../contrib/status/README.md) are separate from the baseline. | Optional contrib / RUNTIME-GO-01: retain as a source example until a bounded stdlib status consumer and its formatting/selection fixtures have their own checkpoint. No baseline package/dependency adoption is implied. |
| [fvwm-menu-desktop.in](https://github.com/gonz0w/fvwm-x/blob/6d8db4199c207e22ddac86c9641bcea17262527e/bin/fvwm-menu-desktop.in) | Installed Python full XDG menu provider. Existing behavior includes menu discovery, XDG XML/layout, desktop entries, icons, regeneration and saved preferences. | RUNTIME-GO-01: [efe8 fixture checkpoint](VERIFICATION-2026-10-12-XDG-FIXTURES.md) captures the real provider with an isolated pinned PyXDG oracle across six targets. The [d33f input-policy foundation](VERIFICATION-2026-10-12-XDG-POLICY.md) is qualified but has no production caller. The [9337 discovery/preference checkpoint](VERIFICATION-2026-10-12-XDG-DISCOVERY.md) qualifies four more inactive policy stories. The [be8e ordered-allocation checkpoint](VERIFICATION-2026-10-12-XDG-ALLOCATION.md) qualifies another three inactive policy stories. Full file merging/plan construction/rendering, direct paired settings and CLI/install/native-X qualification follow. The launcher does not replace this utility. |
| [fvwm-menu-desktop-config.fpl](https://github.com/gonz0w/fvwm-x/blob/6d8db4199c207e22ddac86c9641bcea17262527e/bin/fvwm-menu-desktop-config.fpl), Form XDG options/help | Installed Perl-specific configuration frontend emits native Form commands and saves preferences consumed by the full provider. It is also emitted as a Configure action by that provider. | COMPAT-02 / RUNTIME-GO-01: paired generation/discovery/preference/action fixtures pass at efe8; the legacy preference codec now passes at [9337](VERIFICATION-2026-10-12-XDG-DISCOVERY.md), while direct native Form save/export and installed replacement remain open. Coordinate provider/frontend and all emitted/manual references with approved Perl retirement. Do not remove the frontend merely because small utilities now have Go implementations. |
| [fvwm-perllib.in](../../bin/fvwm-perllib.in), [perllib](../../perllib/meson.build), [FvwmPerl](../../modules/FvwmPerl/meson.build) | Installed Perl framework/library entry points; current modules and desktop configuration frontend still consume them. | COMPAT-02: explicit retirement and migration documentation after consumer/install qualification. The metadata generator checkpoint does not retire these installed interfaces. |
| [FvwmTalk wrapper](../../modules/FvwmForm/FvwmTalk-wrapper.in) | Installed base-shell forwarding entry point to native FvwmForm with the shipped Talk configuration. No Python/Perl interpreter is involved. | Retain this small wrapper and configuration-referenced name. No Go rewrite is needed solely to eliminate a base-system shell. |
| [xsession entry](../../contrib/xsession/fvwm3.desktop.in), [desktop recipes](../../contrib/README.md) | Root Meson explicitly installs the session entry. The locking/display/compositor recipes remain optional source contributions. | SESSION-01/02 and owning desktop batches. Classification does not claim runtime lifecycle or optional-service qualification. |

The source authority is [root Meson](../../meson.build),
[bin Meson](../../bin/meson.build), [Script Meson](../../modules/FvwmScript/meson.build)
and [Form Meson](../../modules/FvwmForm/meson.build). The
[Script communication manual](../../doc/FvwmScript.adoc) references both example
files. The [XDG manual](../../doc/fvwm-menu-desktop.adoc), its configured actions
and installed Form help reference the Perl frontend. These references were
checked directly; a filename extension or historical list was not used to
infer installation or portability.

No contribution/generator was executed or removed by this classification.
It introduces no consumer, service, library or new configuration migration.
The [tracker](../PROJECT-TRACKER.md#execution-order) remains the queue authority.
