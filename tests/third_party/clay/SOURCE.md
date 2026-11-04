# Private Clay evaluation source

Only the default-disabled, non-installed layout comparison targets use these
unchanged files. This is not production dependency adoption.

Upstream: https://github.com/nicbarker/clay
Commit: `e6cc36941ab2af5d81107617039d6f527a1c660b`
Header version: `0.14`. License: zlib, retained in `LICENSE.md` and the header.

| File | Bytes | Git blob | SHA-256 |
| --- | ---: | --- | --- |
| clay.h | 303701 | 7d9cf4cc7a918d1bdd766867c180851c949a3310 | 54aef6d5f96b14e296bebb6db4a755543e98fd7e6c188411afb2f22a4b4f7728 |
| LICENSE.md | 891 | 2968c61bcdc6ecbef470e02edd45b47aba1d8c44 | 5dd30f9f52093076b7574458f5efb8242fe6cbbd4669d4a2914e7616810d0df8 |

Review source, license, compiler/API changes, arena bounds and all comparison
oracles before changing this pin. Do not reformat vendored source. Complete
profile/support/runtime/installed-cost evidence and the adopt/defer/reject
decision remain under LAYOUT-01 #212; upstream benchmarks are not FVWM-X results.
