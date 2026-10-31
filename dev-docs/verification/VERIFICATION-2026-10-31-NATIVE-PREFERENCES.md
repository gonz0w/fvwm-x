# Native application preferences — 2026-10-31

Owner: Cam; #60/#73, `codex/r1-native-preferences`, based on merged PR315
`b19c3b04d5d423bdd3ba05ec266c820389304de0`. This completes the selected
[seven-outcome checkpoint](../decisions/NATIVE-PREFERENCES-2026-10-31.md),
with separate final review clear; whole-owner and release acceptance remain open.
The [receipt](VERIFICATION-2026-10-31-NATIVE-PREFERENCES.json) records exact
sources, commands, failures, installed bytes and artifact identities.

Five new schema 9 preferences use the existing typed store and a separate native
editor. Publication is disabled by default. FvwmDesktop owns one private C child,
observes its acknowledgments and actual retirement, and coalesces bounded updates.
The child publishes complete XSETTINGS snapshots and an ownership-preserving
Xcursor root-resource overlay. Empty font/theme values and cursor size zero omit
owned settings. Existing motion supplies the animation preference. No toolkit,
bus, portal, second settings store or core/module packet extension is introduced.

## Executed checks

| Check | Result and boundary |
| --- | --- |
| Linux ARM64 GCC13.3, Go1.27.2, AppleHV native CPU | Affected C/Go/helper/manual builds pass. This is local guest execution, not physical display or whole-release acceptance. |
| Go | Source05: 370 ordinary and 370 race passing records across settings/controller/Desktop; vet passes seven packages. Four unchanged packages retain 59 ordinary source02 passes, giving 429 composed ordinary records. |
| Native policy | New parser/model/codec/resource policy passes normally at source02 and under ASan/UBSan at source04; those C inputs remain unchanged. Changed MFL request policy passes normally and under ASan/UBSan. |
| Raw actual X | Thirteen helper wire/selection/resource groups pass normally at source05 and with ASan/UBSan plus leak detection at source07. No sanitizer reports. |
| Actual controller/GTK3 | Nine groups pass in each of Slate dark, Dune light and enlarged Classic at source07. Existing/fresh GTK3 clients consume all five mapped values; all five notifications and enlarged sample height are observed. Fresh libXcursor lookups pass. |
| Public settings | Legacy actual-X settings integration passes at source06. Eight worst-case rich 26-record replies pass actual encoding and full metadata reconstruction; maximum 8060 bytes under the unchanged 8192-byte cap. |
| Installed lookup | Tagged helper/Desktop/worker install and a Slate controller run pass. The helper override is absent and the exact installed direct C child is observed. Ten groups include the additional installed-lookup assertion. Build-tree core/MFL/Form/capture peers remain; this is not a wholly installed session. |
| Manuals | Three generated pages format with native macOS mandoc, exit0. Lint exits3/4/3 and is not a pass. Diagnostics classify Asciidoctor mso/LINKSTYLE, paragraph/line and MFL table constructs. |

The helper is 170,304 unstripped bytes in this profile; text/data/bss are
34,668/2,312/128 bytes. Direct dynamic dependencies are existing X11, libc and
the loader. Compile-only shared include requirements add no GTK runtime link.
These are artifact sizes, not peak RSS or whole-desktop cost measurements.
The separate [due PR310-to-PR315 core comparison](PERFORMANCE-2026-10-31-NATIVE-CORE.md)
has mixed overlapping command-median ranges, title p95 higher and core peak RSS
+8KiB; it does not measure this helper or demonstrate a speedup.

## Observable contracts

Raw helper cases cover partial frames, malformed/oversized input, stale identity
and sequence, conflicting duplicates, no-op/per-key serials, empty removal,
competing/lost selection, parent EOF during acquisition, external resource edits,
exact cleanup preserving unrelated prefix/tail bytes, modified-block refusal,
reused live XID refusal, confirmed dead-owner adoption and a 65,538-byte resource
limit refusal independent of successful XSETTINGS. Unsigned serial rollover has
pure native policy coverage. Go tests cover coalescing, delayed/partial results,
late runs, bounded process retirement and stop-unknown without launching twice.

