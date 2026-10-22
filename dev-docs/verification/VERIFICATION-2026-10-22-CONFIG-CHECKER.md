# Non-executing desktop configuration checks — CFG-02

This first CFG-02 subset checks literal desktop settings and presents source
reports without executing the configuration. The functional checkpoint is
locally qualified; full FVWM-language checking and general core runtime source
diagnostics remain open. [Machine-readable receipt](VERIFICATION-2026-10-22-CONFIG-CHECKER.json).

Three related features ship together:

- A bounded, non-executing checker validates the seven registered literal
  `*FvwmDesktop:` settings and follows literal `Read` filenames with optional
  `Quiet`. It shares the running helper's value registry and compares explicit
  final day/night values. It does not load saved preferences or predict the
  complete effective runtime state.
- Diagnostics identify the owning parser, filename, logical start/end lines and
  the full include chain. The installed helper's offline
  `--check-config FILE [--json]` entry point needs no display. Text escapes
  control characters in filenames; JSON schema 1 includes observed source hashes.
- The settings center offers **Check local.config...**. Existing bounded workers
  perform the file analysis; the main module thread presents four diagnostics
  per native Form page. Opaque result tokens and page bounds refuse stale
  callbacks. One active check and one result snapshot bound retained work.

The checker never dispatches FVWM commands, evaluates functions/expansions,
starts modules or runs shell/PipeRead/Lua actions. Unsupported commands and
module syntax are **unchecked**, not invalid merely because this checker does
not recognize them. Literal `Read` searches user/data/current directories as the
core reader does, rather than the including file's directory. Inode identity
rejects active include cycles while allowing later repeated reads. Checking writes no source or preference file. Configuration formats,
dependencies and native module ABIs remain unchanged.

The limits are 4 MiB total input, 64 file visits, eight active source levels,
10,000 physical lines, 1 MiB per logical line and 64 diagnostics. Exceeding a
limit reports incomplete checking. A three-second elapsed budget is checked
between reads/lines and cannot preempt an individual filesystem call. Only
regular files are read; nonblocking opens avoid FIFO waits. File hashes identify
bytes observed, not an atomic cross-file snapshot or later runtime contents.
Exit 0 covers only the declared subset, 1 reports supported-value/source errors,
and 2 reports partial/limited checking or CLI usage errors.

## Frozen source and combined qualification

Runtime/policy/observer candidate: `886d9e339210477f5a5f3b631dcc9a8aa5d1da5f`;
2157 inputs; source archive SHA-256
`64601573ba12a90ff2e99f34bfe1a04892203b29055e16d7d4967edc9b032d7a`.
The base is merged PR152, `29f2727f7625727857ce8af6921d39d5ebb46e78`.
This is the final freeze after the review correction below; no runtime or
observer correction follows it. Publication adds only
README/tracker/evidence documentation and does not repeat the runtime matrix.

One combined checkpoint runs two affected registered entries on each of ten
profiles across six targets: Linux AMD64/ARM64 GCC/Clang, Linux ARM64
software/minimal, FreeBSD AMD64/ARM64 and OpenBSD 8.0 AMD64/ARM64. All 20 pass.
The unchanged native foundation retains 2931 composed PR152 entries, keeping
2420 standard / 531 optional results. These are composed results with the
original foundation's scopes and gaps, not a new full-suite execution.

Each profile uses incremental `meson compile -C BUILD -j 2`, followed by
`meson test -C BUILD --num-processes 1 --print-errorlogs desktop-policy
desktop-baseline-xvfb`. Staged sessions invoke `tests/live_x/desktop_live.py`
with the staged core/helper and `--settings-only`; BSD default-server commands
reuse the owning authenticated fixture. Each canonical capture includes exact
commands, executed helpers, source manifests and per-file payload hashes.

