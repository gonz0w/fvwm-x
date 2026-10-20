# Process and session startup transactions

Status: implemented; focused Linux/FreeBSD checks pass; combined qualification
pending. Owners: FD-01 / PROC-01 / DATA-01; main base is merged PR132.

Three related runtime improvements:

- Core Exec checks fork, opens and duplicates /dev/null before launching the
  shell, and uses _exit on child setup/exec failure. It uses fork's owned copy
  of the command instead of allocating a duplicate string.
- Session authentication obtains every network string and cookie before writing
  or publishing credentials; failed construction cleans both temporary files
  and all prepared entries. File/helper failure also precedes publication.
- Session startup checks composed network identifiers and setenv, publishes
  only after watch admission, and removes watches/resources after failure.
  Selectable listener count bounds precede authentication-size arithmetic.

No command names, arguments, configured shell text, module packet layouts,
logging inheritance, X ordering barriers or dependencies change. Pure controls
wrap real production APIs and actual core objects. Existing real-X session and
compatibility drivers cover successful use and descriptor inheritance.

The initial Linux probe compile failed on a missing type header; corrected
fixture evidence is retained separately. Runtime sources were unchanged by the
fixture correction. Required combined qualification and scoped sanitizers are
pending. PR132's accepted performance reference is at 0/5 subsequent merged
implementation checkpoints; this constructor batch adds no benchmark run.

## Baseline and focused observations

Six isolated unchanged-main controls use original builtins/fsm implementations,
compiled with the owning native flags and linked with the new probes. Core Exec
continues to exec after injected open refusal (assertion). Session network-name
refusal reaches incomplete data (assertion), cookie refusal faults with SIGSEGV,
network-list refusal and environment-publication refusal incorrectly report
success (assertions), and iceauth refusal follows prior credential publication
(assertion). These are local constructor/failure observations, not a demonstrated
remote compromise. Canonical baseline logs are retained separately.

The candidate passes 12 focused entries on Linux ARM64 GCC and 12 on FreeBSD
ARM64: core Exec's seven observations, seven separate session constructor cases
with successful retry after each refusal, and compatibility/session/Buttons/
Script real-X integration. Eight binary identities per target bind retained
results. The initial missing-header compile is preserved, not counted as a pass.

Linux guest capacity initially failed the unchanged 1-GiB owner guard. Four
completed duplicate checkpoint captures were retired only after full canonical
host gzip/CRC and exact native payload/hash comparison (941665141 duplicate
bytes). Canonical host archives, source/build/stage trees and VM disks remain.