Controller runs prove old schema off/inherit loading without rewriting, actual
native child admission/publication, individual 128-byte values, combined oversized
payload refusal, empty group removal, invalid-save/stale-generation/no-op
non-mutation, toolkit effects, editor Cancel, disable/EOF cleanup and retirement,
competing owner/no restart loop/explicit retry, and actual Desktop restart with
saved intent and a fresh run. The public MFL request retains its 384-byte normalized
payload and 1000-byte core command limits. Individually valid large preferences can
exceed the total together; the native editor retains its bounded atomic group
route. MFL rejects a 129-byte string before Go value policy; these error layers are
recorded separately. Saved/effective values and metadata differences remain intact.

Three [unmodified screenshots and configurations](../../doc/development-preview.md#native-application-preferences-in-progress)
show both palettes and enlarged FVWM UI. The lead inspected all three originals:
editor labels, input values, observed status and action buttons are readable and
contained. Classic uses the larger mono font; the editor uses its explicit theme
colorsets. GTK samples independently use the published application font. Screenshots
do not establish assistive-technology, compositor or physical-input acceptance.

## Failures retained and corrected

- Source01 native compile lacked inherited Xft include paths. Existing compile-only
  include dependencies fix the affected helper; runtime linkage remains X11.
- Source02 rich settings response was 10,305 bytes. Source03/04 compaction attempts
  still exceeded the cap in worst cases; source03 also found an old raw-row test
  that needed the documented metadata reconstruction. Two scalar-default maps,
  non-HTML JSON encoding and concise registry descriptions retain complete records;
  quote/backslash-heavy values reach 8060 bytes in the final check.
- The first instrumented orphan test assumed a new X connection could not reuse
  the dead owner's XID. The helper correctly refused the now-live ID. The corrected
  fixture proves both live/reused refusal and dead/nonreused adoption independently;
  production ownership protection is unchanged.
- Source05 legacy integration treated a second owner's asynchronous invalidation
  as an unwatch reply. Its bounded decoder now validates those notifications before
  the matching acknowledgment. Source06 passes.
- Source05 controller exposed the old MFL nonempty 64-byte input restriction. The
  shared validator now accepts empty/128-byte strings with all structural/payload
  limits retained and boundary units. The129-byte refusal expectation now names
  the transport layer. Source06 passes these boundary and GTK cases.
- Source06 assumed automatic editor focus despite shipped click-to-focus policy.
  Source07 explicitly selects the real window through FVWM and verifies actual
  keyboard focus before capture/Cancel; product focus policy is unchanged.
- Supplement01 incorrectly expected an untagged existing manual to be refreshed by
  the helper install tag. Supplement02 stages only the two affected untagged pages
  at their actual Meson-declared paths. All six installed files match build bytes;
  helper/manual and Go executables use the actual tagged install.

Failed aggregates remain failed; valid unchanged component results are reused.
No full build, race suite or matrix is repeated solely to create the PR.

## Source, review and continuation

Final source07 closure is
`d65732843aa1ad841a269bad8341832300313f6bc62a14072cd865291270cc4a`;
main fixture is
`a61bd51d37824f00a0745ed44fc62f51de0c1bf93cec79e55f0e6e73c175f40e`.
Source guards match before/after each checkpoint. Runtime C/Go inputs remain
unchanged after their passing checks; subsequent gallery/status/report changes
are documentation. Eleven canonical host archives retain 344 fully verified raw
files (3,159,811 compressed bytes), including failures. The separate paired
performance archive retains49 verified files. Separate source/artifact/visual review is clear without outstanding findings.
Verified guest duplicates/input archives and the superseded PR310 runnable pair
retired, releasing 56,423,937 bytes. Old full host inputs are reconstructable from
the current archive plus verified deltas; duplicate expanded failures retired
(172,196,480 bytes). Canonical compressed failure records remain. Current source,
builds, useful prefix, SDK/caches and the PR315 runnable reference remain. The
owned Linux container and machine stopped normally; the receipt records cleanup.

Selected Temper implementer/analyst/reviewer roles were available. Backend model
and effort metadata remain unverified; separate agent review is not human approval.

#60/#73 stay open for their remaining frozen registry/preview/provider, optional
portal and toolkit/browser/support-profile criteria. No Qt/browser, cached-client
cursor refresh, six-target/compiler/default-X/full-install, broad native sanitizer,
physical hardware or release acceptance is inferred. #257/#258 remain parked.
The verified runnable merged315 core/MFL pair becomes the next performance
reference; counter 0/5 at315, 1/5 if this runtime candidate merges.
