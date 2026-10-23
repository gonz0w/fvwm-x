# Native scrolling collection list

Status: implemented; focused Linux/FreeBSD and scoped sanitizer checks pass. Owners: FILE-VIEW-01, with
FILE-ACTION-01's consumer and shared clipboard metadata prerequisites.

The next checkpoint after merged PR167 replaces the example browser's paged
choices with a reusable FvwmForm List. The coherent batch covers native viewport
rendering, keyboard/mouse focus and selection, bounded whole-view transport, and
browser adoption including acknowledged configuration uploads. No existing
configuration directive changes; List/ListItem/ListFocus are additive. Internal
browser callback version 2 replaces version 1 with shipped consumer/probe updates.

One X child holds at most 4096 labels; drawing visits at most 32 configured rows
(twelve in the browser). ButtonFont and item colors use existing native resources.
Pure C owns scroll/focus/selection state; Tab traversal extends to lists only in
forms containing them. Legacy forms retain their input/history ring. Restart
restores initial state. Labels/arrays/names belong to module lifetime and are
released at exit. List-only bitmaps use separate allocations so legacy item
records do not grow with collection capacity. An invalid/over-limit item refuses the encoded list result.

The native packet is two big-endian focus bytes and a little-bit-first row
selection bitmap, URL-base64 encoded without padding: at most 514 raw / 686
encoded bytes. No filenames, stable IDs or clipboard contents are returned by
this control. The consumer validates exact length, tail bits, focus, generation
and the aggregate 256-selection cap before publishing state. It resolves rows
through its immutable ordered filtered projection; filtered-out selections
survive. Existing collection paging remains available to other model consumers.
The browser removes its page controls/state, retains stable focus on refresh,
and offers focused Enter independently of selected F3 activation.

The 4096-row actual-X case reproduced MFL's finite output-queue refusal when
configuration was uploaded without backpressure. The browser now sends at most
32 commands before awaiting a matching instance/form/batch core Echo. One-slot
acknowledgement storage and a ten-second acknowledgement deadline bound this
phase. The authoritative core and MFL queue limits are unchanged. Configuration
is complete before the final Module command; scrolling/selection thereafter
remain local until an action needs the model. UI counters describe the latest
submitted state, not each local selection click. Filtering/sorting/refresh still
replace the form; incremental provider updates are future owner work.

This is metadata UI only. Protected clipboard ownership, private content
delivery, icon-grid mode, provider APIs, file mutations, accessibility/hardware
acceptance and broad FILE-VIEW qualification remain open. SQLite remains deferred
to STORE-01's protected memory and optional encrypted persistence contracts.

Use focused native Linux C/Go policy, legacy Form integration, scoped sanitizers
and actual-X screenshots, with early FreeBSD compiler/native-X checks. Shared
six-target coverage follows the existing cycle; native-CPU sanitizers and full
milestone/release acceptance remain separate. Evidence records exact inputs and
initial failures rather than promoting this checkpoint to release qualification.
