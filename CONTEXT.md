# CONTEXT — Architectural Constitution

Read this file before writing or modifying any code in this repo. It defines
what this project is and the rules that keep it disciplined. When directing
Cursor, reference @CONTEXT.md so these rules are always in scope.

## What this is

A persistent key-value store written in C, built from scratch. It supports
GET / SET / DELETE / TTL, persists to disk via a Write-Ahead Log, and survives
process crashes by replaying that log on startup. Exposed over a raw TCP socket
with a simple binary protocol.

The goal is not to compete with Redis. The goal is a correct, durable store
that is fully understood and defensible — every design choice has a reason the
author can explain.

## The rules that matter most

1. No layer is "done" until its verifier (an automated test) passes. Not "it
   compiles" — the test proves the property holds.
2. Every feature ships with an ADVERSARIAL test, not just a happy-path test.
   We try to break our own code: random inputs, crashes at bad moments,
   corrupt files.
3. The oracle is the correctness backbone. Core operations are tested against
   a known-correct reference implementation across thousands of randomized
   histories, not a handful of hand-written cases.
4. AddressSanitizer (ASan) is ON in every test build. A clean ASan run is a
   merge gate. A memory error is a failing test, full stop.

## C discipline

- Every malloc has exactly one matching free. No leaks.
- Compiler warnings are errors in spirit: -Wall -Wextra stay clean.
- All test/debug builds run under -fsanitize=address.
- Untrusted input (files read on recovery, bytes off the socket) is parsed
  defensively — length checks before reads, no assumptions about well-formed data.

## Scope fence (out of scope — do not build)

- Replication or clustering. This is single-node.
- MVCC / transactions / multi-key atomicity beyond single operations.
- Authentication, TLS, access control.
- Any feature not required by the current layer. If it's not needed now, it
  doesn't get built now.

## Architecture

Decided incrementally as layers are built. Do not invent these ahead of time.

- Module boundaries: TBD — decided in Layer 1.
- WAL record format: TBD — decided in Layer 3.
- Durability contract (exact crash guarantee): TBD — decided in Layer 3 / 7.
- TTL expiry: dual mechanism. Lazy expiry in store_get guarantees an expired
  key is never returned. Active expiry (store_expire_cycle) sweeps a min-heap
  of expiry times to reclaim memory from expired-but-untouched keys. The store
  entry is the source of truth; heap entries are hints and may be stale, so the
  expire cycle re-checks the real entry before deleting. Time comes from an
  injectable clock (real in prod, mock in tests) for deterministic testing.
- Hash collisions: separate chaining (linked list per bucket). Resize doubles
  buckets at load factor 0.75 and rehashes all entries.

## How to direct Cursor in this project

When asking Cursor to implement something, always:
1. Name the file the code should live in.
2. State the function's input and output types.
3. State what it MAY and MAY NOT do / import.
4. Reference @CONTEXT.md.

Vague prompts ("add the store") let Cursor take the shortest path and violate
the discipline above. Be specific.

## Build

- `make test`  — build the test binary (ASan on) and run all tests.
- `make clean` — remove build output.
- Default compiler is clang; override with `make CC=gcc test`.
