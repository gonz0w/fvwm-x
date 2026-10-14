# Stage 3 monitor and X protocol reconciliation — 2026-10-12

Owners: MONITOR-01, EWMH-01 and SELECTION-01. This audit has a fixed set of
32 coverage rows: 12 monitor, 12 WM protocol and 8 selection/drag roles. It
reuses accepted DATA/COMPAT/font/modal evidence and introduces an independent
EWMH inventory client. It does not reopen completed geometry work or claim
complete standards conformance. Runtime adoption stays with the tracker owners.

Source boundary: the accepted native C baseline in
[Prompt/protocol qualification](VERIFICATION-2026-10-11-PROMPT-METADATA.md),
SHA256 `397cd39eea71bd7059e03d1948f85ec0299fbed7931569d85c79635bd09ecc32`.
The inspected production/fixture sources below match that archive. The new
[affected checkpoint](VERIFICATION-2026-10-12-COVERAGE-CI.md) records exact source,
compiler profiles and results. A source or test name alone is implementation
inventory, not passing evidence for every clause in that area.

Historical IDs retain the `v0.1/` namespace from the
[requirements crosswalk](../PROJECT.md#monitor-and-hidpi-requirements-migration).
Existing named-profile evidence comes from the accepted Prompt/font/modal/core
reports and their frozen suite logs. Physical display/toolkit/native release CI
claims remain outside local QEMU/container evidence.

## Monitor requirements

| ID / historical scope | Implementation and independent test | Disposition / actual remaining work |
| --- | --- | --- |
| M01 / v0.1/TOP-01 | `libs/FScreen.c`, `monitor_topology.c`; monitor-topology and fscreen-topology-adapter tests | Locally verified publication, equal/changed generations and transactional failures. Preserve these policies. |
| M02 / v0.1/TOP-02 | `monitor_ownership.c`; monitor-ownership test | Locally verified negative origins, clones, reordered identities, rotation extents, gaps, seams and hysteresis. Physical rotation remains unqualified. |
| M03 / v0.1/TOP-03 | `topology_refresh_policy.c`, `events.c`; topology-refresh-policy, topology-event-adapter, live harness topology scenario | Locally verified quiet/hard deadlines, downstream ordering and synthetic topology bursts. Physical event bursts remain unqualified. |
| M04 / v0.1/TOP-04 | `FScreen.c:legacy_reconcile_apply` retains disabled records; harness removes a Monitor object | Partial evidence. Monitor-object removal does not establish migration/visibility/restore of managed windows after output loss. Add output-disable/re-enable cases with windows in normal/maximized/fullscreen and active interaction states before changing recovery. |
| M05 / v0.1/TOP-05 | Status topology and `PrintInfo Scale`; monitor-scale-config test; harness scale-policy/topology scenarios | Inspection is implemented and locally exercised. Synthetic xrandr topology is a test mechanism; no shipped general simulation command is established. Reuse the harness rather than infer a new configuration command. |
| M06 / v0.1/TOP-06 | `live_x_harness.py`, `run_in_linux.py`; failed-query recovery and harness self-tests | Locally verified owned process cleanup, retained failures and last-good topology. Keep real X observations distinct from injected queries. |
| M07 / v0.1/SCALE-01..05 | `monitor_scale.c`, `monitor_scale_config.c`; monitor-scale/config and live scale-policy tests | Locally verified rational quarter buckets, DPI/fallback reasons, exact-name precedence, escaped diagnostics and allocation failure retention. No scale-policy restart is needed. |
| M08 / v0.1/SCALE-06 | `monitor_scale_config_finalize_initial` freezes startup configuration; test_finalize_owns_one_store_and_rejects_late_mutation | Proven implementation gap for selective runtime reload. Current manual explicitly documents refusal of later scale changes. Design transactional recomputation, resource generations and affected-owner adoption together; preserve existing names/arguments and unrelated resources. |
| M09 / v0.1/UI-01..06 | `Flocale.c`, picture/cache/UI metrics owners; accepted IMAGE/DRAW/font/modal suites | Existing local scale/resource evidence retained. Exact client pixels are not scaled by the WM. Remaining bundled consumer audit belongs to MODULE-SCALE-01, not a new toolkit/cache. |
| M10 / v0.1/GEO-01..07 | geometry-transition, ICCCM size hints, monitor ownership policies; geometry_transition_live.py | Locally verified logical restore, increment/fixed-size constraints, pointer anchors, work rectangles and command transitions. Output-loss recovery remains M04; external toolkit/hardware observations remain M12. |
| M11 / v0.1/MOD-01..03 | additive native monitor packets/metadata; module-monitor and pager-monitor tests | Producer/metadata foundations verified locally. Audit each actual module UI consumer before extending scale handling; packet dimensions stay actual pixels. |
| M12 / v0.1/TEST-01..05 | six-target pure/native-X/default-BSD-X reports | Local evidence retained. Physical GPU/hotplug, actual GTK/Qt/browser feedback, external compositors and physical input remain required separate acceptance. Native Linux release sanitizer CI is also open. |

## WM protocols

References: [EWMH 1.5](https://specifications.freedesktop.org/wm/1.5/),
[ICCCM 2.0](https://xorg.freedesktop.org/archive/current/doc/xorg-docs/icccm/icccm.html).
EWMH hints are adopted individually: absent optional protocols are not automatic
required defects. Required clauses of an advertised feature still need evidence.

| ID / clause | Current implementation / independent evidence | Disposition |
| --- | --- | --- |
| E01 / EWMH 3.1 | `ewmh.c:set_net_supported`; ewmh_inventory_live.py | New client verifies ATOM/32, no duplicates and the exact 63 declared names. This verifies publication identity, not semantics for all 63 hints. |
| E02 / EWMH 3.10 | `ewmh.c`; new independent root/child check and UTF8 name | New client verifies WINDOW/32, self-referencing child and nonempty name. Identity remains FVWM3; release branding belongs to RELEASE-01. |
| E03 / EWMH 3.2 | new independent map/raise/destroy clients | New client verifies mapping order, bottom-to-top stacking after Raise and removal of dead IDs with continued commands. Broader transient ordering uses existing stack tests. |
| E04 / EWMH 3.3, 3.6 | new desktop property type/count/range observations | Basic initial count/current state verified. Dynamic expansion/shrink/high IDs and publication consistency still need explicit independent cases. |
| E05 / EWMH 3.9 | `ewmh_SetWorkArea` uses configured NumberOfDesktops; initial effective desktop count is separate | Reproduced required-clause gap: one desktop is advertised with an empty work-area property in the minimal startup configuration. Inventory records this finding explicitly. Repair publication count/geometry with dynamic desktops, struts and topology owners; do not use this audit pass as conformance evidence. |
| E06 / EWMH 5.17 | new CARDINAL/32 extents observations and actual X frame/client sizes | New client verifies four extents and horizontal/vertical pixel sums. Per-edge root coordinates, decorated states and frame-extents requests need separate cases. |
| E07 / EWMH property validation | hardening_live.py and legacy_property_live.py; FProperty/property-policy helpers | Existing malformed lengths/types and WM_PROTOCOLS checks retained. They do not cover every ClientMessage format/source/timestamp. Add scoped foreign-client request cases before each runtime change. |
| E08 / EWMH 3.8, 5.15, 5.16 | ActiveWindow exists; USER_TIME and USER_TIME_WINDOW absent from tables and observed advertisement | Focus/user-time adoption and stale timestamp/source decisions remain unqualified. Existing command Focus checks do not prove EWMH focus policy. |
| E09 / EWMH 5.9, 5.10 | legacy STRUT read/validation exists; STRUT_PARTIAL absent | Legacy short/type refusal verified. Partial struts are a missing feature; add policy and dock mutation/removal/topology fixtures before adoption. Keep root coordinate and precedence rules explicit. |
| E10 / EWMH 6.3 | fullscreen state/command geometry exists; FULLSCREEN_MONITORS absent | Command fullscreen/restore evidence retained. It does not establish multi-monitor ClientMessage selection. Optional protocol adoption remains queued with MONITOR/geometry. |
| E11 / EWMH 6.1, 6.2 | PING and SYNC_REQUEST absent from tables and observed advertisement | Optional protocols are unimplemented; no support claim. Negotiate and test their deadlines/lifetimes if adopted. Do not add them to _NET_SUPPORTED first. |
| E12 / ICCCM 4.1.2..4.1.7 | core focus/transients/WM_PROTOCOLS; legacy-property and stack/client-identity fixtures | Existing deletion, malformed protocols and transient lifetime evidence retained. Enumerate TAKE_FOCUS timestamp/focus-model and WM_STATE transitions independently; atom presence is insufficient. |

## Selection and native drag roles

[ICCCM selection conventions](https://xorg.freedesktop.org/archive/current/doc/xorg-docs/icccm/icccm.html)
require selection-owner TARGETS/MULTIPLE/TIMESTAMP support. Those requirements
apply to the actual owning component; they are not waived because bounded legacy
transfer tests pass. Existing INCR refusal is failure coverage, not INCR support.

| ID / role | Implementation / existing independent evidence | Disposition |
| --- | --- | --- |
| S01 / core manager selection WM_Sn | `fvwm/icccm2.c`; legacy_property_live.py sends valid/invalid MULTIPLE pairs and reads TARGETS | Required target machinery exists and malformed-pair handling is locally verified. Add direct TIMESTAMP/VERSION, MANAGER announcement, replacement and owner-loss checks before full manager-role qualification. |
| S02 / Script PRIMARY owner | TextField selection acquisition and FvwmScript.c SelectionRequest/Clear | STRING ownership exists; TARGETS/TIMESTAMP/MULTIPLE handlers are absent in this role. Required interoperability gap; preserve raw stored theme bytes while adding target negotiation. Verify ownership timestamps and property=None handling. |
| S03 / Script PRIMARY/CLIPBOARD requestor | `FSelectionRequestTextWithService`, TextField paste; module-selection and text-list real-X fixtures | Locally verified bounded ordinary text, UTF8_STRING-to-STRING fallback paths, malformed types/lengths, silent owners and recovery. Primary full target negotiation and sustained/flood owner-change tests remain incomplete. |
| S04 / Script private message owner/requestor | SendMsgToScript/ReceivFromScript; module-selection and legacy-property fixtures | Locally verified ordinary private messages, empty/invalid targets, missing property fallback, late replies and lifetime bounds. This private transport is not clipboard-manager or native XDND support. Required generic-owner target coverage needs a deliberate role review. |
| S05 / Form legacy cut-buffer consumer | `FvwmForm/ReadXServer.c`; selection_live.py | Locally verified typed/limited cut-buffer paste, recovery and UTF8 boundary behavior. Disabled selection event handlers are not PRIMARY/CLIPBOARD ownership. No owner-target requirement is inferred for this consumer. |
| S06 / INCR transfer and cancellation | FSelection fixed bounded property reader; test_selection_x.c covers INCR refusal, silent/dead owners, stale requestors and high descriptors | Bounded refusal verified. INCR receive/send is unimplemented, so large conforming incremental selections do not transfer. Design operation-local chunk/deadline/owner-loss state with DATA/INPUT; preserve cancellation and main-loop servicing. |
| S07 / CLIPBOARD_MANAGER/SAVE_TARGETS | no manager implementation in the adopted core/modules | Planned under CLIPBOARD-01/CONTENT-01 stage 6. This audit adds no daemon or persistence. Manager handoff needs foreign-client evidence when implemented. |
| S08 / native XDND participants | no native XdndEnter/Position/Status/Drop/Finished handlers found in core/modules | Roles remain planned with FILE-VIEW/launcher UI owners. Generic managed clients retain their own toolkit protocols. No WM-wide drop interception or dropped-file execution is implied. |

## Resulting order

Retain completed arithmetic, cache, logical geometry and bounded legacy transfer
work. CI-01 now has the fixture acknowledgement and shared native setup with
candidate action pins; its transitive assets/native jobs remain open.

At the tracker-authorized runtime stage, prioritize E05 desktop/work-area
publication, S01 manager transition deadlines/evidence, M04 output-loss recovery
and M08 transactional scale reload. The existing replacement path waits for the
previous WM to destroy its ownership window without a deadline; preserve that
source observation for the manager-transition checkpoint, not a claimed test.
Then close S02 required owner targets and S06 incremental transfer before richer
clipboard consumers. Optional E08–E11 protocol adoption and S08 native UI targets
follow those ownership/geometry contracts. These gaps require no configuration
migration by their existence; choose a concrete implementation before assessing
any new command or dependency requirement.
