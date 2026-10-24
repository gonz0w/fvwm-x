# M1 utility retirement and retained scope

Implemented for DEP-GO-01 #179, DEP-XDG-01 #180 and LEGACY-01 #181.
The maintenance register remains the queue authority. This is an exact
component disposition, not complete Python/Perl framework retirement.

| Component | Disposition and owner |
| --- | --- |
| Go utility family / compiler | Every Meson build requires the existing stable patched compiler baseline. Keep local toolchain, vendored inputs and no implicit downloads. Remove the `golang` feature option and disabled install graph. Runtime machines need no Go executable. |
| `bin/FvwmCommand.in` | Retire the replaced Python implementation; keep installed `FvwmCommand` and its qualified Go transport/CLI. |
| `bin/fvwm-menu-{directory,xlock}.in` | Retire replaced Perl producers; keep installed names, options, formatting and native menu consumers. |
| `bin/fvwm-menu-desktop.in` | Retire the replaced Python/PyXDG provider, including PIL/Pillow and ImageMagick fallback code; keep hierarchical menus, locale/filter/settings, literal argv and supported image formats in Go/native owners. No packages are uninstalled from users' machines. |
| `bin/fvwm-menu-desktop-config.fpl` | Retire the original full Perl frontend. Native `fvwm-menu-desktop --configure` owns settings. |
| `bin/fvwm-menu-desktop-config.fpl.in` | Retain the small FvwmPerl entrypoint adapter to the native settings provider until #42/M6. It is not the retired frontend and still needs the separately retained framework when explicitly invoked. Direct `PipeRead` needs no Perl. |
| `fvwm-script-ComExample.pl` | Retire the replaced Perl FIFO example producer; retain the native Script consumer and installed Go helper. |
| `fvwm-convert-2.6` | Pre-FVWM3-only migration: remove Go package, Perl template, test-only producer/targets, install tag and manual/catalog targets together. Existing FVWM3 configurations do not need conversion. |
| Other native FvwmScript examples | Retain BaseConfig/BellSetup/Buttons/Colorset/Date/FileBrowser/Find/KeyboardSetup/PointerSetup/Quit/ScreenDump/ScreenSetup/WidgetDemo: these exercise current Script commands and system tools; age is not retirement proof. No converter command is referenced by these consumers. |
| XDG/directory menus, XPM, ICCCM, no-monitor root geometry and OS branches | Retain supported FVWM3 and required Linux/BSD behavior. This batch changes no C, rendering, window state or dependency linkage. |
| Go `xdgsource/legacy.go` and desktop legacy preference readers | Retain supported old settings migration; filenames are not evidence of unused behavior. |
| FvwmPerl/perllib/fvwm-perllib and Perl metadata | Retain under #42/M6, pending #50 scripting disposition. Remaining Python controllers are M2; owned build/test tools M7. |
| `contrib/{randr-tool,status,xsession,coccinelle}` | Retain current monitor/status/session/developer helpers; none is the pre-3.x converter. Broader tool retirement stays with its owner. |
| Historical NEWS/CHANGELOG, dated receipts and translation catalog messages | Retain provenance. Remove the retired frontend from active gettext extraction inputs; do not rewrite historical translations or evidence. |

Upgrade: the converter is no longer supplied. Migrate pre-FVWM3 files with an
older FVWM release first. Preserve user configuration and Git history. Fresh
stages omit retired programs; package upgrades must remove old owned files using
the previous package manifest (see INSTALL.md). No automatic filesystem cleanup
of arbitrary installed prefixes occurs.

## Independent CLI references

`tests/fixtures/{directory,xlock}-cli.json` captures the original Perl programs
from commit `6d8db4199c207e22ddac86c9641bcea17262527e`, before deletion.
Each file records the original source SHA256. No Go candidate was run to
create expected bytes. There are 48 directory reference cases and 21 xlock
cases; additional native-only ownership/deadline/error checks remain in the
drivers. Fixture roots/executable names are normalized. Directory size comes
from independent `stat` at execution; native enumeration/stable-partition order
is checked against independent `os.listdir`, with frozen command rows compared
as a set only for those inherently filesystem-specific order modes. Fixed
sorting modes and all other reference output remain exact.

`utility-contracts.json` retains all 11 surviving expected values unchanged;
only four retired converter entries are removed. Version/help wording
normalization preserves historical reference bytes without requiring a Perl
program. The Script FIFO peer already asserts exact independent replies,
ownership and failure controls; only its duplicate retired producer run is
removed. Native-X clients and settings/application-argv checks remain, with
PATH sentinels that fail if the provider attempts retired interpreter or image
converter commands. The independent fixture application uses an absolute
Python shebang; Python remains test tooling, not the provider implementation.

No new rendering is introduced: retain the source-bound three-theme gallery
and prior utility/provider qualification for unchanged scope. The M1 report
records fresh compiler/profile/install/source/native-X outcomes separately
from retained core/rendering/sanitizer evidence.
