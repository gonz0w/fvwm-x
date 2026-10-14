# LIB-01 / IPC: valid UTF-8 JSON output

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-07; implementation and qualification
pending. The compatible vendor update and numeric/cleanup checks are separate.

## Concrete observation

The production cJSON call-site inventory contains serialization in core status,
monitor/topology diagnostics and MFL events. No production parse, duplicate or
cJSONUtils JSON-Patch call was found. A finite private-X core/MFL probe sends one
14-byte `Echo legacy-` command with a single legacy byte, plus its newline. MFL
places that byte directly inside a JSON string. A strict UTF-8 JSON reader rejects
that record with UnicodeDecodeError; a subsequent ordinary Echo remains healthy.
The response hex, source, bounded driver and core/server logs are under
`builddir/codex-2026-10-07/cjson-review/`. This is a protocol correctness
observation; it is not a demonstrated compromise.

The upstream 1.7.19 string printer retains the same raw-byte behavior. A version
bump alone therefore does not make these records valid UTF-8. RFC 8259 section
8.1 specifies UTF-8 for JSON exchanged outside a closed ecosystem:
[JSON interoperability](https://www.rfc-editor.org/rfc/rfc8259#section-8.1).
Existing strict Python/Go consumers make this an actual application boundary.

## Approved contract

At the JSON string printing boundary, keep well-formed UTF-8 unchanged and replace
each byte that cannot participate in a well-formed UTF-8 scalar sequence with
Unicode replacement character U+FFFD, encoded as the JSON escape `\ufffd`.
Handle invalid leads/continuations, truncated sequences, overlong encodings,
UTF-16 surrogate scalars and values above U+10FFFF deterministically. Continue
processing the record and subsequent events; do not terminate the WM/MFL because
of a text encoding error.

Keep the core's original configuration/theme strings, command syntax and binary
module packets unchanged. This proposal changes their JSON presentation only;
it does not impose a new UTF-8 admission rule on FVWM command input or silently
reinterpret legacy bytes using a guessed locale. Existing ASCII/control/quote
escaping and valid Unicode output remain unchanged. JSON object keys use the
same rule as string values. Trusted numeric Raw fields remain separate.

Use a linear checked size pass and a linear output pass, with no second global
state or new mandatory dependency. Retain the printer's existing finite integer
size ceiling; validate replacement expansion and offsets before addition or
allocation. Add native policy cases for valid/boundary/invalid UTF-8, size guards,
fixed-buffer failure and allocation cleanup, plus actual core/MFL JSON events.
Combine Linux/BSD/default-X/optional/sanitizer qualification with the reviewed
cJSON update at the next checkpoint.

## Compatibility decision

A consumer that depends on the exact invalid legacy bytes in a JSON record will
see replacement characters instead. Normal FVWM rendering and native module
payloads keep their original bytes. This is a specific output compatibility
change and needs the user's decision under [AGENTS.md](../../AGENTS.md):
“A specific compatibility break requires an explicit user decision.”

Alternative: retain the historical raw bytes. That preserves this behavior but
leaves the observed record unreadable by a strict UTF-8 JSON client. Do not claim
that the compatible vendor update repairs this encoding boundary.

Implementation and the eight-path combined frozen local checkpoint pass. [Evidence](../verification/VERIFICATION-2026-10-07-JSON-BOUNDARY.md).
