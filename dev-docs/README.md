# FVWM-X developer documentation

Start here for current project guidance. The tracker owns priorities and status;
source/tests determine behavior. The former GSD system and inherited FVWM notes
are preserved under history rather than mixed into active instructions.

The published runtime starts from the qualified core/utility snapshot. Shared
project documents also describe separately owned desktop/artwork work and dated
checkpoints. Each report retains its original source identity and limitations;
restoring that documentation does not publish or requalify its runtime inputs.

| Guide | Purpose |
| --- | --- |
| [Project](PROJECT.md) | Product scope, compatibility and migrated monitor/HiDPI requirements |
| [Project tracker](PROJECT-TRACKER.md) | Authoritative execution order, backlog, decisions and evidence |
| [Three-release plan](decisions/RELEASE-PLAN-2026-10-26.md) | R1 features and finite R2/R3 maintenance contracts with Major/Minor scope |
| [Final release gate](RELEASE-GATE.md) | Public-launch review after coding checkpoints and the user's release decision |
| [Development](DEVELOPERS.md) | Coding, ownership, bounded work and change verification |
| [Architecture](ARCHITECTURE.md) | Maintained subsystem/source map |
| [Workflow](WORKFLOW.md) | Project upkeep, source navigation and optional Codex integrations |
| [Documentation](DOCUMENTATION.md) | Standard document structure, man quality and lightweight HTML |
| [Standards](STANDARDS.md) | Adopted protocols, coverage owners, independent tests and scoped conformance claims |
| [Theme standards](THEME-STANDARDS.md) | Native accessibility guidance and measurable theme/configuration defaults |
| [Support](SUPPORT.md) | Linux/FreeBSD/OpenBSD AMD64/ARM64 qualification |
| [BSD](BSD.md) | Local BSD verification and source isolation |
| [Security status](SECURITY-STATUS.md) | Current findings and remaining evidence |
| [Package audit](DEPENDENCY-AUDIT-2026-10-05.md) | Actual packages, signed refresh and security qualification |
| [Cleanup roadmap](CLEANUP-ROADMAP-2026-10-05.md) | Runtime cleanup, libraries and maintenance migration details |
| [Optimization roadmap](OPTIMIZATION-ROADMAP-2026-10-05.md) | Queue, X11, stack, style and cache findings/dependencies |
| [Decisions](decisions/README.md) | Proposed/accepted contracts and documentation direction |
| [Verification](verification/README.md) | Dated source/profile scope, results, failures and limitations |
| [History](history/README.md) | Retired milestone planning and inherited upstream documents |
| [Maintenance tools](../tools/maintenance/README.md) | Adopted tooling and individually gated candidates |

User documentation lives in [doc/](../doc), including
[the desktop manual](../doc/FvwmDesktop.adoc), and generates installed man pages.
[Development screenshots and configuration examples](../doc/development-preview.md)
show the optional native browser across three actual FVWM theme configurations.
The gallery is a repository guide, not an installed manual or release claim.
[INSTALL.md](../INSTALL.md) describes current build/dependency requirements.
Developer guides/history are not installed product components.

Root AGENTS.md contains concise current repository instructions. New evidence
belongs in verification and new proposals/decisions in decisions. Preserve old
reports and failed checks; do not treat an archived plan or configured workflow
as passing evidence. Read the relevant scope before citing a result.

Use the tracker's [current checkpoint summary](PROJECT-TRACKER.md#current-checkpoint-status)
for accepted, running and queued work. Dated reports retain the exact source,
results and limitations of their own checkpoints; they are not a second active
queue. The public website/manual redesign remains deferred under
[DOCS-01](decisions/WEBSITE-MANUALS-BRIEF.md).