All 150 native ELF targets and 77 catalogs retain exact PR152 identities. Ten
helper stages verify all 927 installed identities; only generated FvwmDesktop
files differ. Eight stages combine the new helper with exact unchanged prior
installed files via additional hard links; the helper uses independent storage,
and no prior inode is edited. Both disposable OpenBSD stages use fresh complete
Meson installs. Sources, reusable builds, prior stages and VM disks are preserved.

Ten built desktop-X sessions, ten staged settings-X sessions and four
authenticated default BSD-X sessions pass. Unauthenticated clients are refused.
Each affected X fixture invokes the actual generated/staged helper CLI without
DISPLAY, verifies continued child-source lines 2–3 and its Read caller, opens
both native report pages, then verifies source/preferences unchanged and the
side-effect marker absent. The supported failures and unchecked dynamic
constructs are intentionally supplied as file data after startup; the core never
Reads that test input. Unexpected observer X errors remain failures.

Forty-two policy cases per profile cover supported values, missing/malformed
sources, inode cycles, repeated and nested reads, search order, continuations,
NUL/encoding handling, all resource caps, explicit schedules, offline CLI exit
codes/escaping, worker/main-thread separation, bounded paging and stale/busy
refusal. Existing typed preference and grouped-editor coverage remains included.
Two actual staged Linux PNGs show both report pages; payload hashes and paths
are in the receipt. These are running-application captures, not mockups.
Disposable OpenBSD jobs exit QEMU with status zero and remove private job keys.

## Failure disposition and remaining gates

Final review of the initial `c0e72459d` candidate finds a checker/runtime
option-splitting mismatch: tabs can be falsely accepted, and leading Unicode
whitespace can be falsely rejected. The correction reuses the existing
`desktop_configuration` parser and extends the existing whitespace regression
case. Its two affected entries and helper acceptance gates are rerun across all
six targets; native foundation/build identities remain retained. Initial passing
results remain preserved but are not substituted for the corrected checker.

The first Linux ARM64 software staged session times out at the existing
palette/reset echo barrier before the grouped editor and the new checker. Its
original failed capture remains canonical and fully hash-validated. One
unchanged isolated staged control passes with unchanged deadlines and exact
source/configuration/native/stage identities. Only the unfinished minimal
profile then builds/tests/stages; the six already completed GCC/Clang/software
registered entries are retained. No failure is relabeled and no scheduling/root
cause is established. Emulated AMD64 jobs are serialized for this checkpoint.

No new native memory or ownership contract changes, so the exact native
sanitizer evidence and known PR152 gap are retained without repeating its
existing exit diagnostic. The leak-enabled PR152 functional exercise reports
fontconfig/Xft allocations in exiting native Forms; a standalone existing Form
without the helper/editor reproduces the same family. That sanitizer gate is
not clean. CLEAN-01 keeps process-exit font cleanup in Later; no detector is
disabled, no suppression is introduced and no fresh clean sanitizer/release
claim is made for this batch.

The initial portable verification passes 177 of 191 entries with 14 environment
skips; its unaffected maintenance harness/inputs retain that result after the
desktop-only parser correction. Fresh final documentation/repository checks and
local diff review pass. Linux Asciidoctor renders the
updated man page; host mandoc formats it. Lint exits 3: the existing one mso and
one LINKSTYLE diagnostic plus eight section-spacing warnings. The added
CONFIGURATION CHECK section adds one warning in the existing spacing family;
DOCS-01 retains formatter scope. No new diagnostic family appears.

General runtime command/argument/source diagnostics remain under CFG-02.
CFG-01 live preview/recovery still needs observable shared-core generations and
conflict recovery with IPC-02, which is the next feature checkpoint. Visual
polish, general maintenance and performance tuning stay deferred. PR149 is the
performance reference at 3/5 merged implementation checkpoints; no benchmark is
due. Native-CPU sanitizers, physical hardware, toolkit/compositor behavior and
release acceptance remain separate. Owner-controlled guest capture/archive
copies are retired only after canonical integrity/payload/identity validation;
failed attempts remain represented in the preserved host evidence.
