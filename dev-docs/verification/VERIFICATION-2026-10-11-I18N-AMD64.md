# AMD64 translation-tool qualification — 2026-10-11

Tracker owners: I18N-01 / MAINT-02. Status: focused AMD64 generation/catalog-only
checkpoint qualified. This separately extends the
[ARM64 maintenance checkpoint](VERIFICATION-2026-10-08-I18N-MAINTENANCE.md).
It does not adopt/remove translation sources, committed GMO files or the legacy
helper, or replace the accepted parser-production checkpoint.

## Frozen source and checks

The 34-file candidate source has SHA256
`905309e09a86f3b51b4f6d0b16078161c1a890824d38f8cab2db64eaf79fa2f5`.
It uses the original candidate plus only the four previously accepted Language
header corrections. Byte comparisons verify those fields are the only PO
changes. Exact files, modes, runners, commands, complete successful archives and
both initial failed BSD attempts are retained under
`builddir/codex-2026-10-11/i18n-amd64-qualification/`.

All three AMD64 targets pass five focused unit tests each: ordered shared
language discovery, complete publication, source preservation, selection errors
and failed-operation cleanup. Real native gettext extraction on each target
matches all 138 retained legacy message keys. The three targets pass 66 direct
catalog comparisons (eleven languages in each of two fuzzy modes per target)
and generate 33 candidate PO updates. All four corrected Language fields appear
in both binary catalog modes. The legacy shell helper is not rerun here;
message parity uses the retained original key set.

Three clean catalog-only Meson fixtures use the actual unchanged `po/meson.build`
and candidate PO/LINGUAS inputs. All 33 regenerated and staged catalog identities
match their default candidate builds, with zero committed GMO inputs. Validation
also checks exact fixture PO/LINGUAS hashes and executed runner identities. These
are catalog-only fixtures, not clean whole-FVWM release builds.

## Retained attempts and environment preparation

Initial host preparation mistakenly selected the older core snapshot, which
predates the translation-test file and header follow-up. It was corrected to the
two owning maintenance archives before a target checker ran. A later validator
parameterization needed a conditional-path parenthesis correction; validation
was repaired without changing or rerunning native tests.

Both BSD AMD64 guests initially lacked the five gettext developer programs.
Their first real extraction attempts failed after five passing unit checks each.
Those failures and complete logs remain. Package preparation starts only after
each owning guest finishes and archives its parser/default-X gates; independent
Linux qualification can continue without mutating a running target.

FreeBSD receives gettext-tools 1.0, libtextstyle 1.0 and libunistring 1.4.2. Its
package manager also updates itself from 2.7.5 to 2.8.4, omitted by its initial
three-package dry-run summary. Before/after inventories show no other existing
package changed. OpenBSD receives only gettext-tools 1.0 and libtextstyle 1.0;
no existing package version changes. Normal repository/package-signature checks
remain enabled, and neither guest's clock is changed. These are developer tools
in owned QEMU overlays; base disks and accepted predecessor trees remain.

The affected BSD extraction/catalog/update recoveries pass with identical source.
Their five initial unit passes are reused explicitly rather than rerun. Fresh
catalog-only fixtures then regenerate/install eleven catalogs on each BSD target.

## Remaining adoption and release gates

Current follow-up: the [I18N-01 adoption checkpoint](VERIFICATION-2026-10-11-I18N-ADOPTION.md)
completes the later whole-source maintenance/distribution trial and helper/GMO
retirement. The focused source9053 results below retain their original scope.


Whole-source NLS/default/minimal/feature and clean source-release/distribution/
reference checks remain before legacy helper or committed-GMO removal. The
accepted GEN-01 runtime matrix covers its own frozen source and profiles; it is
not relabeled as this NLS adoption gate. Native release sanitizer CI and physical
localization/font/input checks retain their owners. No translated message text,
FVWM command or configuration format changed.
