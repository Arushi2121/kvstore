/* store.c
 *
 * Hash table implementation (separate chaining) with TTL support.
 * TTL uses two mechanisms:
 *   - lazy expiry: store_get removes an expired key and reports it absent.
 *   - active expiry: store_expire_cycle sweeps the min-heap of expiry times.
 * The store's own entry (value + expire_at) is the source of truth; the heap
 * is only a hint about what to check and when (heap entries may be stale).
 */
 #include "store.h"
 #include "heap.h"
 
 #include <stdlib.h>
 #include <string.h>
 #include <stdint.h>
 
 #define INITIAL_BUCKETS 16
 #define MAX_LOAD_FACTOR 0.75
 
 /* Sentinel expire_at meaning "this key never expires" (plain SET, no TTL). */
 #define NEVER_EXPIRES UINT64_MAX
 
 typedef struct Entry {
     char *key;
     char *value;
     uint64_t expire_at;       /* NEVER_EXPIRES = immortal */
     struct Entry *next;
 } Entry;
 
 struct Store {
     Entry **buckets;
     size_t num_buckets;
     size_t num_items;
     Clock clock;              /* borrowed clock for TTL timing */
     Heap *expiry_heap;        /* keys with TTLs, ordered by expiry */
 };
 
 /* --- helpers -------------------------------------------------------------- */
 
 static char *dup_string(const char *s) {
     size_t len = strlen(s) + 1;
     char *copy = malloc(len);
     if (copy == NULL) return NULL;
     memcpy(copy, s, len);
     return copy;
 }
 
 static unsigned long hash_string(const char *s) {
     unsigned long hash = 1469598103934665603UL;
     while (*s) {
         hash ^= (unsigned char)(*s);
         hash *= 1099511628211UL;
         s++;
     }
     return hash;
 }
 
 static size_t bucket_index(Store *store, const char *key) {
     return hash_string(key) % store->num_buckets;
 }
 
 static bool entry_is_expired(Entry *e, uint64_t now) {
     return e->expire_at != NEVER_EXPIRES && e->expire_at <= now;
 }
 
 /* --- resize --------------------------------------------------------------- */
 
 static bool store_resize(Store *store, size_t new_num_buckets) {
     Entry **new_buckets = calloc(new_num_buckets, sizeof(Entry *));
     if (new_buckets == NULL) return false;
     for (size_t i = 0; i < store->num_buckets; i++) {
         Entry *entry = store->buckets[i];
         while (entry != NULL) {
             Entry *next = entry->next;
             size_t new_idx = hash_string(entry->key) % new_num_buckets;
             entry->next = new_buckets[new_idx];
             new_buckets[new_idx] = entry;
             entry = next;
         }
     }
     free(store->buckets);
     store->buckets = new_buckets;
     store->num_buckets = new_num_buckets;
     return true;
 }
 
 /* --- lifecycle ------------------------------------------------------------ */
 
 Store *store_create(Clock clock) {
     Store *store = malloc(sizeof(Store));
     if (store == NULL) return NULL;
     store->buckets = calloc(INITIAL_BUCKETS, sizeof(Entry *));
     if (store->buckets == NULL) { free(store); return NULL; }
     store->expiry_heap = heap_create();
     if (store->expiry_heap == NULL) { free(store->buckets); free(store); return NULL; }
     store->num_buckets = INITIAL_BUCKETS;
     store->num_items = 0;
     store->clock = clock;
     return store;
 }
 
 void store_free(Store *store) {
     if (store == NULL) return;
     for (size_t i = 0; i < store->num_buckets; i++) {
         Entry *entry = store->buckets[i];
         while (entry != NULL) {
             Entry *next = entry->next;
             free(entry->key);
             free(entry->value);
             free(entry);
             entry = next;
         }
     }
     heap_free(store->expiry_heap);
     free(store->buckets);
     free(store);
 }
 
 /* --- core operations ------------------------------------------------------ */
 
 bool store_set(Store *store, const char *key, const char *value) {
     size_t idx = bucket_index(store, key);
 
     for (Entry *e = store->buckets[idx]; e != NULL; e = e->next) {
         if (strcmp(e->key, key) == 0) {
             char *new_value = dup_string(value);
             if (new_value == NULL) return false;
             free(e->value);
             e->value = new_value;
             e->expire_at = NEVER_EXPIRES;   /* plain SET clears any prior TTL */
             return true;
         }
     }
 
     if ((double)(store->num_items + 1) / (double)store->num_buckets > MAX_LOAD_FACTOR) {
         store_resize(store, store->num_buckets * 2);
         idx = bucket_index(store, key);
     }
 
     Entry *entry = malloc(sizeof(Entry));
     if (entry == NULL) return false;
     entry->key = dup_string(key);
     entry->value = dup_string(value);
     if (entry->key == NULL || entry->value == NULL) {
         free(entry->key); free(entry->value); free(entry);
         return false;
     }
     entry->expire_at = NEVER_EXPIRES;       /* plain SET: immortal */
     entry->next = store->buckets[idx];
     store->buckets[idx] = entry;
     store->num_items++;
     return true;
 }
 
 bool store_set_ttl(Store *store, const char *key, const char *value,
                    uint64_t ttl_ms) {
     if (!store_set(store, key, value)) return false;
 
     uint64_t now = clock_now(&store->clock);
     uint64_t expire_at = now + ttl_ms;
 
     size_t idx = bucket_index(store, key);
     for (Entry *e = store->buckets[idx]; e != NULL; e = e->next) {
         if (strcmp(e->key, key) == 0) {
             e->expire_at = expire_at;
             heap_push(store->expiry_heap, key, expire_at);  /* reminder */
             return true;
         }
     }
     return true;
 }
 
 const char *store_get(Store *store, const char *key) {
     size_t idx = bucket_index(store, key);
     uint64_t now = clock_now(&store->clock);
 
     Entry *prev = NULL;
     for (Entry *e = store->buckets[idx]; e != NULL; prev = e, e = e->next) {
         if (strcmp(e->key, key) == 0) {
             if (entry_is_expired(e, now)) {
                 if (prev == NULL) store->buckets[idx] = e->next;
                 else prev->next = e->next;
                 free(e->key); free(e->value); free(e);
                 store->num_items--;
                 return NULL;
             }
             return e->value;
         }
     }
     return NULL;
 }
 
 bool store_delete(Store *store, const char *key) {
     size_t idx = bucket_index(store, key);
     Entry *prev = NULL;
     Entry *e = store->buckets[idx];
     while (e != NULL) {
         if (strcmp(e->key, key) == 0) {
             if (prev == NULL) store->buckets[idx] = e->next;
             else prev->next = e->next;
             free(e->key); free(e->value); free(e);
             store->num_items--;
             return true;
         }
         prev = e;
         e = e->next;
     }
     return false;
 }
 
 size_t store_expire_cycle(Store *store) {
     uint64_t now = clock_now(&store->clock);
     size_t expired_count = 0;
 
     uint64_t soonest;
     while (heap_peek_min(store->expiry_heap, &soonest) && soonest <= now) {
         char *heap_key;
         uint64_t heap_expire;
         heap_pop_min(store->expiry_heap, &heap_key, &heap_expire);
 
         size_t idx = bucket_index(store, heap_key);
         Entry *prev = NULL;
         for (Entry *e = store->buckets[idx]; e != NULL; prev = e, e = e->next) {
             if (strcmp(e->key, heap_key) == 0) {
                 if (entry_is_expired(e, now)) {
                     if (prev == NULL) store->buckets[idx] = e->next;
                     else prev->next = e->next;
                     free(e->key); free(e->value); free(e);
                     store->num_items--;
                     expired_count++;
                 }
                 break;
             }
         }
         free(heap_key);
     }
     return expired_count;
 }
 
 size_t store_size(Store *store) {
     return store->num_items;
 }