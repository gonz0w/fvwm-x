# Temper Terra product routing evidence — 2026-10-27

TEMPER-03 is the [owning work item](../PROJECT-TRACKER.md#temper-terra-product-routing--temper-03).
The owner requested this additive adoption. The candidate is qualified for
maintainer review; merge remains separately authorized. [Project policy](../PROJECT.md#codex-routing-selection-and-policy-change--2026-10-24)
controls local routing and takes precedence over the selected profile.

## Source and local selection

The source record pins [Agent Temper](https://github.com/gonz0w/agent-temper/tree/fcfb819f62d1a1091010db1df8fc5b7560cd1b8a)
revision `fcfb819f62d1a1091010db1df8fc5b7560cd1b8a`. The lead reports that
it fetched the canonical GitHub commit through the connector. The retained bundle
contains nine upstream files, including the license, plus local `SOURCE.md` and
`SHA256SUMS`: eleven files in all. The new product TOML is a local adaptation
of the upstream template, outside the bundle.

A source-only inspection compared the nine local upstream file hashes with the
lead-supplied private byte map; all nine matched. The map identifies only
`profiles/codex.md` as changed from the prior pin. The adapted product TOML hash
also matched the map. This comparison depends on that supplied map; it is not an
independent authentication of the upstream fetch. The local source record identifies
the pin, inventory and license. The lead additionally compared all nine retained files byte-for-byte with the
immutable Git objects, verified the complete eleven-file inventory and checksums,
and confirmed all four original role definitions remain byte-for-byte unchanged.
Separate review found no source, inventory or policy blocker.

The selected Codex profile adds `temper_product` for bounded documentation,
visual inspection, authorized browser tasks and small specified UI fixes.
The four existing routes remain selected: workflow Luna/medium, implementer
Sol/high, analyst Astra/high and reviewer Sol/high. The lead may explicitly assign
Sol/high implementer fallback if Terra or required tools are unavailable, recording
the choice. There is no silent fallback or nested helper delegation. The lead owns
integration and separate final review. This adoption changes no runtime code,
default model, permissions or security settings.

## Bounded assignment and limits

This report is the bounded product documentation assignment. The lead supplied
facts, scope and file ownership and requested `gpt-6.1-terra`/medium with the
product instructions. Native named-role selection is unavailable in this session;
the project's explicit model and instructions mapping is used. The request and
this agent's self-description do not independently attest backend model identity
or automatic role-file loading. No client version or runtime routing verification
is claimed. The outer checkout lacks role definitions; the active publish checkout
contains five.

Static TOML parsing in the active checkout succeeded for all five role files.
Their names, model and effort fields match the local routing table, including
`temper_product` at `gpt-6.1-terra`/medium. Parsing establishes configuration
syntax and stated values only. It does not establish role discovery or execution
on the requested model. No browser, hardware, UI, build, VM or runtime test was
performed for this assignment.

## Final recorded results

The explicit Terra/medium assignment completed and saved this report; no
model-unavailable response was encountered. This establishes an accepted explicit
assignment and returned work, not independently attested backend model identity.

`make check lint`, working/staged whitespace checks, TOML/policy consistency,
original-role byte comparison, immutable upstream byte comparison and complete
inventory/checksum validation pass. Local source/link checks pass. Separate
`temper_reviewer` Sol/high review found no actionable blocker. The lead retains integration; merge requires explicit maintainer authorization. No runtime/native/build/VM or public browser
checks were run or required for this documentation/configuration-only change.
PR271's feature branch remains untouched; performance counters do not advance.
