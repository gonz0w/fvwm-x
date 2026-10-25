# Portable and OpenBSD hardening follow-ups

Date: 2026-10-25. Owner: Cam (@gonz0w).
Status: issue scope approved; implementation and qualification pending.

The user requested a review of accepted hardening and outstanding plans before
proposing additional issues, then explicitly requested that those issues be
added or updated. This record captures that planning decision. The
[tracker](../PROJECT-TRACKER.md#execution-order) retains execution authority;
GitHub issues and Project fields mirror it.

## Existing work and evidence boundary

The review read the current tracker, security status, issue bodies, existing
verification reports and selected source, including the image worker, common
process launcher, build hardening and executable inspector. The primary shared
checkout had older documentation, so current GitHub source/status was checked
before issue planning. No new runtime tests, complete vulnerability scan or
release qualification were performed for this review.

Accepted input/property/packet validation, descriptor/constructor ownership,
bounded queue/service/deadline policies and the qualified asynchronous image
owners remain accepted in their recorded source/platform scope. The finite
safety closeout through PR143 remains accepted. Current issues record merged
PR230's Script message cleanup and the next bounded GetOutput ownership slice.
The issue additions below do not reopen those components or claim broader
audit completion.

The reviewed image worker already has checked process/resource containment:
descriptor cleanup, finite memory limits, disabled worker core dumps, deadlines
and validated result transport. It does not establish an OS filesystem/syscall
sandbox. SVG decoding and image effects share an executable, but effects opens
an X connection. Their required authority must be reviewed separately.
The existing executable-hardening inspector is registered on Linux and covers
C targets; that does not establish BSD or Go protection coverage.

These observations identify defense-in-depth and evidence gaps. They do not
establish a new exploit, remote compromise or privilege escalation.

## Approved issue map

All six additions are open P2 Backlog items. Scope and acceptance live in the
linked issue; these summaries do not create a competing implementation queue.

| Tracker ID / issue | Outcome | Stage / Work batch | Native parent |
| --- | --- | --- | --- |
| HARDEN-01 [#231](https://github.com/gonz0w/fvwm-x/issues/231) | Measurable portable/OpenBSD requirement, authority and evidence map with justified exceptions. | 05 / D3 Sandbox evaluation | None; cross-cutting coordination. |
| BOX-02 [#232](https://github.com/gonz0w/fvwm-x/issues/232) | OpenBSD image-worker pledge and locked unveil after the accepted finite policy. | 05 / D3 Sandbox evaluation | BOX-01 #41. |
| BOX-03 [#233](https://github.com/gonz0w/fvwm-x/issues/233) | FreeBSD worker capability mode with reduced descriptor rights after the accepted finite policy. | 05 / D3 Sandbox evaluation | BOX-01 #41. |
| BOX-04 [#234](https://github.com/gonz0w/fvwm-x/issues/234) | Linux worker syscall/filesystem restrictions with detected support and explicit effective protection. | 05 / D3 Sandbox evaluation | BOX-01 #41. |
| VERIFY-02 [#235](https://github.com/gonz0w/fvwm-x/issues/235) | Produced/installed C and Go binary protections with platform-appropriate checks and exceptions. | 01 / Qualification | VERIFY-01 #5. |
| VERIFY-03 [#236](https://github.com/gonz0w/fvwm-x/issues/236) | Bounded OpenBSD ownership/failure/teardown qualification with MALLOC_OPTIONS=S. | 01 / Qualification | VERIFY-01 #5. |

Existing owners receive bounded additions rather than duplicate umbrella issues:

- BOX-01 [#41](https://github.com/gonz0w/fvwm-x/issues/41) accepts each finite
  platform/mode capability and access policy before its implementation child.
  Its stale geometry outcome is corrected to helper sandbox evaluation.
- VERIFY-01 [#5](https://github.com/gonz0w/fvwm-x/issues/5) retains native and
  release evidence and links both new verification children. FUZZ-01
  [#227](https://github.com/gonz0w/fvwm-x/issues/227) already owns local fuzzing.
- FD-01 [#7](https://github.com/gonz0w/fvwm-x/issues/7) inventories finite
  runtime-file/temporary-file/endpoint and descriptor trust boundaries: checked
  ownership/mode/type, creation/replace races, inheritance and cleanup.
- PROC-01 [#15](https://github.com/gonz0w/fvwm-x/issues/15) inventories finite
  executable lookup, argv, environment, working-directory and process-lifetime
  boundaries. Intentional configured shell commands stay supported.
- IMAGE-01 [#8](https://github.com/gonz0w/fvwm-x/issues/8) coordinates worker
  permission containment with its existing input/resource/lifetime owners.
- BUILD-PROFILES-01 [#226](https://github.com/gonz0w/fvwm-x/issues/226)
  coordinates real profile/artifact identities and costs with VERIFY-02.

Existing issues keep their status, priority, milestone and native prerequisites.
Parent links scope ownership; they do not require every optional part of an
umbrella to finish before a finite child. A worker implementation depends on its
accepted platform policy inside #41, not the whole hardening profile or all
platform sandboxes. No blanket parent-completion blocker is added.

## Required boundaries and acceptance

Start with narrow workers. Inventory startup and lazy library/font access,
relative SVG resources, descriptors, filesystem access, network access and
execution before selecting restrictions. The decoder and X-connected effects
mode need distinct policies. Preserve formats, configuration syntax, themes,
relative resources, reload/restart, scale/alpha/options and existing generations,
deadlines and output/memory bounds. Required containment setup failures must
produce explicit refusal and recovery rather than silent unsandboxed retry.

OpenBSD uses staged pledge and locked unveil. FreeBSD uses capability mode and
reduced descriptor/ioctl/fcntl rights with required access prepared explicitly.
Linux evaluates seccomp, no_new_privs and Landlock against actual kernel and
architecture support. Define strict refusal or an explicitly documented reduced
profile where support is unavailable; never label partial protection as full
containment. These mechanisms do not have identical semantics.

Test allowed work and attempted forbidden operations in dedicated workers:
unrelated file access, writes, network/X connections and execution as applicable,
plus setup errors, timeout, cancellation, partial output, child exit and later
recovery. Check affected platform APIs early, with both supported architectures,
installed consumers and relevant X fixtures. Preserve accumulated milestone and
release gates, and distinguish emulated from native-CPU evidence.

VERIFY-02 checks actual artifacts, supported compiler/loader/kernel properties
and disabling options. Do not impose Linux dynamic-linker tags or GNU
fortification assumptions on every BSD/Go binary. Distinguish emitted metadata,
toolchain defaults and observed runtime enforcement. VERIFY-03 exercises a
finite existing test subset with OpenBSD allocator auditing; it changes no
production allocator default and does not replace injected failures, Linux
ASan/UBSan, Go testing or fuzzing.

An ordinary trusted X11 connection remains powerful even inside a restricted
process. These controls do not promise confidentiality isolation between trusted
clients sharing a display. Keep unnecessary X access out of decoder workers and
record remaining authority honestly. Preserve deliberate user-configured shell
execution separately from project-owned helper argument/environment policy.

## Scheduling and non-goals

Preserve the current maintenance order and selected M5/GetOutput slice. The new
issues belong to D3's bounded policy/worker lane and VERIFY-01's conditional
qualification lane. Issue planning authorizes the recorded scope; it does not
select every implementation now, turn historical broad audits into new blockers,
reopen finished checkpoints or supply release acceptance.

No whole-core sandbox, general broker service, replacement decoder, mandatory
desktop package, configuration migration, hosted runner or release publishing is
selected. Later implementation must follow the existing configuration-only
compatibility authority and other product constraints. No comprehensive OpenBSD
compliance or certification claim is made.

## References

- [OpenBSD ports security recommendations](https://www.openbsd.org/faq/ports/guide.html#Security)
- [OpenBSD security approach](https://www.openbsd.org/security.html)
- [OpenBSD mitigation mechanisms](https://www.openbsd.org/innovations.html)
- [OpenBSD 8.0 pledge](https://man.openbsd.org/OpenBSD-8.0/pledge.2)
- [OpenBSD 8.0 unveil](https://man.openbsd.org/OpenBSD-8.0/unveil.2)
- [OpenBSD 8.0 malloc and allocator auditing](https://man.openbsd.org/OpenBSD-8.0/malloc.3)
- [FreeBSD capability mode](https://man.freebsd.org/cgi/man.cgi?query=cap_enter&sektion=2)
- [FreeBSD descriptor rights](https://man.freebsd.org/cgi/man.cgi?query=cap_rights_limit&sektion=2)
- [Linux seccomp](https://docs.kernel.org/userspace-api/seccomp_filter.html)
- [Linux no_new_privs](https://docs.kernel.org/userspace-api/no_new_privs.html)
- [Linux Landlock](https://docs.kernel.org/userspace-api/landlock.html)
- [X.Org protocol security model](https://www.x.org/guide/communication/)
