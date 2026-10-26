# M5 Script base-window/runtime ownership

Status: composed focused checks and separate source/evidence review pass. Owners:
SCRIPT-01 #4 / CLEAN-01 #34. Neither broad owner closes from this finite family.
Base is merged PR253, `965787c6b4e0da2f3115d3df724a0778609ed691`;
canonical signature/DCO and reviewed tree
`660c3a6414b8a49eb700aae320f2e5d7713dc53b` are verified. Branch and
GitHub/Project cleanup are complete.

## Complete base-resource family

This continues the base resources explicitly excluded from widget teardown.
The base cursor caller reference ends after window creation; the parsed cursor
name and its temporary alias are cleared. Named-color allocation records explicit
success, including valid pixel zero; borrowed colorset pixels never acquire an
owner merely because their value is nonzero. ChangeMainColorset and final cleanup
consume the same successful-reference flags. The shared color parser/validation
remains one implementation, with existing Pixel-returning wrappers and a new
status result for owners.

Parser header strings transfer to the base and clear their old slots. At idle
shutdown, widget callbacks and records retire before the shared base GC, owned
colors, base strings, every populated script ID and scratch records. Parent and
launch IDs retain their bytes; a child copies its formerly borrowed argument.
Module arguments stay borrowed. The drawing scratch record's text/GC/colorset/
region remain borrowed. Partial preparation and repeated empty release are safe.
Active Init/event/Periodic Quit paths retain the base and aliases, matching the
existing callback owners. These paths are not claimed leak-free.

QuitFunc and the bounded selection-message grace period retain their ordering.
Selection cleanup uses the recorded registered atom, avoiding a NULL-name lookup
before child registration. DeadPipe keeps its existing heap-only path: no new
X operation, widget destruction or display close. The X error handler retains its
benign filter and fatal reporter, choosing a live title or stable ScriptName
when late dispatch occurs after base retirement.

## Controls, review corrections and focused results

The new cursor observer links unchanged production objects with preserved old
headers: one cursor created, one attached and zero caller-reference frees, then
the intended assertion aborts. Compilation/link pass; the raw old-source failure
is retained. The initial child control had invalid Icon braces; the second used
32768 as a size, which is valid. Both observer mistakes are retained. At the real
65536 refusal boundary the old module exits 1 with its geometry diagnostic and
no Init; the runner's anticipated signal assertion fails. **No child crash was
reproduced or claimed.** The candidate's installed-X-call observer independently
requires a non-NULL atom name and registration-before-release ordering.

One normal GCC13.3 incremental Linux ARM64 profile and one scoped Clang18.1.3
ASan/UBSan profile supply the composed checkpoint:

- Ten affected unit entries per profile pass. The exact-object base owner checks
  cursor attachment/release, valid black versus refused colors, GC/string/ID
  retirement, named-to-colorset transition, partial/repeated cleanup and active
  retention. Existing widget/property/value/block/binding/message/packet owners
  remain covered.
- Thirteen base lifecycle cases pass per profile: father, child, invalid cursor,
  named/borrowed/changed colors, four partial paths, three active callbacks and
  injected production DeadPipe. Exact watched owners retire at idle; the active
  cases retain 13 watched owners. Widget callbacks precede base GC release.
- Normal property 7, widget 9, message 5, packet 7 groups and block 6 X observations
  pass. The actual block-module probe executes its updated idle-base expectation;
  parser/property probes verify detached transfers and final/active lifetimes.
- Separate review found the colorset-reference path passed zero RGB to an RGB
  allocator, exposing the baseline's uninitialized-RGB defect. The correction
  queries the selected pixel before allocation. Both profiles pass corrected
  unit query/refusal/RGB checks and actual-X comparisons of four distinct colors
  through status and legacy interfaces; borrowed source values remain unchanged.
- The lead found late X-error diagnostics still dereferenced the retired base.
  Six affected exits per profile pass the corrected label observer. It invokes
  the actual installed handler with synthetic BadValue at XCloseDisplay, checks
  live/fallback pointers, and intercepts only that synthetic fatal-report call.
  Real errors still delegate to the original reporter. This is a handler test,
  not a naturally reproduced server failure or proof of real fatal termination.

The first normal build and ten units pass. The new X driver stops after six
passing cases because it expected raw QuitFunc text rather than WriteToFile's
metadata wrapper. Correct only payload extraction and the unavailable isolated
child diagnostic expectation, then run the remaining seven cases. Preserve exit,
no-Init, exact Quit payload, geometry diagnostics and owner/selection assertions.
The private before-controls and this failed candidate observer remain archived.

Retain the nine unaffected unit paths per profile and normal regressions after
the isolated colorset-reference correction; those named-color/ownership inputs
do not execute that branch. Rerun the corrected color unit and real-X color
comparison per profile. Likewise, retain prior lifecycle ownership results after
the diagnostic-only fix and check its six affected exits per profile. Original
and continuation output identities remain separate. No preferred-result repeat,
weakened product expectation or broad matrix follows the corrections. All 22
recorded candidate commands, including the failed observer, total 31.145 seconds;
this is fixture command time, not general development speed or WM performance.

Units enable leak detection and halt-on-error. Actual-X whole-module execution
retains leak detection disabled for unrelated interpreter/font-cache owners;
selected allocation/resource assertions and ASan/UBSan remain active.
No changed-source compiler warning or sanitizer diagnostic is observed; existing untouched-input warnings
and nonfatal X keyboard warnings remain in raw logs.

## Evidence and limits

All 580 final input identities match the owned source, including 13 changed
production/test/build files. The canonical host archive is
`builddir/codex-2026-10-26/m5-next/native-evidence.tgz`: 393 payloads plus
manifest, all hashes/sizes/modes and gzip CRC verified. Old source/objects/binary,
executed observers, every failure/continuation, raw logs/JSON, actual compiler
profiles and output identities remain source-bound. The 393 verified guest
payloads, manifest and duplicate archive are retired; canonical host evidence,
source/build caches, stages, VM disks and performance references remain. The
[machine receipt](VERIFICATION-2026-10-26-M5-SCRIPT-BASE.json) records exact scope.

No parser-wide error/object cleanup, pre-atexit XOpenDisplay failure, global font
cache or resource-budget completion is claimed. No new UI/configuration syntax,
module protocol, dependency or install rule is introduced; no new screenshot or
three-theme matrix is required for these existing ownership/color contracts.
Actual RGB values are checked on Xvfb. Fresh BSD/AMD64, complete compiler/feature/
install/theme and native-CPU/hardware/toolkit/compositor/release qualification
remain pending with their owners. #4/#6/#9/#25/#34 remain open; #31/#35/#36 remain
completed. Daybreak-blocked work stays parked.

The due [PR248–253 comparison](PERFORMANCE-2026-10-26-PR253.md) completes before
candidate upload with preserved binaries and no rebuild. Timing overlaps and
selected memory measures increase; no speed gain or allocation/leak attribution
is established. PR253 is the next reference at 0/5 before this PR merges.

Requested Astra/high analysis and Sol/high implementation/separate review follow
local routing; actual runtime model metadata is unavailable/unverified. Separate
source findings are resolved; final raw-evidence/report review passes with no
remaining actionable findings. Packet evidence is seven groups, corrected from
the initial report count without a test repeat.
