# Safety and maintainability closeout

Status: approved direction from the user on 2026-10-21; implementation and
qualification remain with the owning tracker items. The
[tracker](../PROJECT-TRACKER.md#hardening-closeout) owns the finite pickups,
dispositions and handoff criteria. This record is not a second work queue.

The user considers the larger performance problems sufficiently addressed for
this release and requests major safety/maintainability work before moving to
features and later milestones. Remaining performance-only tuning is deferred
to the next release. Accepted implementations and their source-scoped evidence
remain accepted; deferred work is not marked complete.

Prioritize malformed external input, unchecked arithmetic, crashes, hangs,
unbounded work, resource/process ownership and failure-path cleanup. Prefer
small typed boundaries and explicit ownership/results where they make a named
contract safer or support an upcoming feature. Cosmetic refactoring, generic
API renames, speculative cache/style tuning, timer-heap/XCB experiments and
build-fanout work without a concrete safety/dependency need do not extend this
release's foundation phase. A safety defect stays in scope even when its owner
was historically categorized as responsiveness or optimization.

Use bounded entry-point reviews and coherent implementation batches. Each
selected case needs a disposition with exact source/evidence or an explicit
owner and deferral reason; confirmed high-impact safety defects must be
resolved before handoff. Already-qualified paths do not need replacement to
count as reviewed. Further broad inventories do not automatically enlarge the
closeout. Advance existing feature dependencies when the handoff criteria are
met, retaining configuration, platform and approved desktop scope.

Keep the requested five-checkpoint performance observations as lightweight
regression telemetry using existing binaries. They do not create tuning work
from noise or require new benchmark builds before safety progress. Preserve
required affected correctness gates and separate native-CPU sanitizer,
physical/toolkit/compositor and final release acceptance. Feature development
does not imply release acceptance or authorize publishing a release.

This decision changes priorities and scope only. It introduces no new runtime
behavior, dependency, sandbox design or claim of whole-project security.
