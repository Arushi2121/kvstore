# kvstore

A persistent key-value store in C, built from scratch — with a Write-Ahead Log,
crash recovery, TTL expiry, and a raw-socket binary protocol.

**Status:** In active development. Built layer by layer, correctness-first.

## What it is

A from-scratch key-value store (a minimal Redis-lite) focused on durability and
provable correctness rather than feature count. GET / SET / DELETE / TTL, backed
by a Write-Ahead Log so data survives crashes.

## Guarantees

_Precise durability contract defined in Layer 3 / 7 — will state exactly what
survives a crash and under which mode._

## How it's tested

Correctness is proven, not assumed:

- **Oracle / property testing** — the store is checked against a known-correct
  reference across thousands of randomized operation sequences. _(Layer 1)_
- **Automated crash recovery** — the process is killed with `kill -9` at
  randomized points and must recover to a correct, consistent state, verified
  in CI. _(Layer 5)_
- **AddressSanitizer** — the full test suite runs clean under ASan on every
  commit.

## Build & run
make test        # build with AddressSanitizer and run all tests
make clean        # remove build artifacts

Requires a C compiler (clang or gcc), make, and a POSIX environment
(Linux, macOS, or WSL2 on Windows).

## Known limitations

Stated honestly and by design:

- Single-node only — no replication or clustering.
- No transactions / MVCC / multi-key atomicity.
- No authentication or transport security.

_Full limitations and "what I'd change at scale" documented as the project
matures._

## Roadmap

Built in verified layers: store core and oracle (L1) → TTL (L2) → Write-Ahead
Log (L3) → crash-recovery kill-test (L5) → protocol hardening, durability modes,
concurrency, compaction, benchmarks.
