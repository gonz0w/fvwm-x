# Modern parser generation: ordinary configuration parity

Historical focused generation checkpoint. The later
[runtime adoption report](VERIFICATION-2026-10-11-PARSER-ADOPTION.md) tracks the
modern generated runtime and developer-wrapper qualification. Original results
and pending wording below describe this October 8 source only.

Tracker owners: GEN-01 / MAINT-02. Focused candidate preparation passes on
Linux, FreeBSD and OpenBSD, AMD64 and ARM64. Generated runtime sources are not
adopted by this checkpoint.

## Source and generation

The actual grammar, scanner input, committed generated owners and type header
match the independently qualified `3c37` source fingerprints. The existing
candidate tool generates with Bison 3.8.2 and Flex 2.6.4, records both versions,
and uses stable relative names. Two fresh Linux generations match byte for byte
for `script.c`, `script.h`, `scanner.c` and `generators.json`.

Evidence is under
`builddir/codex-2026-10-08/parser-generation-maintenance/`. It includes the exact
candidate files, comparison probe/runner, compiler logs, parsed output and five
complete native guest archives. `linux-arm64-evidence/summary.json` records the
generated-source hashes; all five other archives validate those same hashes.

## Native ordinary configuration checks

The probe parses configurations without executing their actions. It serializes
window properties, strings, variables, widget properties/flags, case IDs,
instruction IDs/arguments and nested blocks. It represents child blocks
structurally rather than comparing process-specific addresses.

All thirteen actual installed Script examples from the accepted source parse
successfully with both the existing and newly generated parser. Their serialized
structures match exactly on each of the six targets: 78 configuration comparisons
and 156 successful parse invocations. Parsed-source and output hashes also match
across those targets.

Each native check compiles the candidate parser/scanner and ordinary probe with
the accepted target's recorded compiler flags, and links unchanged helper/library
objects from its own `3c37` build. Their identities are checked before and after.
No accepted build object, executable or source is overwritten. This explicitly
reuses verified objects; it is not a new clean whole-project build.

No focused generation, compilation or ordinary configuration failure occurred.
No configuration compatibility decision is pending from this scope.

## Remaining adoption gates

The ordinary corpus is not every supported Script construct or user theme.
Error/recovery diagnostics, the complete parser policy corpus, actual module/X
execution, installed runtime and relevant optional-profile checks remain before
adopting generated sources or removing the old syntax helper. Normal release
builds must remain generator-free. No dependency floor, grammar, installed parser
or runtime behavior was changed by this candidate check.

Further boundary-condition implementation remains paused under the user's
work-selection direction. This maintenance work does not resume the queued
`a653`/`3074` matrices or claim new native Linux release sanitizer CI.
See the [tracker](../PROJECT-TRACKER.md#current-checkpoint-status) and
[maintenance guide](../../tools/maintenance/README.md).
