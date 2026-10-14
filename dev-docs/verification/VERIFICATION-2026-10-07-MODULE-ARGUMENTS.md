# Module argument growth and shared-library cleanup

Status: focused Linux normal/scoped sanitizer and native FreeBSD/OpenBSD ARM64
checks pass. This source is outside the frozen typed-schema checkpoint and will
join the subsequent descriptor-width work in one combined batch. Existing FVWM
module commands and configuration text remain usable; no library or limit is added.

Module argument storage now grows geometrically instead of reallocating for every
optional token. Its count/capacity use size_t, and doubling is checked against the
existing allocation helper's representable pointer count. The fallback retains
that helper's existing allocation-failure policy. Parsing, token ownership, alias
recognition and child invocation stay with the existing constructor.

Actual Module/ModuleListenOnly controls cover 0, 1, 7, 8, 9, 31, 64, 127 and 256
optional tokens in each mode, including quoted spaces and a first alias candidate.
The fork boundary compares exact token text/order and the final NULL, observes
storage growth, and checks refusal leaves the descriptor bitmap and active list
unchanged. Existing repeated refusal and successful real child retry remain
covered. At 256 tokens the prior source grows 256 times; the repaired source grows
six times in both modes. Normal/scoped prior-source controls preserve all eighteen
observations. This is an operation-count result, not a physical latency claim.

Two legacy implementation inclusions in module_list.c and icons.c now use their
existing System.h/Rectangles.h interfaces and link the already-built library
implementations. No shared implementation is copied or changed by this cleanup.
Affected full compilation emits no new normal-profile warnings; existing icon
redraw pixels and module/core regression checks remain covered.

Final Linux normal/scoped Clang ASan/UBSan profiles each pass eleven pure and
twenty-four actual-core checks. Pure units enable leak checking; core/startup
controls retain the existing leak-disabled scope. Both native BSD ARM64 overlays
compile the seven affected iterator/core/icon owners and pass the runtime and
expanded constructor controls. Four full archives validate payload hashes/modes
and compressed EOF: twelve Linux focused, fifty per BSD overlay and sixteen
prior-source files. Final gates needed no source repair or deadline extension.

Frozen source: 1345 files, four owned changes from typed-schema source1345.
Archive SHA-256:
`7f60cb46bad1e70fde9a686603d29ca15ed89aa1ae1ab965b284aa0e17aba2ee`.
Evidence: `builddir/codex-2026-10-07/module-arguments-production`.
Combined qualification and native release CI/physical checks remain separate.

Combined local qualification now passes under descriptor1349: 1336 standard
entries/eight profiles, 120 staged identities, 132 default BSD X drivers,
optional 172/152 and 55 scoped checks (22 pure/33 core). Seven archives validate.
The [descriptor report](VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md) retains
the original OpenBSD display refusal and Linux AMD64 Xorg teardown failure with
three unchanged affected passes each. Native release CI/physical evidence remains
separate; this does not claim every initial run passed.
