# Design Decisions (ADR Log)

A running record of significant design decisions, the alternatives considered,
and the tradeoffs accepted. One entry per decision, added as the project grows.
This is the "why" behind the code — read it to understand or defend any choice.

---

## ADR-001: Separate chaining for hash collisions

**Decision:** The hash table resolves collisions with separate chaining (a
linked list of entries per bucket).

**Alternatives considered:** Open addressing (linear/quadratic probing), where
collisions are resolved by probing to the next open slot.

**Tradeoff accepted:** Open addressing is more memory- and cache-efficient and
is common in high-performance tables. We chose chaining because deletion is
clean and simple (unlink a node) whereas open-addressing deletion requires
tombstones or backward-shifting and is easy to get subtly wrong. For a
correctness-first, defensible implementation, chaining's clarity wins over open
addressing's performance. A future optimization could revisit this.

---

## ADR-002: FNV-1a hash function

**Decision:** Keys are hashed with FNV-1a (64-bit).

**Alternatives considered:** A cryptographic hash (overkill, slow); a simpler
hash like djb2; or a library hash.

**Tradeoff accepted:** We do not need cryptographic strength, only good, fast
distribution. FNV-1a is tiny, dependency-free, well-understood, and distributes
well for string keys. We give up the marginally better distribution of more
complex hashes for simplicity and the ability to explain every line.

---

## ADR-003: Dynamic resizing at load factor 0.75

**Decision:** The table doubles its bucket count and rehashes all entries when
items/buckets exceeds 0.75.

**Alternatives considered:** A fixed-size table (simpler, but chains grow
unbounded and lookups degrade to O(n)); a different threshold or growth factor.

**Tradeoff accepted:** Resizing adds complexity (the rehash path is a classic
source of key-loss bugs, which we cover with a dedicated test) and an occasional
O(n) rehash cost. We accept that for amortized O(1) operations that stay fast as
the store grows. 0.75 is the standard space/speed balance.

---

## ADR-004: TTL via dual lazy + active expiry with a min-heap

**Decision:** TTL uses two mechanisms. Lazy expiry (in store_get) removes and
hides an expired key on access, guaranteeing correctness. Active expiry
(store_expire_cycle) sweeps a min-heap ordered by expiry time to reclaim memory
from expired-but-untouched keys.

**Alternatives considered:** (a) Lazy-only — simple, but expired-but-untouched
keys leak memory forever. (b) Scanning all keys each sweep — correct but O(n)
per sweep, wasteful. (c) A fully indexed heap kept perfectly in sync with the
store on every overwrite/delete.

**Tradeoff accepted:** We rejected the fully-synced heap (c) because keeping a
heap in sync with arbitrary updates is complex and heaps do not support
efficient arbitrary-entry lookup. Instead the store entry is the source of
truth and heap entries are treated as hints that may be STALE: when the active
cycle pops a reminder, it re-checks the real entry's current expiry and skips it
if the key was deleted or reset. We accept occasional stale heap entries (cleaned
up when popped) in exchange for simple, correct code. Covered by a dedicated
stale-entry test.

---

## ADR-005: Injectable clock (dependency injection) for time

**Decision:** The store never reads the OS clock directly. It holds a Clock
(a function pointer + state) and asks it for the time. Production uses a real
monotonic clock; tests use a mock clock whose time is set by hand.

**Alternatives considered:** Calling clock_gettime() directly wherever time is
needed (less code up front).

**Tradeoff accepted:** Direct OS-time calls would make expiry logic nearly
untestable — tests would need real sleeps, making them slow and flaky, and
edge cases (far-future expiry) untestable. Injecting the clock costs a small
amount of structure but makes all time-dependent logic deterministically
testable at any timescale, instantly. For a correctness-first project this is
clearly worth it.
