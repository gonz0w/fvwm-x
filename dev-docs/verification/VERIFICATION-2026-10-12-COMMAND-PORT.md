# FvwmCommand Go client checkpoint — 2026-10-12

Status: **affected six-target checkpoint locally qualified**.
Owner: RUNTIME-GO-01, coordinated with PROMPT-01 and stage 3's incremental
installed-client ports. This is one client port, not completion of UTIL-01,
Perl retirement, the desktop controller migration or release qualification.

## Adopted batch

1. Go-enabled builds install `FvwmCommand` from
   `bin/FvwmPrompt/cmd/fvwmcommand`. It shares Prompt's qualified
   standard-library `transport.Session`; no separate connection framing,
   unchecked single write or fixed single-read JSON implementation is added.
   Command arguments, stdin mode, explicit socket, info/diagnostic flags,
   successful help/version output and ignored FVWM2 options remain available.
   `--version` works, and `--verbose` is a diagnostic alias rather than the
   legacy client's unhandled-option assertion.
2. Endpoint discovery follows the current MFL layout: explicit path, exported
   socket, configured directory, XDG runtime directory, or per-UID temporary
   directory, with normalized display bytes. The retired shared temporary
   directory is not probed. Explicit/exported endpoints work without DISPLAY.
3. Commands are complete newline frames, bounded by the shared 1000-byte
   one-line contract. Stdin accepts CRLF and an unterminated last line and
   preserves order. A reader drains events during input, keeps one first
   record, and is closed/joined on every return. Incremental strict UTF-8 JSON
   handles fragmentation, coalescing and records larger than 32 KiB; numeric
   decoding preserves exact integers. Connection/write limits and a finite
   five-second post-input response wait are explicit. The first record is
   usually the connection profile, **not an execution acknowledgement**.
   Explicit stdin mode can wait for user input until EOF; its lines are bounded.
4. Meson builds/tests/stages the Go client offline with the existing optional
   Go feature. The disabled-Go profile retains the configured Python client.
   Its removal and the future default/minimal runtime profile remain stage 5
   work. No interpreter, extra GUI package or new library is needed by the Go
   executable. The expanded AsciiDoc manual documents options, paths, limits,
   errors and examples; installed command/manual names remain unchanged.

The shipped FvwmCommand references were reviewed; existing examples use ordinary
one-line commands. No FVWM configuration-file/format migration is required.
No core, native module, header, authoritative X state or packet ABI changed.

## Source composition and evidence

Artifacts: `builddir/codex-2026-10-12/command-port/`, including exact source
archives, scope maps, executed runners, logs, installed clients, actual Meson
JSON records, manual output and the independently checked validation summary.

| Snapshot | SHA-256 | Disposition |
| --- | --- | --- |
| Accepted predecessor | `ebd123efac3bf5096552827af28bab2fbc0a66897a36a594cc1070712da09003` | CI/coverage checkpoint |
| Initial client | `04bc114aab49d51249092b8d96644b6c4d71870a533e4a6eda30101ba3caefaa` | Original peer fixture retained |
| Final client controls | `f274a793b6dba89ecafb13c72b44419b07765d8e02d661b894930d1d3d696889` | Only peer fixture differs from initial |
| Final composed source | `295e87d89e5d9f61de8ab9b94f2d694536ff215858b70b7c8681624c4e20a59b` | Only manual title/metadata/SEE ALSO polish differs from final controls |

The final archive contains 1422 files and nine owning paths. Shared bin/tests
Meson files compose only this batch's blocks with the accepted predecessor;
another thread's newer targets are excluded. Every C/header byte is unchanged.
Reused core/MFL build identities match the predecessor's accepted native
**tested**, rather than rpath-transformed installed, identities exactly.
All archive file hashes/modes, saved execution helpers and complete gzip streams
validate. Manual-only final composition is verified separately; unchanged
runtime checks were not repeated for that documentation delta.

## Completed affected gates

| Coverage | Result |
| --- | --- |
| Native Linux, FreeBSD, OpenBSD on AMD64 and ARM64 | All six pass |
| Registered Go package tests | 18 package results; three packages per target, including new CLI policy tests |
| Independent command CLI peers | 162 final cases: 27 per target |
| Prompt CLI regressions | 60 cases: 10 per target |
| Actual core/MFL integration | 14 invocations / 84 cases over ten accepted core profiles and four default BSD servers |
| Tagged command installation | Six Go and six disabled-Go legacy stages; built/staged Go identity and executable mode validated |
| Successful legacy CLI parity | 12 exact help/version output/status comparisons on six targets |
| Go test race/vet | Four controls on Linux AMD64/ARM64 |
| Actual race-instrumented command client | 54 peer cases and two further actual-MFL invocations on Linux AMD64/ARM64 |
| Dependency/build metadata | Only standard-library/project transport imports; offline vendored build, Go floor unchanged |

