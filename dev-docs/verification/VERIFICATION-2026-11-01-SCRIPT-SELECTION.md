# Script selection shared checkpoint — 2026-11-01

The combined Linux checkpoint exercises merged PR323–325 together. Normal
Script ownership, metadata, direct/INCR paste, exact messages, servicing and
shutdown checks pass after two explained corrections. The actual 32 KiB paste
exposed quadratic TextField viewport measurement; a local character-boundary
search fixes that stall without lowering the payload cap or extending the
transfer deadline. ASCII and multibyte 32 KiB titles now return intact and the
module services the following command within the existing three-second fixture
observation window. This is a responsiveness regression result, not a timing
benchmark or whole-issue completion.

[Machine receipt and retained failure logs](VERIFICATION-2026-11-01-SCRIPT-SELECTION.json)
identify the immutable source, plans, commands, nested fixture results, archive
hashes and exact scope. Work used the existing Linux ARM64 Podman/AppleHV lab,
native ARM instructions and owned Xvfb. It establishes neither physical hardware
nor a six-platform/release result. No new VM, installation sweep or Go race run
was needed for Script; the separate OpenBSD attempt is recorded below.

## Results and boundaries

| Check | Observed result |
| --- | --- |
| Four normal policies/linked service probes | `script-message`, `script-selection`, `script-selection-wait`, `script-base-owner` pass in linux01. The linked adapter checks Init servicing, event-record bounds, recursive-reader refusal, mutation and fast pipe-loss fencing. |
| Native owner API | Four boundary groups pass: real stale/future acquisition refusal preserving the snapshot, exact 32 KiB owner output and cap+1 refusal, vanished-recipient recovery for the later queued message, foreign owner preservation on loss/teardown. The normal log is recovered from the first fixture; the sanitized API also passes. |
| Independent actual Script owner/consumer | linux04 normal fixture passes immutable snapshot/metadata/MULTIPLE, visible loss, replacement/stale clears, exact private direct/INCR/NUL validation/recovery, nested read refusal, pending-read periodic service and widget mutation, negotiated PRIMARY/CLIPBOARD, ASCII/multibyte wire caps, malformed metadata, FIFO/aggregate budget and shutdown. |
| Existing message count/grace controls | Composed once in the final normal fixture: 40 admitted, overflow refusal, silent grace 2.017 s, invalid requests 2.012 s, drain 0.066 s and cancellation 0.002 s. These measurements include scheduling and are not a stricter two-second wall-clock claim. |
| Transport and existing actual modules | linux03 passes the independent typed transport probe, 12 legacy helper cases and 12 actual Script/Form selection-consumer groups. Those existing consumers are separate from the new extended owner fixture. |
| Modal/base lifetime | Both existing native-X Meson entries pass once in linux03. |
| Scoped ASan/UBSan | Four policies and independent raw transport pass in linux03. The final sanitized owner run reaches its shutdown check, then fails the required exit-zero assertion with LeakSanitizer's 320 bytes / three allocations through Fontconfig initialization. Earlier recorded owner/consumer assertions and the native API pass; **the owner sanitizer suite is failed, not passed**. Its later shutdown assertions and composed grace controls did not complete. No suppression, disabled leak detection or clean-exit claim is added. |
| Representative UI | Final Slate/Dune/Classic component captures pass, with corrected quoted Xft font recipes and enlarged Classic text. The lead visually inspected all three raw PNGs. These are standalone component palettes/fonts, not full desktop-theme qualification. |
| OpenBSD Q67 D1/D2 | Fresh owned OpenBSD 8.0 ARM VM fails SSH source-copy with a contemporaneous virtual network reset. Preparation and both checks never run; zero new functional groups. Prior 53/55 remains unchanged. QEMU is terminated by the owning runner; exit status zero does not establish normal guest poweroff. |

## Corrections and retained failures

linux01 passes the four policies, then the fixture's delayed `XSelectInput`
subscription races an independent module-pipe mutation. Xlib exits with
BadWindow before Python cleanup. The retained temporary logs prove earlier
Init/private/nested/periodic controls reached their assertions. The lead exports
those logs and terminates only the positively identified orphan Xvfb using a
PID-bound handle. The corrected subscription synchronizes before the mutation,
rejects every X error and retains Python failure evidence.

linux02 confirms that correction, then times out waiting for command handling
after the large INCR paste. Source analysis finds repeated whole-suffix width
measurement as the cursor advances 32,768 characters. The viewport correction
searches character indices and measures actual suffixes with the existing font
API. It does not assume byte offsets are characters or change text ownership.
For ordinary nonnegative font advances it selects the first fitting boundary;
unusual advances can choose a different fitting viewport, with the cursor still
bounded. Exact legacy pixels are outside the configuration compatibility contract.

linux03 completes the independently unrun checks without repeating the passing
policies. Visual review discovers that brace-quoted Font recipes reached the
fallback font; those captures qualify palettes only. linux04 recompiles only
TextField and its archive/module links in the two existing profiles, then runs
the affected owner fixture and two corrected short UI captures. Normal checks
pass; the Fontconfig allocation report remains a real failed sanitizer exit.
No unrelated font subsystem repair is inferred from that observation.

## Continuation

SELECTION-01 #22 remains open: retain core manager ownership/race/property-write
rows, clipboard-manager/SAVE_TARGETS scope, supported-platform qualification and
the sanitizer clean-exit follow-up. FILE-INTEGRATION-01 #67 remains 3/4 with its
two disabled-profile groups pending. The OpenBSD transport failure does not prove
or disprove the separately reviewed Meson empty-directory setup correction.
The due [core performance observation](PERFORMANCE-2026-11-01-SCRIPT-CHECKPOINT.md)
is complete and makes no Script workload claim.

The final source/reference snapshot matches the exercised files. The exact
generated Script manual renders with host mandoc exit zero. All owned X/Script
processes are absent at export; the original owner stops its Linux container
and machine with both commands exiting zero. The stopped failed OpenBSD disk
and duplicate source archive are retired (about 3.0 GiB logical file bytes);
logs, receipts and reusable base/SDK/build caches remain. No normal OpenBSD
poweroff or host physical-space recovery measurement is inferred.
