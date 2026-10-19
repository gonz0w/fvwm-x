# Style command ownership checkpoint

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Base: merged PR #116, `f9325ee41665430d1580f01cad6426a818a81910`.
Three improvements implemented; seven focused entries and 41 real-client cases pass. One combined checkpoint follows.

## Three related improvements

1. UseStyle borrows existing rules but owns its parsed selector. Release its
   temporary fields on success, miss and parse refusal; preserve copied style
   resources and option order. An empty list is a diagnosed miss.
2. DestroyStyle borrows its parsed selector for synchronous removal and window
   invalidation. Release temporary fields after removal/miss/refusal; retained
   rule/index/window ownership remains with its existing owner.
3. Style/FocusStyle/WindowStyle schedule a global pass only after admitting their
   selector/input. Refused commands release temporary storage and preserve any
   already pending update without creating a new pass. Accepted option parsing,
   including partial-option behavior, stays unchanged.

Actual-core allocation observations, style copying/removal, command order,
ordinary pending updates and X frame/client geometry will be checked together.
No persistent selector/window cache, format migration or moved application/grab
barrier is introduced. Existing warm native builds and known CI/SVG required
omissions/install gap remain; VESA #93 and native-CPU/hardware/release coverage
stay separate.

## Baseline and diagnostic preparation

Unchanged production with the completed allocation fixture retains nine selected
selector fields across seven observations: three UseStyle calls and six fields
from found/missing DestroyStyle calls. Three safely refused input cases each
still schedule one server grab. Style copies survive source removal and actual
frame/client geometry plus _NET_FRAME_EXTENTS retain their expected values.

The original bare Style/FocusStyle command sequence fails under scoped UBSan at
style.c's compatibility-name comparison (null name); a separate direct NULL
handler preparation reaches CreateFlagString's nonnull input precondition.
The final parser refuses missing input/token before constructing a compatibility
ID, initializes output state and reports a missing selector. Accepted selectors
and partial-option behavior stay unchanged. The fixture separately exercises
ordinary command-engine empty arguments and direct missing arguments after the
fix. Unterminated-selector controls supply unchanged baseline scheduling costs.

Earlier fixture preparations incorrectly treated comma-separated UseStyle input
as one option, then tried a whole-option quote which changes tokenization. The
completed fixture uses existing single compatibility/explicit-name forms and
multi-field DestroyStyle. All original failures, inputs, sanitizer diagnostics
and completed baseline are retained in the ignored owner directory; no broad
suite has run during this preparation.

## Focused candidate checkpoint

Seven Linux development-profile entries and 41 retained real-client cases pass.
The same seven selected ownership observations retain zero selector fields;
three safely refused cases perform zero server grabs. Ordinary missing-selector
Style/FocusStyle commands and direct missing inputs return cleanly, and a valid
subsequent style/removal still applies exact frame/client geometry. Existing
style matching, filter, lookup/profile, window-update and colorset/context
regressions pass. These are selected operation counts, not a universal timing
claim. One combined checkpoint follows on the frozen candidate.
