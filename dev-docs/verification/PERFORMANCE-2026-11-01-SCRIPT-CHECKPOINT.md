# Script checkpoint core observation — 2026-11-01

The due comparison used existing PR305 and PR325 core/MFL binaries in the same
owned Linux ARM64 GCC debugoptimized Podman/AppleHV lab. All command and timer
median ranges overlap. No causal speed improvement or regression is established.
The [raw comparison and all six sample records](PERFORMANCE-2026-11-01-SCRIPT-CHECKPOINT.json)
include source/profile/binary identities and the original measurement receipt.

Three alternating pairs used owned Xvfb 1280×720×24, 128 command/timer samples
per case and 10,000 requested title/resize operations per second. The complete
observation took about 32 seconds without rebuilding or booting another lab.
Both sides use their recorded MFL binary; the driver identities differ. It
measures core command/timer behavior, not Script, paste, the whole desktop or
physical performance. The later TextField correction does not affect these core
and MFL binaries and receives separate functional evidence.

| Measurement | PR305 | PR325 |
| --- | ---: | ---: |
| Idle command median ms (range) | 0.331 (0.327–0.336) | 0.349 (0.231–0.354) |
| Title command median ms (range) | 0.312 (0.243–0.355) | 0.305 (0.299–0.325) |
| Resize command median ms (range) | 0.270 (0.161–0.290) | 0.285 (0.268–0.288) |
| Idle 10 ms timer median ms (range) | 13.968 (13.782–14.593) | 13.781 (13.631–14.278) |
| Title 10 ms timer median ms (range) | 12.214 (12.154–12.264) | 12.131 (12.107–12.323) |
| Resize 10 ms timer median ms (range) | 12.467 (12.457–12.608) | 12.505 (12.400–12.516) |
| Median per-run peak core RSS KiB | 7,488 | 7,468 |
| Unstripped core file bytes | 5,689,360 | 5,832,128 |
| Core text/data/bss bytes | 1,156,662 / 36,484 / 306,336 | 1,167,928 / 36,580 / 306,768 |

Peak RSS ranges touch at 7,480 KiB; the 20 KiB median difference supports no
memory improvement claim. Core file size rises 142,768 bytes, including debug
metadata; text rises 11,266 bytes, data 96 and bss 432. File size is not resident
memory. CPU uses coarse tick sampling. Achieved title/resize rates are roughly
9,986–9,995 operations/s. There was one observation, with no failed timing retry.

Reference PR305 is `8402e098c0fe3fcab8b14f2ff9c52346f7ff7d9b`, candidate
PR325 is `132e52e62e7d83603f56fa09632a1cb73ad389ea`. Core hashes are
`d66a52661b4507fa5084241ae540438e6948d0ae70acd12a622212852cbc7a0b` and
`6c03886d99a47b4811535934a5b4f268ae4dd289fbfddbc6e60d6a2d3a3fbac0`.
MFL hashes are `ea499dcffe250ef913ec6ebdce7e3dd13ffb5453bf00041c494649467dbd9dd0`
and `3033aaa23933405db7f0baacee91b9718b2419697e9ef7791b3275a196ba5810`.
Keep this as the next core observation reference; no standalone performance run
is required for each subsequent implementation PR.
