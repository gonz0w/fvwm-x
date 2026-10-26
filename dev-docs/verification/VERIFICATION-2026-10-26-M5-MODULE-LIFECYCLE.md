# M5 Pager, Animate and Event lifecycle owners

Status: composed focused gate and separate source/test/raw/report review pass. Owner: CLEAN-01 #34. Base is merged PR255,
`46d45b52a1415f2e277c66cced1f5bda613c7a37`; canonical signature/DCO and
reviewed tree `ac47a7b4325edc6b9542bc7945235226a1e7eaa9` match.
PR255 is Done and its branch is retired. This is four related ownership stories,
not completion of the broad audit.

## Four local ownership changes

Pager's new desk styles acquire separate references for inherited background/
highlight picture slots using existing PCloneFvwmPicture. Equal cached pointers
still represent separate acquisitions. Existing styles keep their snapshot when
defaults later change. Terminal cleanup clears those picture slots, eight GCs per
initialized style, NormalGC/MiniIconGC/Balloon.gc and the drawing scratch container.
It covers normal EOF/signal returns, reader setup refusal and ExitPager; repeated
selected cleanup is harmless. Unrelated font, heap, window, monitor and ungrab
teardown stays in its original paths. Scratch members remain borrowed; only the
container is freed.

Animate retires its GC before the copied tile as one pair before replacement.
Width changes and the post-config rebuild no longer abandon the previous tile.
Color/Pixmap callers use the same retirement; final GC/tile/configuration strings/
module prefix retire after Loop. Unlock and StopCmd remain before final retirement.
ModuleArgs and its name stay borrowed. The existing explicit DeadPipe entry and
display-open refusal also use bounded cleanup; no production atexit or X-I/O
recovery framework is introduced.

Event's three static action tables retain literal names while owning action
values. Those values, command prefix and module name retire after shutdown action
dispatch, with early argument/descriptor refusal cleanup. Command order, masks,
delays and configuration syntax remain unchanged. No shared teardown framework,
dependency, layout change or configuration migration is introduced.

## Controls, correction and composed gate

Before candidate upload, all 584 accepted input hashes/modes match the Linux
source. Preserved old module objects/headers link with the accepted shared library;
these are selected old-owner controls, not fresh full baseline qualification.
Event retains seven watched owners after shutdown. Corrected Pager controls
observe eight acquired references, six excess releases, no explicit release of
43 selected GCs and no scratch-container release. The old observer defers picture
destruction and drains only acquired references to measure wrong counts safely;
this is not a naturally reproduced crash. Animate tiled controls allocate six
tiles but release two, release eight of nine GCs and retain two selected strings.
Missing-image startup releases one of two GCs and retains the same two strings.

The first candidate normal build and Event unit pass. Pager's first aliases case
then fails because the observer counted NULL Balloon.gc: specifying its font did
not enable balloons. A diagnostic-only probe finds 42 frees, no live tracked GC,
balanced 14 picture references and one scratch release. Add documented
`Balloons All` to the fixture and require three non-NULL global GCs, preserving
the intended 43 actual owners. Production code is unchanged. Four affected old
Pager controls run with the corrected fixture; passed old Event/Animate controls
and candidate build/Event unit are retained. Original source, failed output,
diagnostics and corrected continuation remain separately archived.

One reused GCC13.3 debugoptimized Linux ARM64 profile and one scoped Clang18.1.3
ASan/UBSan profile establish the combined result:

- Seven actual-X ownership cases pass per profile. Four Pager cases cover cache
  aliases, distinct pictures, EOF, SIGTERM, explicit ExitPager and injected reader
  setup refusal. Each balances 14 references including six inherited acquisitions,
  frees all 43 selected GCs and frees scratch once without freeing borrowed text.
  Returning paths then invoke ExitPager to check repeated selected cleanup; this
  does not reroute production's full teardown.
- Three Animate cases cover startup/Width rebuilds, pixmap/color/none changes,
  missing image, Stop/EOF/signal and a repeated terminal call. Tiled cases free
  all six tiles/nine GCs; missing startup frees both GCs and allocates no tile.
  Selected strings reach zero. One synchronized packet's unlock precedes Stop
  menu dispatch while resources remain live, then final retirement follows.
- The Event owner unit passes four scripted actual-main cycles per profile:
  repeated complete configurations, descriptor refusal and partial configuration.
  Selected retained owners reach zero. Production configuration/dispatch runs;
  transport interfaces are replaced for owner isolation. Existing real-pipe
  Event regression separately passes ten cases, including configuration/active
  termination and EOF shutdown.
- Four existing normal Pager X regression groups pass: startup/bounds (six),
  layout (six), transport (four) and queued-X service (one). Together with Event
  this is five regression groups, not new full platform/release qualification.

The Event sanitizer unit enables leak detection and halt-on-error. Sanitized
whole-module X runs disable unrelated global-cache leak detection while retaining
ASan/UBSan halt-on-error and exact selected assertions. No whole-module leak-free
claim follows. Corrected logs contain no sanitizer/runtime/assertion failure;
expected incomplete/unknown Event and missing-image diagnostics remain.
The 15 candidate/diagnostic/continuation commands total 6.953 seconds of fixture
command time, excluding baseline/dispatch/review. This is not WM speed or total
development time.

## Evidence and limits

All 592 recorded final runtime/test/build input hashes/modes match the owned
source. These include nine changed/new files; the 584-to-592 union also adds
existing regression runners and Animate's build file to the recorded scope.
Canonical host evidence is
`builddir/codex-2026-10-26/m5-follow-on/native-evidence.tgz`: 328 payloads plus
manifest. All payload hashes/sizes/modes and gzip CRC verify. Old source/objects/
binaries, initial/corrected observers, raw results, profiles, commands and final
output identities remain preserved. Verified guest payloads/manifest/duplicate
archive are retired (16,386,386 bytes); source/build caches, stages, VM disks,
canonical host evidence and performance references remain.
[Machine receipt](VERIFICATION-2026-10-26-M5-MODULE-LIFECYCLE.json).

No fresh BSD/AMD64, full compiler/feature/install/theme, physical hardware,
compositor/toolkit or release qualification is claimed. Rendering is preserved;
no new UI requires screenshots or a new-primitives theme matrix. No dead-X-server
recovery or all-exit-path/module-cache qualification is claimed. #6/#9/#25/#34
remain open; #31/#35/#36 remain completed. Parked Backer/pathname work and deferred
Form font cleanup are excluded. PR253 reference is 2/5 after PR255, 3/5 only after
this batch merges; no comparison is due. Requested Astra/high analysis and
Sol/high implementation/separate review follow project routing; actual runtime
model metadata is unavailable/unverified.
