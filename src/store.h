/* store.h
 *
 * Public interface for the key-value store. This is the "menu": callers see
 * WHAT the store can do, not HOW it does it. The how lives in store.c.
 *
 * The Store type is opaque: callers hold a "Store *" but cannot see its fields,
 * so internals (hash table, TTL heap, clock) can change without breaking callers.
 */
 #ifndef STORE_H
 #define STORE_H
 
 #include <stddef.h>   /* size_t */
 #include <stdbool.h>  /* bool   */
 #include <stdint.h>   /* uint64_t */
 #include "clock.h"    /* injectable clock for TTL timing */
 
 typedef struct Store Store;
 
 /* Create a new, empty store using the given clock for TTL timing. The clock is
  * borrowed (not owned) — the caller keeps it alive for the store's lifetime.
  * Pass clock_real() in production, clock_mock(...) in tests. NULL on OOM. */
 Store *store_create(Clock clock);
 
 /* Destroy a store and free ALL memory it owns. */
 void store_free(Store *store);
 
 /* SET: associate key with value (copies both). Overwrites if key exists, and
  * clears any prior TTL (a plain SET makes the key immortal). false on OOM. */
 bool store_set(Store *store, const char *key, const char *value);
 
 /* SET with a time-to-live: the key auto-expires ttl_ms milliseconds from now
  * (per the store's clock). false on OOM. */
 bool store_set_ttl(Store *store, const char *key, const char *value,
                    uint64_t ttl_ms);
 
 /* GET: returns the stored value string if present and not expired, else NULL.
  * Performs lazy expiry: an expired key is removed and reported as absent. The
  * returned pointer is owned by the store; do not free it. */
 const char *store_get(Store *store, const char *key);
 
 /* DELETE: remove key. Returns true if it existed, false otherwise. */
 bool store_delete(Store *store, const char *key);
 
 /* Actively sweep and remove keys expired as of now. Returns count expired.
  * Lazy expiry in store_get already guarantees expired keys are never returned;
  * this reclaims memory from expired-but-untouched keys. */
 size_t store_expire_cycle(Store *store);
 
 /* Number of key-value pairs currently stored (may include expired-but-not-yet-
  * swept keys until a get or expire cycle removes them). */
 size_t store_size(Store *store);
 
 #endif /* STORE_H */