Linux ARM64 uses existing Go 1.22.2; the other five targets use the previously
qualified isolated Go 1.27.0 SDKs. No SDK or package installation was needed.
Linux core profiles are GCC/Clang on both architectures, plus ARM64 minimal and
enabled-software profiles. Running the Go client against a minimal C build is
distinct from changing that profile's installed runtime language.

BSD default-server checks confirm FreeBSD scfb on both architectures, OpenBSD
ARM64 wsfb and OpenBSD AMD64 vesa. They preserve any existing Xorg and terminate
only the server PID owned by the fixture. Echo observations from an independent
subscriber prove command delivery and stdin order; invalid input returns failure
and the real WM remains responsive. Full unrelated C suites are reused through
source/binary identity rather than rebuilt/rerun.

## Manual and small startup sample

Linux AMD64's existing Asciidoctor produces roff and optional HTML from the final
AsciiDoc. Linux and FreeBSD mandoc render the same generated page at width 60.
Hashes: man `e8335b2b9ba013fc53f8d090c9750e0790599477b11d6e1638d1a944778346a2`;
HTML `32f4f218f0101362d7f30f1e4ff8b26f298944280dbb4cc8ab1e4756982f039f`.
The command retains its existing manual-manifest/install entry.

Mandoc lint returns 3 on both targets: Asciidoctor's generated `mso`/LINKSTYLE
extension preamble, intentionally empty reproducible TH date, paragraph macros
and long roff input lines are retained/classified. Actual presentation includes
all new sections and examples. This is **not a clean lint**, full man-package,
HTML accessibility or ASD-STE100 certification claim; shared converter/manual
rewrite work remains DOCS-01-owned.

Twenty measured `-h` launches after one warm-up, using the installed binaries:

| Environment | Go median wall time | Python median wall time |
| --- | --- | --- |
| Linux AMD64 QEMU/TCG | 29.8 ms | 193.9 ms |
| FreeBSD ARM64 QEMU | 0.894 ms | 13.94 ms |

Raw per-child CPU and platform RSS observations are saved with executable hashes.
Some FreeBSD Go RSS observations are zero; no reliable universal memory reduction
is inferred. These are small non-resident help-command samples, **not desktop
input latency, full-session PSS, startup under load or resident-controller GC**.

## Retained failures and bounded recoveries

- The initial burst peer serialized a large event write before reading commands.
  FreeBSD correctly observed a client close before every unsolicited event was
  sent; the fixture treated it as an error. The final fixture sends and reads
  concurrently, permits the producer's expected late event-write closure, and
  still requires byte-for-byte receipt of all 1000 commands. Both FreeBSD
  architecture failures and original fixture remain. Final peers pass all six
  targets, including the actual race-instrumented Linux clients.
- All three AMD64 emulators timed out the initial registered Go package test
  while using Meson's separate cold test cache. Existing native build caches
  were copied to that owning test cache before affected retry. Registered
  180-second unit and 90-second client limits were unchanged. Original timeouts,
  test logs and executed setup are retained; no product timeout was raised.
- A resume helper initially omitted its `base` variable; the harness error is
  retained. Its corrected continuation checks source identity and preserves
  initial evidence before completing the unfinished target work.
- The initial manual runner assumed Linux ARM64 had mandoc/groff; those commands
  were absent. Inventory selected the already provisioned Linux AMD64 renderer,
  and transferred its exact outputs to FreeBSD's existing mandoc. A raw-render
  assertion initially overlooked terminal overstrikes; presentation checks now
  normalize those, without changing the rendered manual. No package was added.
- Meson JSON test names carry version-dependent project separators. The
  validator normalizes those names and checks actual statuses/output, rather
  than treating the separator as a product/test failure.

## Remaining owners

RUNTIME-GO-01 still owns the full desktop/launcher/session/screenshot/menu ports
and default/minimal runtime migration. UTIL-01's fifteen Perl utility fixtures
are qualified; those utilities are not ported by this batch. COMPAT-02 still
owns Perl API retirement. CI candidate assets/profiles and actual native release
CI remain open; there is no remote or publication action. Physical/toolkit/GPU
coverage and native Linux release C sanitizers remain separate requirements.
