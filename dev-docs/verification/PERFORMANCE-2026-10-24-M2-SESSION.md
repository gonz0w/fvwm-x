# Installed resident-session observation

Three alternating pairs compare PR194's preserved Python resident and qualified
Linux ARM64 worker with the current Go owner. Both use the same current native
core/modules/data, installed configuration, one application fixture and nested X.
The separate Python-free namespace is not used for this comparison.
[Raw observations and identities](PERFORMANCE-2026-10-24-M2-SESSION.json).

| Median observation | PR194 Python resident | Current Go resident |
| --- | ---: | ---: |
| Core launch to settings/bar ready | 192.411 ms | 114.954 ms |
| Retained process-tree RSS sum | 45472 KiB | 27588 KiB |
| Sampled idle tree-RSS peak | 45472 KiB | 27588 KiB |
| XTest key to core Echo | 0.303 ms | 0.311 ms |

These three samples show lower observed startup and RSS with the Go owner;
input timings overlap. RSS sums include shared pages more than once and are not
physical memory/PSS. The sampled idle peak and retained-process HWM are not
startup or whole-session peaks. The one-shot helper's transient usage can be
missed. Both profiles show zero retained CPU-tick delta during the short idle
window; that is coarse data, not zero CPU usage. Thread-leader context-switch
counts cannot establish total Go-thread activity or scheduler wakeups. The
scheduler tracing endpoint is absent in this container; wakeups remain uncollected.

Startup includes observer polling, not login or display-server startup. Synthetic
key-to-core Echo does not measure physical devices, widget paint or loaded apps.
This ARM64 guest observation supplies no native-CPU/hardware, GC or general
performance guarantee. Tuning remains paused.

The first reference attempt accidentally selects a host Mach-O worker and fails
before readiness; it is not a performance sample. Recover the preserved installed
Linux worker and match its hash to PR194's original qualified receipt before
sampling. No new reference build or old port reimplementation is used.

This is a separate M2 session acceptance observation. PR194's independent metadata
comparison reference stays 2/5 after PR198, 3/5 on this merge; no periodic scan
comparison is due or reset here.
