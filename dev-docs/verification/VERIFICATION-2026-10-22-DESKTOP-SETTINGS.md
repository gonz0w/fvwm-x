# Typed desktop settings checkpoint — CFG-01

Combined qualification is pending. This checkpoint follows merged PR150.

Three related behaviors: one registry for existing setting types/defaults/scopes
and bounded queries; validated saved/effective changes with private versioned
persistence, scoped reset/reload and stale/write-failure refusal; a native menu
configuration-center consumer with truthful explicit-override reporting.

Two prerequisites were reproduced during focused actual-X checks. `AddToMenu
Greyed` freed its caller-owned label and leaked the replacement; omitted labels
also dereferenced NULL. The bounded repair retains the caller token and owns
only its replacement. The helper had emitted Greyed as an action, making info
rows selectable, and ignored FVWM's normalized colon-free configuration packets.
Both shipped consumers and the direct/native regression cases change together.

Preview is explicitly unavailable. Confirmation/cancel/timeout, owner recovery
and conflicting-generation recovery remain CFG-01 follow-up work. No new setting
domains, generic IPC API, toolkit, core thread or Go runtime port is introduced.

Focused Linux ARM64 GCC passes 20 policy cases and the actual desktop fixture.
The fixture verifies registry queries, scoped reset, stale edit refusal/reload,
bar pixels, keyboard activation/cancel, restart persistence and explicit overrides;
32 create/empty/quoted/destroy Greyed sequences exercise label lifetime.
Earlier failed query/menu/binding/ownership/configuration attempts are retained
under the owner capture; they are not passing acceptance evidence.

A single frozen candidate will run the complete registered default suite and
fresh installs on six targets/ten compiler/feature profiles, with scoped guest
sanitizers and four authenticated default BSD displays. Native-CPU sanitizer,
physical hardware, toolkit/compositor and release checks remain separate.

Performance reference PR149 is at 1/5 merged implementation checkpoints after
PR150. No comparison or tuning is due.
