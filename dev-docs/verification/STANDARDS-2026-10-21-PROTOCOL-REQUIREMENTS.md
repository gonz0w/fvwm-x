# Protocol requirement register — 2026-10-21

Owner: STANDARDS-01 #16, with EWMH-01 #18, SELECTION-01 #22 and the existing
DATA/INPUT/MONITOR/COMPAT owners. Status: **fixed inventory complete; runtime
acceptance remains scoped or open as recorded below**.

Three audit stories close here: root capabilities/desktop publication, window
hints/requests, and selection roles. The fixed set is **40 requirements**:
R01–R12, W01–W12 and S01–S16. They refine the existing E01–E12 and S01–S08
areas in the [32-row reconciliation](RECONCILIATION-2026-10-12-STAGE3.md).
The older IDs in the Parent column refer to that reconciliation, not this
register. Its twelve M rows remain with MONITOR-01. This is a bounded inventory,
not completion of all standards areas or authorization for runtime adoption.

## Revisions, profiles and evidence rules

References reviewed for this batch:

- [EWMH 1.5](https://specifications.freedesktop.org/wm/1.5/), publication
  2011-11-29, revision noted as 2013-01-25: [root properties](https://specifications.freedesktop.org/wm/1.5/ar01s03.html),
  [other root messages](https://specifications.freedesktop.org/wm/1.5/ar01s04.html),
  [window properties](https://specifications.freedesktop.org/wm/1.5/ar01s05.html)
  and [WM protocols](https://specifications.freedesktop.org/wm/1.5/ar01s06.html).
- [ICCCM 2.0](https://xorg.freedesktop.org/archive/current/doc/xorg-docs/icccm/icccm.html),
  X11R7.7 edition, chapters 2–4. Section numbers below use the conventional
  ICCCM numbering; the HTML uses named headings.
- Clipboard-manager and native XDND adoption remain **planned**. Their protocol
  revisions must be selected at their implementation checkpoint; this batch
  does not silently adopt a living specification for either service.

Normative levels are kept separate: **MUST**, **SHOULD**, **MAY/conditional**,
and **project** (our bounded-work/validation contract). An absent optional
protocol is not an implemented feature. A required clause of an advertised
feature remains required even when the old inventory reports a finding and
exits successfully.

Status meanings follow [STANDARDS.md](../STANDARDS.md). **Verified for named
profiles** below means only the precise historical observation in the named
report and frozen candidate. It does not qualify every clause in its section,
transfer results to this source, or imply a fresh run on OpenBSD 8.0. Other
rows are **implemented / unqualified**, **review pending**, **planned**, or
**not applicable with rationale**. Supporting evidence in a row never changes
its stated acceptance limits.

Evidence keys:

| Key | Source-bound evidence and profile boundary |
| --- | --- |
| I | [Inventory checkpoint](VERIFICATION-2026-10-12-COVERAGE-CI.md), final archive `ebd123efac3bf5096552827af28bab2fbc0a66897a36a594cc1070712da09003`: ten EWMH inventories in Linux ARM64 GCC/Clang/software/minimal, Linux AMD64 GCC/Clang, FreeBSD/OpenBSD AMD64/ARM64 Xvfb profiles. OpenBSD here is historical 7.9. Each result is an observation of that checkpoint, including the recorded work-area failure. |
| A | [Atom checkpoint](VERIFICATION-2026-10-19-EWMH-ATOMS.md), runtime `844c5c7ee0c37e7cfc001beca17171f917d1acf0`, archive `a4ccc108d493e47066e07a573000c3d8df4c33117446fb8c0cb48852fcc80697`: ten compiler/feature profiles, actual-core and independent inventory checks. Synthetic atom failure modes are distinct from ordinary publication. |
| P | [Publication checkpoint](VERIFICATION-2026-10-19-EWMH-PUBLICATION.md) and its [machine receipt](VERIFICATION-2026-10-19-EWMH-PUBLICATION-COMBINED.json): actual-core work-area/strut/action checks, with explicitly retained conformance gaps. Internal differential agreement is not an independent specification oracle. |
| L | [Earlier role reconciliation](RECONCILIATION-2026-10-12-STAGE3.md), selection section, and its linked legacy transfer reports. These retain named historical profiles; they are not a new full ICCCM acceptance result. |
| O | [OpenBSD 8.0 checkpoint](VERIFICATION-2026-10-20-OPENBSD-80.md), archive `ceccf853531842a19024efc2ce2307be6f62c79304290062becf59f50e9dbb36`: both VM architectures pass selected standard/install/default-X entries. No new manager-handoff, selection-target or desktop conformance assertion was added there. |

All statuses are limited to their cited profile/configuration and tested
observable. QEMU/container execution is distinct from native-CPU, physical
hardware, toolkits and compositors. The initial inventory made no runtime-test,
VM-boot, binary, install or performance claims. Later runtime qualification is linked in its owning rows.
The initial inventory's audited source/fixture hashes and checks are in the
[receipt](STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.json). They identify inspected
files, not a retrospective proof of identical historical binaries.

## Root capabilities and desktop publication

Owner: EWMH-01, with STATE-01/DATA-01 for publication and MONITOR-01 for geometry.
Entry points: [ewmh.c](../../fvwm/ewmh.c),
[ewmh_events.c](../../fvwm/ewmh_events.c),
[ewmh_conf.c](../../fvwm/ewmh_conf.c),
[ewmh_names.c](../../fvwm/ewmh_names.c).
Independent client: [ewmh_inventory_live.py](../../tests/live_x/ewmh_inventory_live.py).

| ID | Parent | Clause / observable | Status and evidence | Remaining acceptance |
| --- | --- | --- | --- | --- |
| R01 | E01 | EWMH 3.1 MUST: `_NET_SUPPORTED` ATOM/32 lists supported hints. | Verified for named profiles, I/A: exact 63 declared atoms, type/format, unique ordinary IDs. | Retain exact-profile scope; table equality alone does not settle R02. |
| R02 | E01 | EWMH 3.1 MUST: advertised hints correspond to implemented behavior. | Review pending. A checks lookup/publication, not semantics of all 63 names. | Check each advertised family against independent requests; preserve alias and failure-mode distinctions. |
| R03 | E02 | EWMH 3.10 MUST: root check identifies a WM-created child. | Implemented / unqualified. I checks WINDOW/32, one non-root ID, but does not independently query parentage. | QueryTree must establish that the check window is a child of this root; test stale/foreign IDs. |
| R04 | E02 | EWMH 3.10 MUST: child check self-references and has WM name. | Verified for named profiles, I: self-reference and nonempty UTF8_STRING name. | Replacement/stale-child handling is separate; branding remains RELEASE-01. |
| R05 | E03 | EWMH 3.2 SHOULD: client list preserves initial mapping order. | Verified for named profiles, I: three mapped clients, then destroyed-client removal. | Broader startup adoption, withdrawn and transient cases remain unqualified. |
| R06 | E03 | EWMH 3.2 SHOULD: stacking list orders bottom to top. | Verified for named profiles, I: Raise changes order and dead IDs disappear. | Independently check multiple layers, transient groups and restacking requests. |
| R07 | E04 | EWMH 3.3 SHOULD/conditional MUST: desktop count and accepted changes agree. | Verified for desktop-publication checkpoint profiles: independent growth, empty shrink and explicit format/range/capacity refusal. [Report](VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md). | Retain snapshot/source scope; oversized configured publication and multi-monitor root authority remain explicit limits. |
| R08 | E04 | EWMH 3.3 conditional MUST: shrink reconciles out-of-range desktops/windows. | Verified refusal policy for tested profiles: current/populated-desktop shrink is refused; accepted shrink requires no relocation. [Report](VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md). | Wider configuration/monitor transitions and alternate relocation policies remain unqualified. |
| R09 | E04 | EWMH 3.5: viewport pairs describe each desktop. | Verified for tested profiles: effective-count array lengths at startup, 301/401 desks and actual-pixel viewport updates. [Report](VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md). | Wider desktop-specific/per-monitor geometry remains open; publication capacity is explicit. |
| R10 | E05 | EWMH 3.9 MUST: work area contains a geometry per desktop. | Verified length/typed positive-geometry contract for tested profiles: startup empty-array gap repaired and high counts agree. [Report](VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md). | R11 reserved-edge semantics remain open; matching lengths do not qualify dock geometry. |
| R11 | E05 | EWMH 3.9 MUST/SHOULD: viewport-contained work area accounts for reserved edges. | Implemented / unqualified. `ewmh_SetWorkArea` publishes global monitor geometry; P tests internal struts separately. | Foreign dock changes/removal, page shifts and topology must drive observable root geometry. |
| R12 | E04 | EWMH 3.6 MUST: current desktop stays within advertised range. | Verified for tested profiles: root pager admission, timestamp zero, actual new-client desk, high-desk growth and refusal/recovery. [Report](VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md). | Oversized configured states retain the preceding root snapshot; wider monitor/shrink/configuration cases remain open. |

The original R09 source observation and reproduced R10 startup gap now have
[desktop checkpoint evidence](VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md).
The old inventory receipt remains historical. The 1024-record/server-request
publication budget bounds storage without restricting configured FVWM desktop
numbers; refused publication retains the preceding root snapshot. R11 remains
separate: equal array lengths and positive geometry do not establish reserved
edges, per-desktop dock semantics or multi-monitor root authority.

## Window hints and request handling

Owners: EWMH-01 / DATA-01, INPUT-01 for focus, MONITOR-01 for fullscreen, DRAW-01
for resize synchronization. Entry points: [ewmh.c](../../fvwm/ewmh.c),
[ewmh_events.c](../../fvwm/ewmh_events.c),
[ewmh_intern.h](../../fvwm/ewmh_intern.h),
[focus.c](../../fvwm/focus.c), [misc.c](../../fvwm/misc.c).

| ID | Parent | Clause / observable | Status and evidence | Remaining acceptance |
| --- | --- | --- | --- | --- |
| W01 | E06 | EWMH 5.17 MUST: CARDINAL[4]/32 reports each frame border. | Implemented / unqualified. I verifies four entries and horizontal/vertical size sums, not each edge. | Translate client/frame root coordinates; distinguish left/right and top/bottom, including changed decorations. |
| W02 | E06 | EWMH 4.3 conditional: frame-extents request produces an estimate. | Planned. `_NET_REQUEST_FRAME_EXTENTS` absent in inspected tables and I output. | Select adoption with EWMH-01; unmapped clients, absent hints, refusal and lifetime need independent cases. |
| W03 | E07 | Project validation: malformed properties/messages are refused safely. | Implemented / unqualified for the whole family. L retains bounded property cases; not every message field. | Test format, signed/oversized values, source IDs and timestamps at each changed request owner. |
| W04 | E08 | EWMH 3.8: active-window publication and activation requests. | Implemented / unqualified. `ewmh_ActiveWindow` exists; command Focus is not a request oracle. | Observe root focus plus property; distinguish request source/time, stale/dead clients and refusal. |
| W05 | E08 | EWMH 5.15/5.16: user-time hints and alternate time window. | Planned. Neither atom appears in inspected tables or I advertisement. | Select focus policy before adoption; zero time, child death, wraparound and real application behavior. |
| W06 | E09 | EWMH 5.9: legacy strut uses four root-edge distances. | Implemented / unqualified for complete geometry. L malformed-property and P internal policy evidence remain narrow. | Independent dock mutation/destruction and maximized placement with root-property readback. |
| W07 | E09 | EWMH 5.10 conditional MUST: partial strut overrides legacy strut. | Planned. `_NET_WM_STRUT_PARTIAL` absent from tables/I advertisement. | Root coordinates, partial edge spans, both-property precedence, hotplug and malformed spans. |
| W08 | E10 | EWMH 6.3 conditional: fullscreen monitor selection uses four indices. | Planned. `_NET_WM_FULLSCREEN_MONITORS` absent from tables/I advertisement. | Validate indices/topology generations and restore; command fullscreen does not establish this protocol. |
| W09 | E11 | EWMH 6.1 optional: ping correlates a client reply. | Planned. `_NET_WM_PING` absent from tables/I advertisement. | If adopted, timestamp/window matching, unresponsive peers and bounded completion; no unsupported claim. |
| W10 | E11 | EWMH 6.2 optional: resize synchronization uses client counters. | Planned. `_NET_WM_SYNC_REQUEST` absent from tables/I advertisement. | If adopted, negotiate counter ownership, ordering, timeout, counter replacement and client death. |
| W11 | E12 | ICCCM 4.1.7: focus models and TAKE_FOCUS timestamps. | Implemented / unqualified. Existing property validation does not qualify focus-model transitions. | Four focus models with independent clients; actual server timestamps and client disappearance. |
| W12 | E12 | ICCCM 4.1.3.1/4.1.4: WM_STATE follows window lifecycle. | Implemented / unqualified. Legacy adoption and icon-state regressions cover narrower observations. | Normal/Iconic/Withdrawn transitions, icon XIDs, remap/restart and destruction with independent property reads. |

The role and source are important: `_NET_WM_PID` or an opacity atom does not
prove a ping protocol or compositor. This batch does not advertise new atoms,
change style/configuration behavior or make absent optional protocols mandatory.
W03 is project-wide input validation, not a fabricated single EWMH clause.

## Selection owners, requestors and consumers

Owner: SELECTION-01 / DATA-01, with INPUT-01 for bounded servicing and
CLIPBOARD-01 / FILE-VIEW-01 for later services. Sources:
[icccm2.c](../../fvwm/icccm2.c),
[FSelection.c](../../libs/FSelection.c),
[FvwmScript.c](../../modules/FvwmScript/FvwmScript.c),
[TextField.c](../../modules/FvwmScript/Widgets/TextField.c),
[ReadXServer.c](../../modules/FvwmForm/ReadXServer.c).
Fixtures: [legacy_property_live.py](../../tests/live_x/legacy_property_live.py),
[selection_live.py](../../tests/live_x/selection_live.py),
[test_selection_x.c](../../tests/test_selection_x.c).

| ID | Parent | Clause / role observable | Status and evidence | Remaining acceptance |
| --- | --- | --- | --- | --- |
| S01 | S01 | ICCCM 2.8/4.3: WM_Sn ownership is acquired and confirmed. | Verified for manager-handoff checkpoint profiles: server-time acquisition, competing owner and non-replacement refusal. [Report](VERIFICATION-2026-10-21-MANAGER-HANDOFF.md). | Multi-screen selection, broader acquisition/refusal races and post-startup ownership epochs remain open. |
| S02 | S01 | ICCCM 2.8 SHOULD: MANAGER announcement identifies timestamp, selection and owner. | Verified for manager-handoff checkpoint profiles: full initialized event and actual owner, observed before startup. [Report](VERIFICATION-2026-10-21-MANAGER-HANDOFF.md). | Retain source/profile scope; multi-screen and wider acquisition races remain open. |
| S03 | S01 | ICCCM 2.8 plus project bound: replacement releases old resources finitely. | Verified for manager-handoff checkpoint profiles: finite old-owner wait, SIGTERM and superseding-owner refusal; startup connection cleanup. [Report](VERIFICATION-2026-10-21-MANAGER-HANDOFF.md). | Individual Xlib calls still require a responsive server; broader race/flood and multi-screen coverage remain open. |
| S04 | S01 | ICCCM 2.6.2 required: owner TARGETS conversion lists supported targets. | Verified for named profiles, L: legacy fixture reads exact four core targets via MULTIPLE. | Direct conversion and ownership epochs remain unqualified; list equality is not target semantics. |
| S05 | S01 | ICCCM 2.6.2/4.3 required: TIMESTAMP and VERSION conversions return typed values. | Implemented / unqualified. `convertProperty` has both branches; L does not read their successful results. | Direct INTEGER/32 values, lengths and acquisition epoch; repeat after replacement. |
| S06 | S01 | ICCCM 2.6.2 required: MULTIPLE preserves order and marks failed pairs. | Implemented / unqualified for complete target behavior. L covers malformed/empty pairs, unsupported/None results and TARGETS. | Successful mixed targets, sequential conversion ordering, resource refusal and vanished requestor. |
| S07 | S02 | ICCCM 2.6.2 required: PRIMARY owner supports TARGETS/TIMESTAMP/MULTIPLE. | Implemented / unqualified, missing branches: Script PRIMARY handles only STRING. | Add role-correct conversion, timestamps and failure replies while preserving stored configuration text. |
| S08 | S02 | ICCCM 2.1/2.3: PRIMARY acquisition and ownership loss are handled. | Implemented / unqualified. TextField uses event time; SelectionClear unselects text. | Foreign owner replacement, stale acquisition, selection lifetime and request-after-loss. |
| S09 | S03 | ICCCM 2.5: replies correlate with the outstanding request. | Implemented / unqualified for the full correlation contract. The helper checks all five fields; L tests selected mismatches and late replies, not every field independently. | Add wrong-selection replies as well as existing wrong-requestor/target/time/property cases; retain isolation and owner-change/flood limits. |
| S10 | S03 | ICCCM 2.5/2.7 plus project bound: receive typed, complete text. | Verified for named profiles, L: ordinary bounded UTF8/STRING, type/format/size refusal and recovery. | Negotiated targets/encodings and incremental transfer remain outside these ordinary-property cases. |
| S11 | S04 | ICCCM 2.2/2.6.2: review private selection-owner target obligations. | Review pending. Script private messages implement custom targets and refusal, not generic target negotiation. | Define this role's standard target responses without changing message ordering/bytes; do not claim clipboard ownership. |
| S12 | S05 | ICCCM chapter 3: cut-buffer consumer reads STRING/8. | Verified for named profiles, L: Form bounded paste and malformed/UTF8 boundary recovery. | Not applicable with rationale to PRIMARY owner-target obligations: this consumer owns no selection. |
| S13 | S06 | ICCCM 2.5/2.7.2: requestor accepts incremental transfer. | Implemented / unqualified, unsupported path: INCR currently refused. | Operation-local chunking, size/deadline bounds, zero terminator, cancellation and owner death. |
| S14 | S06 | Project bound: silent/dead selection peers cannot stall servicing indefinitely. | Verified for named profiles, L: finite helper refusal, stale requestors and high-descriptor coverage. | Scope remains one bounded transfer; broader interaction/flood servicing needs owner fixtures. |
| S15 | S07 | Planned clipboard-manager role: persistence handoff. | Planned. No CLIPBOARD_MANAGER/SAVE_TARGETS service adopted. | CLIPBOARD-01 selects revision, targets and protected lifetime; foreign clients must establish handoff before claims. |
| S16 | S08 | Planned native XDND participants: explicit source/target roles. | Planned. No native XDND event handlers found in the inspected core/modules. | FILE-VIEW/SELECTION selects version and exact participants; managed toolkit clients keep their own roles. |

S02 began as a source inspection finding: the earlier `SetupICCCM2` left
MANAGER owner data uninitialized. The subsequent
[manager checkpoint](VERIFICATION-2026-10-21-MANAGER-HANDOFF.md) reproduces
announcement and wait failures with the preserved old core and qualifies the
three runtime repairs. This is not an exploit claim. The inventory receipt
above remains the historical inspected source; the manager report records the
later runtime inputs, peer cases and explicit remaining coverage.

## Bounded handoff and acceptance

The three inventory stories are complete when all forty rows have a revision/
clause or explicit planned-role rationale, owner, source, status, evidence
boundary and remaining check. Every older E/S row maps into this set; none of
the twelve monitor rows is silently closed. This set is closed as an inventory,
including unresolved findings; STANDARDS-01 as a whole remains open.

The next concrete runtime candidates retain the authoritative tracker order:

1. EWMH desktop geometry: retain qualified R07–R10/R12 admission/array scope
   above. R11 reserved edges, dock/topology and broader per-monitor geometry need
   independent clients before changing their owning publication policy.
2. Manager targets: S04–S06 and the broader S01–S03 coverage above. The bounded
   startup handoff is locally qualified; direct targets and wider ownership epochs
   retain independent-client, ordering and lifetime acceptance.
3. Script selection and transfer: S07–S14, preserving private-message ordering,
   text bytes and continued input. Negotiate owner targets before richer clipboard
   services; INCR refusal is not incremental interoperability.

These are owner handoffs, not new priority overrides. No duplicate issues, new
mandatory dependencies, generic protocol framework or repeated full conformance
sweep is queued. Optional W02/W05/W07–W10 and S15/S16 keep their existing stages.
The future affected runtime checkpoint runs its focused tests and one combined
matrix. This documentation-only checkpoint uses source inspection, register/
link/fingerprint validation and repository checks; unchanged runtime evidence
remains historical rather than being counted again.

Performance reference PR132 stays at two subsequent merged implementation
checkpoints (PR133/134). PR135/136 are open at this audit. An inventory adds no
implementation checkpoint or speed/size claim, so no benchmark run is due.
