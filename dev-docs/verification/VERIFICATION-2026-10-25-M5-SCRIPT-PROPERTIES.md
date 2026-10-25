# M5 Script property ownership — 2026-10-25

Owners: SCRIPT-01 #4 / CLEAN-01 #34. Base merged PR248:
`b808a97dc75d94b5d49b44027dfe6d9db54616ce`.
**Focused checks pass; broader owner/release acceptance remains pending.**

Repeated header title/icon/font/four colors and widget Type/title/SwallowExec/
icon/font/four colors release their previous owned string before adopting the
next token. Last-definition-wins, named-color/colorset ordering, configuration
syntax, token IDs and parser tables remain. Header WindowLocaleTitle now owns an
exact duplicate of FGettext's result before releasing scanner input; padded and
multiline catalog bytes survive. Widget LocaleTitle keeps its existing
FGettextCopy/CopyString normalization and releases the consumed scanner input.
The shared copy/translation helpers are unchanged.

BuildGUI releases five heap base font/color defaults when parsed overrides replace
them. ChooseFunction borrows each Type token until it returns successfully, then
BuildGUI frees it and clears that parser slot. Final widget strings still transfer
to XObj and retire through existing widget cleanup; final base font/colors/title
stay live. There is no partial-parser/error-stack, base-X/font teardown, locale-
path change, new UI, configuration migration or retry of blocked pathname work.

## Baseline and one combined checkpoint

The actual old parser/scanner control observes 66 selected scanner allocations
plus three widget translation copies: 69 retained owners. It verifies all sixteen
final properties before private post-observation cleanup. The candidate observes
52 consumed scanner owners and four superseded translation copies released;
sixteen final owned values remain correct, then explicit test cleanup leaves zero.
Controlled translation returns padded/multiline catalog data or unchanged padded
input. The header is byte-exact; widget copy uses actual CopyString semantics.
This is controlled translation coverage, not a real external catalog installation.

A private probe links actual old/new module objects. All seven old-module X cases
reproduce zero replaced-default/consumed-Type releases. The frozen candidate
passes all seven: final shutdown, dead producer, Init/event/Periodic Quit, refusal
before preparing the fourth widget, and refusal during fourth-widget preparation.
Five defaults retire in every case; three successful Types retire on pre-prepare
refusal, four on every other case. Existing cleanup retires all twenty watched
widget strings on pre-prepare refusal or twenty-six otherwise. Six final base
strings remain live. Explicit/inherited strings, colorset precedence and callback
reads are checked. These are selected owners, not a whole-module leak claim.

The 546 initial baseline identities plus four unchanged fixture helpers are
verified; all 552 final inputs match source hashes/modes. One reused normal GCC
13.3.0 Linux ARM64 profile builds the module, private probe and six units:
script-property-owner, script-parser, script-value-owner, script-widget-owner,
script-block-owner and script-binding-owner. All pass. The new seven-case X probe
and existing actual-module widget lifecycle fixture pass. The property unit also
passes Clang 18.1.3 ASan/UBSan with leak detection enabled. Actual options and
compile commands are in the [receipt](VERIFICATION-2026-10-25-M5-SCRIPT-PROPERTIES.json).
Combined candidate commands total 5.572 seconds; every frozen-candidate gate
passes on its first invocation. No platform/install/theme matrix is rerun.

Pinned Bison 3.8.2 / Flex 2.6.4 regeneration uses existing generate.py. Final
script.c matches the selected actions and generated line metadata; script.h and
scanner.c remain byte-identical. Initial generated candidate and reviewed final
regeneration are both retained.

Controls found two fixture errors before candidate dispatch: Font braces are
literal, and WriteToFile requires a STR/VAR pathname rather than brace GSTR.
Corrected font lines and initialized $file preserve the intended test. Independent
source review caught whitespace trimming in the initially proposed header copy
and a possible address-reuse hazard in the private observer. Exact header copying,
a real CopyString widget oracle and one-shot parser observation fix both. Failed
control sources/logs and the reviewed corrections are retained. The first local
repository check also saw the report link before this report existed; completed
documentation resolves it. Repository checks/lint and final evidence review are
recorded at delivery; no additional runtime rerun is implied by documentation.

Canonical private archive: October 25 `m5-property-ownership/native-evidence.tgz`,
192 payloads plus manifest, 1,918,318 bytes; SHA256
`e387bd6c2ddd7f5daa86fbc1e3b96470e19a35386f57a76fc51f6f34d19d61c1`.
Every payload hash, size and mode is verified. It includes source/control inputs,
failed and successful controls, generator receipts, actual profile/compile/output
identities, raw candidate results and periodic performance samples/runners. The
193 guest capture files, 25 original performance capture files and duplicate
archive are retired after exact manifests and no-active-reference checks. Host
canonical evidence, reusable source/build caches and PR243/PR248 performance
reference binaries remain.

The [due PR243→248 comparison](PERFORMANCE-2026-10-25-PR248.md) is complete before
this candidate: selected Script resident/private-dirty medians fall 260 KiB,
ready-time ranges overlap, and identical browser bytes show no code-speed gain.
PR248 is the next reference at 0/5; this candidate merge makes 1/5.

Separate final source/raw-evidence/report review passes with no remaining
actionable findings. Canonical manifest, all 552 inputs, baseline source, raw
commands/profiles/scenarios and performance calculations were independently
checked. Requested Temper analysis/implementation/review model settings follow
PROJECT.md; actual runtime metadata is unavailable and is not claimed verified.
No new UI, so no screenshot/theme pass. No fresh BSD/AMD64/install/full default-X,
whole-parser/module leak-clean or release claim. Guest execution does not establish
separate native-CPU/hardware/toolkit/compositor qualification. Broader #4/#34 stay
open; #31/#35/#36 remain completed and #93 not planned. No newly completed broad
issue is claimed solely because this owner slice passes.
