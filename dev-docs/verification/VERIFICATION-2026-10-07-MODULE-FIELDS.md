# Structured module packet fields

Status: final focused Linux normal/scoped sanitizer and native FreeBSD/OpenBSD
ARM64 checks pass. This source is separate from frozen lifecycle qualification;
combined qualification remains pending. No configuration migration is needed.

The configure builder now uses one pure bounded field packer and aligned native
word storage. Capacity includes the existing header before each field is read.
Scalar values are copied into local native values before widening; raw fields
use byte copies, including fields ending between word boundaries. This removes
unaligned/alias-dependent stores and makes variadic pointer argument types
explicit. The caller refuses an incomplete packet instead of publishing a
partial configure record. Padding is initialized; no allocation or library is
added. Ordinary signed widening and native module ABI remain supported.

Permanent pure controls check odd-sized raw fields followed by signed int,
short and native-word values, exact capacity, one-byte overflow, huge lengths,
missing storage, empty fields and invalid header/capacity inputs. Guard words,
exact native bytes and zero padding are checked. The startup/core group includes
real configure traffic, lifetime, synchronization and backpressure controls;
a separate follow-up adds exact comparison with the shipped consumer structure.

Final normal and scoped Clang ASan/UBSan profiles each pass twelve pure and
twenty-two actual-core tests. Pure units enable leak checking; startup/core
controls retain the established leak-disabled scope. Native BSD ARM64 overlays
pass the new field unit, module runtime unit and packet fixture. Three complete
focused archives validate payload hashes, modes and compressed EOF.

Frozen source: 1346 files, six owned additions/changes from the 1343-file packet
source. Archive SHA-256: `33ce56577c8cb67fd970fa103e056123d8238a5f8cca81602eb62156d9ce9176`.
Evidence: `builddir/codex-2026-10-07/module-fields-production`.
The next configure-identity correction is separate from this frozen source.
Native Linux release sanitizer CI and physical/toolkit/compositor checks remain
unrun; local emulated checks do not establish those results.

Combined qualification update: this story is locally qualified in the frozen
1346-file [transport checkpoint](VERIFICATION-2026-10-07-MODULE-TRANSPORT.md):
1312 standard entries, 120 staged identities, 124 default BSD X drivers, optional
169/149 and 48 scoped sanitizer checks. Seven complete archives validate source
hashes/modes and EOF; native release CI and physical checks remain separate.
