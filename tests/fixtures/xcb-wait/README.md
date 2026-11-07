# Private XCB reply-wait fixture

This fixture patches an isolated extraction of libxcb 1.15 for the G13
comparison work under #211 and #212. The patcher checks the exact upstream
`src/xcb_in.c` and generated `src/Makefile.in` hashes before changing files.
It adds one standalone observer object to the private core library.

Run `python3 apply_private.py /absolute/private/libxcb-1.15` from this
fixture directory before configuring the separately owned source build.
The source generator inputs and dependency profile belong to the owning
reviewed job. Compile `probe.c` against that private build's `config.h`,
headers and `.libs` directory, with X11, XCB, dl and pthread. Supply the
absolute built `libxcb.so.1.1.0` path as the probe's sole argument and set
`DISPLAY` to the owning job's Xvfb. Compile `test_observer.c` with the same
config and observer source, with assertions enabled.

Preserve the release archive timestamps during a fresh extraction. Before
building the private core target, generate its protocol sources in a
separate Make invocation:

```sh
make -C /private/libxcb-1.15/src -j2 xproto.c bigreq.c xc_misc.c
```

Require each generated source and its corresponding header to exist and
be nonempty. Then build `libxcb.la` in a second invocation. The direct
library target bypasses the release Makefile's normal generated-source
ordering. The generation recipe belongs to the `.c` targets; it writes
both source and header. This sequence generated all six files and built the private library in the
Linux ARM64 continuation. The initial two build-driver failures remain in
the verification record.

The native probe uses six controls. An asynchronous request and retrieval
of an already-buffered reply require zero wait entries. Outstanding direct
XCB, Xlib and checked-void requests require one logical reply wait each.
A separate connection must remain outside the watched connection's scope.
The pinned Xlib retains its prior successful reply node. Its control
therefore requires two exact request records: the prior node retires without
a reply after one logical wait, and the current reply is already buffered
and requires no wait. This distinguishes reply contexts from logical waits.
The probe confirms that its API and Xlib dependency resolve the same
private XCB library. A controller owns a server grab and releases it after
an atomic acknowledgment from the actual reply-wait hook. A three-second
fallback always releases the grab and fails the control if acknowledgment
was absent. A 30-second process alarm bounds the full probe.

Observation covers one calling thread and one connection. A reply context
starts after output flush succeeds and finishes after the reader retires.
Each fixed record identifies a request sequence, wait-call count, partial
wait-call count and completion. Repeated failed polls increment wait calls
while the logical wait count increases once per reply context. Partial
waits count the `read_block` EAGAIN path while a matching reply context is
active. Such a read may be processing an earlier packet on that connection.
The counters therefore describe partial-read waits during reply processing.
They are separate from logical reply waits. Thread-local observation does
not establish whole-process multithread coverage or kernel sleep counts.

Nested scope or reply contexts, unmatched hooks, unfinished scopes and
record/counter overflow produce sticky invalid results. The observer
keeps at most 64 copied request records. It allocates no memory and emits
no per-hook output. Connection and optional atomic acknowledgment pointers
are retained only during the active scope and cleared at scope end.

The focused Python test accepts `XCB_WAIT_SOURCE_ARCHIVE` naming the
checksum-pinned 1.15 original archive. That input enables an actual patch
and reapplication-refusal check, in addition to the ordinary rejection
checks. Native partial-packet qualification remains pending until the
owning job supplies a controlled fragmented-reply case. The C unit covers
the partial counter's scoping, nested/unfinished states and record limit.
