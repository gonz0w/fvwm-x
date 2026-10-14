# Optional desktop recipes

These examples complement an existing installation. They do not add a mandatory
package or start a service. Read each recipe's prerequisites and activation notes;
the recipes below are source-tree contributions, not automatically installed
default configuration.

| Area | Recipe | Purpose |
| --- | --- | --- |
| Locking | [locking](locking/README.md) | Existing xlock/xlockmore appearance and manual action; commented OpenBSD xidle startup. |
| Displays | [Autorandr](displays/README.md) | User-captured display profiles, manual actions and commented initial-login detection. |
| Composition | [Slate Picom profile](compositors/slate-picom.conf) | Optional shadows and fades using an existing compositor. |

The baseline remains usable without these packages. FVWM's proposed native
notification/tray services are tracked in [the project tracker](../dev-docs/PROJECT-TRACKER.md),
not replaced here by external notification/tray defaults. Legacy `status/` and
`randr-tool/` examples remain separate from these recipes and from the baseline.

Approved next direction: the default install supplies the portable session
foundation; a comprehensive optional D-Bus contribution extends the same
configuration and session UI. Its enablement/packaging are not implemented yet.
See the [session contract](../dev-docs/decisions/SESSION-INTEGRATION-2026-10-06.md)
and SESSION-01/SESSION-02 in the tracker. Existing recipes above do not start
a bus or provide this future lifecycle management by themselves.
