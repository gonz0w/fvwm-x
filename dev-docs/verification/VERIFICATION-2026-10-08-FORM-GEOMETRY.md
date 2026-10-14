# Form Input preparation and combined storage checkpoint

Status: two Input stories are focused-qualified and join the three
[counted-array owners](VERIFICATION-2026-10-08-FORM-ARRAYS.md) before one combined
matrix. Final code-focused source1374:
`140fd7a7b03bcee1ee7488478f6c9d3f9647be1db7983356909613027b802787`.

Input parses the name, positive representable character width and optional initial
text before publishing an item. Decimal-prefix parsing and omitted initial text
remain supported. Zero, negative, absent and out-of-range widths return without
adding an Input or changing its ring. Pixel dimensions use wide arithmetic with
the existing font/padding formula and must fit X11 sizes 1..65535. The draw table
is prepared before publication; a refused pixel size may retain a reusable font
table, but adds no field. This does not validate the complete Form layout.

Pure controls cover decimal-prefix/escaped-name parsing, integer boundaries,
odd font rounding and exact X11 endpoints without large allocation or invalid X
requests. An actual Form config contains valid Inputs before and after refused
directives, including representable character width with unrepresentable pixels.
F12 emits both valid values, and the frame has only their two Input children and
one button. Timer, mapping, active commands and EOF remain usable. No old invalid
geometry path is executed. Supported configuration files need no migration.

Final normal/scoped Linux runs each pass seven C policy, two Python harness and
24 actual process/core entries. Both native BSD ARM64 overlays pass the expanded
Form controls. Complete 193/83/88-file archives strictly validate payload hashes,
modes, EOF, 1374 developer identities, eight consumed native identities, exact
runners and outcomes. C policy leaks are enabled; actual module leaks are disabled
and Python is not sanitizer-instrumented. The earlier array source and fixture
failure remain separately retained. Archive collection had an approval-review
deadline timeout; the one permitted retry succeeded without source changes.

The new manual paragraph is the sole later source difference. Existing Linux
Asciidoctor/FreeBSD mandoc render the text with unchanged eighteen diagnostic
messages and status 3. This is paragraph verification, not clean whole-manual lint.

The combined five-story source1374/seven paths is prepared at
`a1282410a70bdda1f87f104e963f3c481ec99696dec7cff500384836d64e76d7`.
Ten runners preflight required1560 standard entries/eight profiles, 120 installed
identities, 212 BSD default-X drivers (53 each), optional200/180 and scoped86
(29 C leak-enabled; 57 actual leak-disabled). These are requirements, not completed
coverage. Dispatch waits for strict preceding argument/data source0375 qualification;
no component-only full matrix is dispatched.

Artifacts: builddir/codex-2026-10-08/form-geometry-production and
module-form-geometry-production. Full layout sums, editing growth, command
expansion, shell callbacks and partial reads remain open. Native release CI and
physical hardware/toolkit/compositor checks remain separate.

The prepared five-story sourcea128 is retained and was never dispatched. Four focused-qualified command stories join it in the [nine-story processing checkpoint](VERIFICATION-2026-10-08-FORM-PROCESSING.md), preserving one combined matrix.
