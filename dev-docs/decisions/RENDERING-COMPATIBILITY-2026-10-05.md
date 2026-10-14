# Rendering compatibility — retain existing capabilities

Status: decided by the user on 2026-10-05. The rendering-removal options
discussed in the configuration review are declined, not queued work.

## Decision

Retain existing FVWM theme/configuration rendering compatibility. Do not remove
image-based titlebars or buttons, multipart titlebar decorations, or optional
SVG support as a code-size simplification. Do not restrict project-wide image
support to wallpapers or replace all existing image consumers with font icons.

Keep native drawing, gradients, image decorations, mini-icons and shared image
support available under their existing commands and build feature options.
Optional backends remain optional; this decision adds no dependency requirement.
The current default theme and configuration remain unchanged.

## Scope and backlog disposition

The alternatives above are closed as **not going to do**. They must not become
implementation tasks through inference from the exploratory source-size review.
Titlebar artwork concepts are deferred for later discussion, not approved theme
changes. Possible additive icon-font support remains a separate future discussion;
it is not a replacement mandate or an approved implementation in this record.

Existing image/dependency hardening and previously approved bounded-input policies
continue under IMAGE-01, DATA-01 and their library owners. Retaining rendering
capabilities does not reverse those specific approved policies or verification
requirements. Compatibility-preserving internal cleanup remains in scope.

The active queue and execution order remain in
[PROJECT-TRACKER.md](../PROJECT-TRACKER.md).
