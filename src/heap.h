/* heap.h
 *
 * A min-heap of expiry entries, ordered by expiry timestamp (earliest on top).
 * Used by the store for active TTL expiry: "what expires soonest, and is it
 * due yet?" answered fast.
 *
 * Opaque type, same discipline as Store: callers use it through functions only.
 */
#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* One entry in the heap: a key and the time it expires. We store a COPY of the
 * key string so the heap owns its own memory (same ownership rule as the store). */
typedef struct {
    char *key;              /* heap-owned copy of the key */
    uint64_t expire_at;     /* absolute time (same unit as the clock) when it dies */
} HeapEntry;

typedef struct Heap Heap;

/* Create an empty heap. NULL on allocation failure. Caller must heap_free it. */
Heap *heap_create(void);

/* Free the heap and every key copy it owns. */
void heap_free(Heap *heap);

/* Insert a (key, expire_at) pair. Copies the key. Returns false on OOM. */
bool heap_push(Heap *heap, const char *key, uint64_t expire_at);

/* Peek at the earliest expiry time WITHOUT removing it. Returns false if empty;
 * on success writes the soonest expire_at into *out_expire_at. */
bool heap_peek_min(Heap *heap, uint64_t *out_expire_at);

/* Remove and return the earliest-expiring entry. Returns false if empty.
 * On success, *out_key is set to a heap-owned string that OWNERSHIP TRANSFERS
 * to the caller — the caller must free() it. *out_expire_at gets its expiry. */
bool heap_pop_min(Heap *heap, char **out_key, uint64_t *out_expire_at);

/* How many entries are in the heap. */
size_t heap_size(Heap *heap);

#endif /* HEAP_H */
