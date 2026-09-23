# MercaHash synchronization record

MercaMiner uses the authoritative MercaHash V1 implementation from
Mercatura Core rather than an independently re-created implementation.

Initial synchronization:

- Mercatura Core commit: `a5ec035`
- Mercatura Core tag: `v0.1-phase13`
- MercaHash production entry point: `mercahash::HashV1()`
- Canonical input size: 80 bytes
- Output size: 32 bytes
- Scratchpad size: 128 MiB
- Production mixing steps: 131,072
- Binding passes: 1

The files under `src/crypto/` and their minimal portability dependencies
were initially copied directly from the Mercatura Core source tree.

MercaMiner production mining code must use `HashV1()` and must not expose
benchmark-only MercaHash workload parameters as mining configuration.

Any future MercaHash synchronization must:

1. record the exact Mercatura Core source revision;
2. preserve applicable copyright and license notices;
3. run the permanent known-answer vectors;
4. verify canonical 80-byte block-header hashing against Mercatura Core;
5. pass `git diff --check` before commit.
