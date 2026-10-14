# Full XDG provider and settings integration

Status: locally qualified and adopted in the optional Go profile. All six
targets and eight affected compiler profiles pass the composed checkpoint.
Frozen final source is `04c1`; no configuration migration or new mandatory
library is introduced. Native release CI and physical support checks remain
separate release requirements. See the [tracker](../PROJECT-TRACKER.md#current-checkpoint-status).

## Scope and behavior

RUNTIME-GO-01 / UTIL-01 moves `fvwm-menu-desktop` and its settings frontend
together to the existing optional Go build. The implementation uses the Go
standard library and native FvwmForm; it adds no mandatory GUI/image toolkit or
Go module dependency. The Go-disabled build retains the original Python provider
and Perl frontend. The installed FvwmPerl entry point delegates to the native
Form when Go is enabled; framework retirement remains COMPAT-02 work.

The provider covers bounded desktop/XML input, XDG discovery and precedence,
merging/Move/legacy directories, ordered allocation, visibility/localization,
layout/inlining, collision-safe menu names and inherited icon lookup. Metadata
becomes literal argv in owned action catalogs. Regeneration retains prior action
records and refuses saturation rather than invalidating existing exported menus.
Catalog writers use a nonblocking owned lock; settings/results are complete
private publications and results are consumed once. User-configured terminal
commands retain their shell syntax; desktop arguments remain separate data.

Acquisition runs in an isolated process group with a five-second default and
finite CLI/environment overrides. Only completed buffered output is published.
Application lifetime is outside that deadline. The Go memory setting is a soft
GC target, not a hard address-space limit. Existing decoder feature detection
controls PNG/SVG/XPM output; PNG scaling uses the standard library, and native
SVG sizing uses existing FVWM support.

Native Form gains bounded InputAppend/CommandAppend and encoded field/result
substitutions while retaining ordinary full-name lookup priority. A shared owned
Read helper dispatches complete logical lines up to 1 MiB, preserving continuation,
ordering, close-on-exec and the existing 50,000-command limit. NUL/I/O/overlong
input fails before partial-line dispatch. This fixes accidental 1024-byte
fragmentation; the broader DATA-01 reader audit remains separate. No configuration
file format migration is required. Export still accepts the existing documented
full output paths, home shorthand and explicit FVWM_USERDIR prefixes.

Scoped Form startup exposed a pre-existing shared `XCmpToken` bsearch callback
type mismatch. The callback now has the standard signature, preserves the
first-pointer-member lookup convention and uses unsigned bytes with ctype.
Direct units cover string arrays, first-member records, delimiters, unknown/null
inputs and high bytes. Shipped users are Form, Animate and Event. They are rebuilt
and qualified with the affected core/MFL consumers at the final checkpoint.

The optional Go verification script now compiles/stages the provider and Form,
selects both registered desktop-menu checks and verifies the installed CLI.
Its six native contract controls cover that command/staging selection.

## Frozen source and evidence composition

The predecessor is the accepted Script communication source `6fcc241762260f97c4c195080ffd5db8a836f4b1dbef9850030456bf7d8e2625`.
It preserves the other chat's desktop/runtime work outside this batch.

| Source | Meaning |
| --- | --- |
| `5aa5fdf4cc65a166c5c54894cd8fa3edbabb82345148f2ee3c1b5df5ddbf754c` | Initial full provider/Form/Read implementation and manuals; 1501 files. |
| `b64afbe664dc088b0d57f20a36b2f8787211d61f59a21b8f0cb3e9a0d246d82d` | Two test-only repairs: executable paths and copied shared verification helper. |
| `42a0e1a9cd493f1ddd3205a3bbcd8288217c31ca3342d107bdb1ba1f17ad30d9` | Test-only Form-close readiness on OpenBSD. |
| `138bff518c5730683d0de29f8c09d1638d64bb7ed24a3e4ddb65953591cf0acd` | Final test-only DestroyNotify readiness; avoids querying retired child windows. |
| `cb13f44e79968c366897a816aa1e149f0320ba3e5fea84da4eb26d30e07b357c` | Shared callback correction, direct unit and registration; four changes from 138b, 1502 files. |
| `04c1ba2f4e25a0a6059621fa4a590eb5e4ac5e0d69075a99c7b1c09d7bf2f974` | Optional Go gate/contract integration only; two changes from cb13, all runtime/manual inputs unchanged. |

Evidence is under `builddir/codex-2026-10-12/xdg-provider/`.
`scope-*.json` records every frozen byte hash and mode. The combined validator
checks source, executed runner/log hashes, registrations, passing rows and
installed binaries after the expected Meson rpath transformation. It explicitly
resolves OpenBSD ARM64's readiness-only successor's retained stage proof through
its predecessor archive. It does not claim the changing shared checkout was
qualified wholesale. Acceptance-status documentation upkeep follows the frozen
checkpoints; the current owning runtime/gate files match the final freeze.
These verification snapshots are not public release archives.

The scoped successor records its executed full source-byte assertions against
cb13 and exact instrumented binary hashes. Its source maps to the independently
validated Linux ARM64 token archive; it does not contain a second whole-source
copy. Minimal Go build inputs are byte-identical across the callback correction.

## Qualification

The composed checkpoint retains 1982 passing baseline full-suite entries and
adds 134 affected callback entries. It preserves ordinary passed rows and adds
only repaired/unfinished checks. Callback correction uses affected incremental
builds instead of another full matrix.

| Target | Baseline full suite | Final affected callback checks |
| --- | --- | --- |
| Linux ARM64 | GCC/Go 258; Clang/disabled 241 | 17 + 16 |
| Linux AMD64 | GCC/Go 258; Clang/disabled 241 | 17 + 16 |
| FreeBSD ARM64 | 246 | 17 |
| FreeBSD AMD64 | 246 | 17 |
| OpenBSD ARM64 | 246 | 17 |
| OpenBSD AMD64 | 246 | 17 |

Twenty-five final archives validate all their applicable frozen source hashes
and modes, executed runners/logs, registrations and rows. Baseline stages carry
120 native profile identities; affected stages carry 40 updated identities across
eight profiles. A separate final audit validates 15 installed native consumers
per target: five updated owners and ten retained unaffected consumers. The
installed provider/shim bytes are independently captured and rehashed locally.
Six additional native-X checks use the actual staged core, MFL, Form and provider
together, after install rpath changes. All pass save/regenerate/export with long
literal input and a literal desktop argv launch through the real menu/core.
Six installed legacy Perl entries open the new native Form. Four default BSD
servers pass with private Xauthority and refuse unauthenticated clients; their
fixture uses tested build-native core/modules and the installed provider.

The final optional Go gate contract passes all six units per target (36 units).
Each gate includes the provider/Form build, both registered menu checks, its
install tag and installed CLI. The full-suite source is the earlier implementation
plus test-only repairs; the later callback and gate changes are qualified by
their explicit affected checks. These counts do not claim a second complete
matrix at final `04c1` or a wholesale rebuild of unaffected installed consumers.

Six ordinary Go runs cover 60 package results; both Linux race runs add 20.
Each run passes 98 top-level tests and 421 named subtests, plus vet and a
standard-library-only provider dependency graph. Both
Linux architectures execute race instrumentation and their 15-case race CLI;
six installed providers each pass 15 CLI cases. Minimal
SVG/PNG/XPM/cairo-disabled provider checks pass the registered and installed
15-case CLI suites. Corrected local Linux ARM64 ASan/UBSan passes four
leak-enabled units and thirteen actual Form/Event/Read integration controls;
integration leak detection is disabled. This is not native release sanitizer CI.

Four affected man pages are generated by native Asciidoctor on Linux; BSD formats
byte-identical generated pages with native mandoc. Linux ARM64's container lacks
a roff formatter, so a host mandoc check covers that rendering. Generated HTML
and manual content checks pass. Mandoc lint reports unsupported Asciidoctor
macros and existing style/date warnings; this is not a clean-lint claim. The
website and ASD-STE100 manual rewrite remain deferred under DOCS-01.

## Retained failures and limits

Initial harness repairs, stale artifact-directory attempts and the earlier
recursive closing-window query failure remain recorded. The final fixture
watches DestroyNotify and then targets the live root; production dispatch did
not change for that repair. Three Echo acknowledgements added to test-only Form
output make completion ordering observable; shipped Form output is unchanged.

Initial scoped Form startup failed through the shared callback mismatch. The
original failing logs/fixture artifacts and corrected controls are retained.
FreeBSD AMD64 image-effects acknowledgement hit its five-second timeout; an
unchanged retry at that deadline passes. Linux AMD64 topology pressure hit its
100-ms xrandr timeout in both GCC and Clang profiles; unchanged retries at that
deadline pass. Causes remain unestablished; these are observations, not attribution to emulation or candidate
changes. OpenBSD AMD64's initial full job reached the 2400-second outer deadline,
with a native-gate contract timeout also recorded. Its valid completed rows are
retained. The continuation adds 39 passes; its native contract timeout and
image-effects acknowledgement failure remain archived. All six native-contract
units pass in isolated diagnosis (110.13 seconds total); the complete registered
contract then passes alone under its original 180-second limit. The
image-effects check passes alone with its original deadlines. Together with the separately bounded verification
harness, all 246 registrations pass. The timing causes remain unestablished.

The first final installed-byte capture used both Linux targets’ older initial
stages; their identity mismatches are retained. The corrected audit selects the updated
token-install stage on all six targets and additionally exercises its installed
native consumers. No production source changed for that correction. An unused
compiler-timeout continuation asserted because the Clang compiler completed
within budget; it dispatched no guest work and is not acceptance evidence.

This checkpoint adds no release publishing, credentials, remote CI result,
physical GPU/input/compositor or arbitrary third-party theme claim. Native Linux
release ASan/UBSan CI and physical/independent-client support qualification remain
explicit release gates. Broader XDG roles/autostart and desktop/runtime services
remain with their existing owners; this provider port is not their completion.
