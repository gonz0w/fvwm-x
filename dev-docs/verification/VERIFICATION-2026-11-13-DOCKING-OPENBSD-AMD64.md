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
