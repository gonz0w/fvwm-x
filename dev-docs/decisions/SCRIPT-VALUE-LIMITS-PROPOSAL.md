# FvwmScript value storage limits — approved

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

The user approved these defaults on 2026-10-05. Implementation and cross-platform verification are complete. See the
[storage verification report](../verification/VERIFICATION-2026-10-05-SCRIPT-STORAGE.md). ExecutionLimit 10000 and
RecursionLimit 64 remain separate limits.

## Accepted defaults

- One interpreted string: 1 MiB, excluding its terminating byte.
- Interactive TextField title: the same ValueLimit, approved separately on
  2026-10-05; see the [extension](SCRIPT-TEXT-LIMIT-PROPOSAL.md).
- Stored constant and variable text: 32 MiB, including names and terminators.
- ValueLimit and MemoryLimit overrides: 1–2147483647 bytes, selected before
  launching the module. Invalid settings retain their prior values.

Instruction counts cannot bound string growth. The largest inspected bundled
script source was 26722 bytes; source size does not measure runtime values or
guarantee compatibility with third-party themes. These defaults provide finite
bounds and configurable headroom.

## Failure behavior

Runtime assembly checks sizes before growing or publishing interpreted values.
Refusal leaves the last published value intact, reports the relevant limit and
stops the current handler through normal cleanup. Later events can replace
existing values with smaller text. Storage usage persists between events;
completed earlier side effects remain.

Parsing refuses oversized tokens or table entries before building and mapping
the script UI. Replacement credits the outgoing value before checking the new
total. Arithmetic checks also reserve room for terminating bytes.

MemoryLimit covers the variable/constant table, including names. It does not cap
the whole process, Xlib/font/image/widget allocations, parser input buffers,
output caches or permissions of external commands. ValueLimit also guards
interpreted temporary assembly; it does not imply a combined temporary-memory
or process-memory ceiling.

## Verification

Native coverage checks exact limits, first excess, overflow, replacement credits,
shrinking, unchanged accounting on refusal and repeated refusal. Real-module
coverage checks default and configured limits, stopped handlers, continued
events and diagnostics. Full suites also retain previous widget and execution
coverage on Linux and both BSD architectures/default X servers.

## Compatibility decision

[AGENTS.md](../../AGENTS.md) requires an explicit decision for a specific
compatibility break. These defaults can reject formerly accepted scripts with
unusually large strings. The user's approval selects 1 MiB and 32 MiB with
configurable overrides; it does not change other command permissions.
