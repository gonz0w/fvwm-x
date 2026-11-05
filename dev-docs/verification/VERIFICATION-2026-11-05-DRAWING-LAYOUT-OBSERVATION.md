# Drawing and layout observation checkpoint

G7 prepares the existing private drawing and layout fixture for BSD process
measurements under [#211](https://github.com/gonz0w/fvwm-x/issues/211) and
[#212](https://github.com/gonz0w/fvwm-x/issues/212). It also recovers package
facts from the [G6 cost receipts](VERIFICATION-2026-11-04-DRAWING-LAYOUT-COSTS.md).
Both owners remain open.

## Process observation

The Linux fixture keeps its existing /proc sampling and mapped executable
check. On BSD the fixture finds a direct child of its owned WM and binds the
group, effective UID, start time, private invocation path and module alias.
It checks the selected file's hash and keeps that check separate from the
process identity. BSD command text is a snapshot of the invocation and
provides no mapped executable hash. A second-resolution start time limits
detection of very fast PID reuse.

The sampler queries the bound PID at workload and idle endpoints. It retains
the displayed cumulative CPU time, its centisecond resolution, and resident
memory in KiB. A missing endpoint or decreasing CPU counter fails the
performance measurement gate. High-water memory and wakeups are unavailable
on this route.
An unchanged displayed counter means no increment was visible at that
resolution. The BSD startup receipt identifies its invocation endpoint and
includes the cost of the observation. Compare paired workloads within the
same profile and retain the original Linux startup evidence separately.

The field choices follow the [OpenBSD 8.0 ps manual](https://man.openbsd.org/OpenBSD-8.0/ps.1)
and [FreeBSD 15.1 ps manual](https://man.freebsd.org/cgi/man.cgi?query=ps&sektion=1&manpath=FreeBSD+15.1-RELEASE).
The implementations document CPU rounding and formatting in
[OpenBSD print.c](https://github.com/openbsd/src/blob/master/bin/ps/print.c) and
[FreeBSD print.c](https://github.com/freebsd/freebsd-src/blob/releng/15.1/bin/ps/print.c).

## Recovered FreeBSD package facts

These nominal sizes and license labels come from successful `pkg info -f`
records on FreeBSD 15.1-RELEASE-p4. They describe whole packages, including
files beyond the resolved shared objects. The XCB Render and SHM interfaces
are both listed under libxcb.

| Package as recorded | License labels | ARM64 flat size | AMD64 flat size |
| --- | --- | ---: | ---: |
| cairo-1.18.2,3 | LGPL21, MPL11 | 3.77 MiB | 3.86 MiB |
| pixman-0.46.4 | MIT | 742 KiB | 869 KiB |
| libxcb-1.17.0 | MIT | 5.00 MiB | 5.03 MiB |

The G6 structured receipt binds these six raw files under each architecture's
`freebsd-repair01/freebsd-<arch>-run01/export/g6-platform-cost01/metadata/raw`
directory. Cairo is `122-pkg-info-001.stdout`, Pixman is
`142-pkg-info-021.stdout`, and libxcb is `141-pkg-info-020.stdout`.
The aggregate query failed because its format used bare `%s`. A later
metadata-only collection can use the byte form `%sb` after checking the
installed tool. The complete manifest, unresolved owner lookups, actual
runtime loader file identity and incremental installed cost remain pending.

## Verification

The host ran 26 graphics fixture, observer and record tests and six layout
record tests. All passed. The new observer tests use controlled ps replies
for valid ownership, foreign and stale identities, ambiguous discovery and
unavailable metrics. Repository and whitespace checks passed.

## Shared checkpoint

Start with one positive fixture run on an owned FreeBSD profile and one on an
owned OpenBSD 8.0 profile. Confirm the actual ps format, binding of both module
children, sample availability, cleanup and source/binary identities before
using the sampler for paired measurements. Reuse eligible builds and the
qualified AMD64 PNG prefix under the G6 identity checks. Add comparison runs
and relevant consumer checks together at the shared runtime checkpoint.

The roundtrip criterion still needs an observation design with a defined
counted wait, coverage of both rendering paths and controlled known-wait and
no-wait cases. The existing observer supplies issued-request counts and
operation boundaries. It supplies no measured count of implicit waits.
