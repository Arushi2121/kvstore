/* store.h
 *
 * Public interface for the key-value store. This is the "menu": callers see
 * WHAT the store can do, not HOW it does it. The how lives in store.c.
 *
 * Design note (recorded here and in CONTEXT.md): the Store type is "opaque" —
 * callers get a pointer to it but cannot see its fields. This is deliberate
 * information hiding: the internals (hash table, buckets, resizing) can change
 * completely without any caller needing to change. Callers only ever touch the
 * store through the functions below. Like a vending machine: you use the
 * buttons, you don't reach into the mechanism.
 */
#ifndef STORE_H
#define STORE_H

#include <stddef.h>   /* for size_t */
#include <stdbool.h>  /* for bool   */

/* Opaque type: callers hold a "Store *" but the struct's guts are defined only
 * in store.c. Forward-declaring it here (without the body) is what makes it
 * opaque. */
typedef struct Store Store;

/* Create a new, empty store. Returns a pointer to it, or NULL if memory
 * allocation failed. The caller owns this and must eventually call
 * store_free() on it. */
Store *store_create(void);

/* Destroy a store and free ALL memory it owns (every key, every value, every
 * bucket). After this call the pointer is invalid and must not be used. */
void store_free(Store *store);

/* SET: associate key with value (copies both — the store keeps its own copies,
 * so the caller's strings can be freed or changed afterward). If the key
 * already exists, its value is overwritten. Returns true on success, false if
 * memory allocation failed. */
bool store_set(Store *store, const char *key, const char *value);

/* GET: look up key. Returns a pointer to the stored value string if found, or
 * NULL if the key is not present. The returned pointer is owned by the store —
 * the caller must NOT free it, and it stays valid only until the next
 * operation that could modify or free that entry. */
const char *store_get(Store *store, const char *key);

/* DELETE: remove key (and free its stored key+value). Returns true if the key
 * existed and was removed, false if it wasn't present. */
bool store_delete(Store *store, const char *key);

/* Number of key-value pairs currently stored. Useful for tests and, later,
 * the dashboard. */
size_t store_size(Store *store);

#endif /* STORE_H */
