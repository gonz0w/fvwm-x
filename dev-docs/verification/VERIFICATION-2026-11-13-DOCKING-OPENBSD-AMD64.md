# OpenBSD AMD64 staged docking qualification

For [#222](https://github.com/gonz0w/fvwm-x/issues/222), the focused run
used canonical source `475deadec432fbd9cf2c93aeebc34ae845763dee` on
OpenBSD 8.0 AMD64 under QEMU TCG. Earlier Ubuntu, FreeBSD, ARM64 and theme
results remain applicable. The full suite was outside this checkpoint.

The first attempt rejected three libpng headers linked into the same
prefix. The correction admitted those exact links. The retained compiler,
installed file digests, vendor ABI and runtime loader checks passed in
the guest. The run reused the qualified PNG build and its existing
36-test receipt.

The corrected run completed all 371 Ninja steps for 36 install targets,
staged the product privately and verified the staged core version and
controller and worker identity. Default Xenocara started at 1920×1200
with the documented disposable AMD64 fixture and rejected a client
without the authentication cookie.

The installed docking scene stopped at application catalog setup.
The desktop controller logged two worker deadline failures, and the
fixture timed out waiting for the initial catalog. The five docking
cases did not reach their assertions. Preserve the staged build evidence
and diagnose the worker startup before repeating the focused X scene.
No screenshots were taken.

The initial plan was
`d56a8e7747d2f4b4ed7d3e31e4992543d076e8d659c21f892c625268b11128ef`.
The corrected plan was
`eb276d0cad2c0d988af8574bb584edfe10e88b820d34883606b6305ab15551ca`.
The corrected archive SHA-256 is
`060536760cdab0966b3756441659360c18e0d00d85ffc83a099d8c6cd49ab966`.
Its runner status SHA-256 is
`9013b4775f4225ba419b5974c842fd80368bb786ed05718237e8e3fc9e5f4651`.
The runner stopped QEMU with exit status 0.

Physical qualification still needs a real dock and two differently scaled
displays to check unplug/replug during preview, connector identity after
restart, and panel, window and pointer reachability.

## Direct worker continuation

PR385 ran commit `6ee84269d7a50d6dd06e9c42dcca07b4b73bc10b` with plan
`1287847a6981176b2a706683662aeb574da347aa64522a781612f489e1819bb4`.
The docking scene starts the staged worker directly. It retains the real
catalog publication check and the original docking assertions.

The emulated OpenBSD AMD64 guest validated the retained PNG build, built
and staged the install targets, and started authenticated default Xenocara.
The server refused a client without its cookie. All five installed docking
cases passed, covering private profile publication, read-only preview,
stale workspace refusal, preservation of a newer manual window move, and
workspace, panel and window placement restore. No screenshots were taken.
The guest command and QEMU process each returned status 0.

The archive SHA-256 is
`927aede4b7e26bdcb12f182948b6920c6e9d21d1aaa342b7a4f4059f161016d1`.
The status SHA-256 is
`23a2cb3863b33ad8c1d7056ca6586dedc56ef7fac971750cca1e8a3527fb6cca`.
Physical docking remains open.
