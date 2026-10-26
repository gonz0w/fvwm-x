# Three-release roadmap and GitHub reconciliation

Date: 2026-10-26. Status: contract/mirror checks and final independent publication review pass. Owner: Cam. This is planning and metadata evidence;
it establishes no new runtime, performance, security or release qualification.

The planning diff is based on unchanged PR256 head
`cf37865e4fee0ca48cad27fd6b5587c0541849d6`. PR256 remains open. The signed
planning branch and [PR261](https://github.com/gonz0w/fvwm-x/pull/261) preserve
its separate review. Neither PR is merged and no release/tag is published.

## Observed results

- All 87 pre-existing open issue contracts are rewritten with a release,
  Major/Minor breadth, end result, acceptance criteria, accepted-work notes and
  exclusions. Four new finite owners cover two parked children and R2/R3 gates:
  #257/#258/#259/#260. No issue is closed or reopened by this reorganization.
- Exact readback matches all 91 issue titles, bodies, labels and milestones.
  Project titles, milestones, Work size, Work batch, Status, Priority and Stage
  match the issue contracts. R1 has 65 issues (60 Major/5 Minor), R2 22
  (18 Major/4 Minor), and R3 four Major issues. These counts exclude PR cards.
- All current feature-bearing contracts and evaluation decisions target R1.
  Evaluations may adopt/defer/reject; an adopted choice needs its qualified R1
  integration. No candidate dependency is forced by a scheduling label.
- Only the three release milestones remain open. Ten old stage/Later milestones
  were closed after direct queries verified zero open assigned items; completed
  milestone/issue history remains intact. No calendar dates were invented.
- All previous native prerequisites and parents remain. Four native parent
  links and 93 explicit release/policy prerequisites were added. The resulting
  151-edge blocked-by graph and parent hierarchy are acyclic. No R1 issue is
  blocked by an open R2/R3 owner. Release gates aggregate their finite release
  manifests; coordination references do not become blanket blockers.
- The old 65,584-byte UTF-8 CLEAN body is preserved exactly in the linked text
  snapshot because it was near GitHub's body limit. Other original bodies remain
  fenced under historical details, retaining exact text without counting old
  unchecked tasks as current acceptance. Accepted source-bound evidence stays
  accepted.

## Status and error dispositions

R2 #4/#6/#7/#15/#34 now describe unimplemented remaining manifests, so their old
Review/In progress labels become Backlog. Actual PR-state checks confirm merged
#238/#241/#254; their component work remains accepted. PR256 remains R1 Review
under #209 while #34's new R2 remainder is Backlog. Each issue records its own
reason. #208 moves from active planning to Review with PR261. Other pre-existing
statuses are retained. Status changes do not claim an audit complete.

Metadata setup retained overlong-label-description HTTP422 responses, then
validated the API length limit before successful creation. A transient connector
error on #57 was resumed with a fresh read before retry; final exact readback
passes. Initial Project readback exposed four missing new-card P2 fields; only
those fields were filled, and final field checks pass. All metadata corrections have matching final readbacks.

## Validation and limits

`make check lint`, repository links, working/staged whitespace and YAML parsing
of the issue forms pass. A bounded validation script checks unique 91-record
coverage, every current feature/evaluation target, issue/Project parity, scope
counts, native graph/hierarchy acyclicity, old relationship preservation and
the original CLEAN-body bytes. The [machine receipt](VERIFICATION-2026-10-26-RELEASE-ROADMAP.json)
records snapshot identities and exact counts; private before/after JSON and API
receipts are retained under `builddir/codex-2026-10-26/release-roadmap`.

No runtime source, VM workload or OS/compiler/install matrix is exercised for
this documentation/metadata change. Existing native/platform/hardware gates
remain with their finite implementation and release owners. Numeric speed/RSS
budgets are required future #29/#209 outputs; this plan invents no measurements
or passing budget result. Future requests enter later release intake unless the
owner changes frozen R1 scope. The two parked families remain blocked on a
supported review route; no source inspection or retry occurred.

Requested Astra/high analysis and Sol/high drafting/separate review follow the
project routing; actual runtime model metadata remains unavailable/unverified.
