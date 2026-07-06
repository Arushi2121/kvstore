/* store.c
 *
 * Hash table implementation of the key-value store, using separate chaining.
 *
 * Structure:
 *   - The store holds an array of "buckets".
 *   - Each bucket is the head of a linked list of Entry nodes (the "chain").
 *   - Each Entry holds one key, one value, and a pointer to the next Entry in
 *     its chain.
 *
 * When two keys hash to the same bucket, they sit in the same chain. When the
 * table gets too full (load factor high), we grow the bucket array and rehash.
 */
#include "store.h"

#include <stdlib.h>   /* malloc, free            */
#include <string.h>   /* strcmp, strlen, memcpy  */

/* --- Tunable constants ---------------------------------------------------- */

/* Starting number of buckets. A small power of two is convenient; we grow as
 * needed. 16 is a fine default — big enough to be useful, small enough that
 * our tests will actually trigger resizes. */
#define INITIAL_BUCKETS 16

/* Resize when items / buckets exceeds this. 0.75 is the classic threshold:
 * high enough to use memory efficiently, low enough to keep chains short. */
#define MAX_LOAD_FACTOR 0.75

/* --- Internal data structures --------------------------------------------- */

/* One key-value pair, plus a link to the next pair in the same bucket's chain.
 * This is the linked-list node. */
typedef struct Entry {
    char *key;            /* our own heap copy of the key   */
    char *value;          /* our own heap copy of the value */
    struct Entry *next;   /* next node in this bucket's chain, or NULL */
} Entry;

/* The store itself: an array of bucket heads, plus bookkeeping.
 * This is the body the header kept opaque. */
struct Store {
    Entry **buckets;      /* array of "num_buckets" pointers to Entry chains */
    size_t num_buckets;   /* how many buckets in the array                   */
    size_t num_items;     /* how many key-value pairs total (for load factor)*/
};

/* --- Small helpers -------------------------------------------------------- */

/* strdup is not in standard C11, so we roll our own: allocate a heap copy of a
 * string. Returns NULL if allocation fails. Every returned pointer must
 * eventually be free()d — that's the ownership contract. */
static char *dup_string(const char *s) {
    size_t len = strlen(s) + 1;      /* +1 for the terminating '\0' */
    char *copy = malloc(len);
    if (copy == NULL) {
        return NULL;                 /* out of memory */
    }
    memcpy(copy, s, len);
    return copy;
}

/* The hash function: FNV-1a. It grinds a string into a well-spread number.
 * We don't need cryptographic strength — just good, fast distribution so keys
 * scatter evenly across buckets. FNV-1a is a well-known, tiny, effective
 * choice: for each byte, XOR it in, then multiply by a magic prime. The exact
 * constants are the published FNV-1a 64-bit values. */
static unsigned long hash_string(const char *s) {
    unsigned long hash = 1469598103934665603UL;   /* FNV offset basis */
    while (*s) {
        hash ^= (unsigned char)(*s);              /* XOR in the next byte */
        hash *= 1099511628211UL;                  /* multiply by FNV prime */
        s++;
    }
    return hash;
}

/* Given a store and a key, return the index of the bucket that key belongs in.
 * hash % num_buckets maps the huge hash number down into a valid bucket index. */
static size_t bucket_index(Store *store, const char *key) {
    return hash_string(key) % store->num_buckets;
}

/* --- Resize --------------------------------------------------------------- */

/* Grow the bucket array to new_num_buckets and rehash every existing entry
 * into its new home. This is the classic "silently loses keys if you get it
 * wrong" path, so it is written carefully and tested hard.
 * Returns true on success, false if the new array couldn't be allocated (in
 * which case the store is left unchanged and still usable). */
static bool store_resize(Store *store, size_t new_num_buckets) {
    /* calloc zero-initializes, so every new bucket starts as NULL (empty chain).
     * That NULL-initialization matters: an uninitialized pointer would be
     * garbage and we'd crash walking it. */
    Entry **new_buckets = calloc(new_num_buckets, sizeof(Entry *));
    if (new_buckets == NULL) {
        return false;   /* allocation failed; keep the old table intact */
    }

    /* Walk every old bucket, and every entry in each old chain, and re-file it
     * into the new array based on its hash mod the NEW bucket count. We move
     * nodes (re-link them) rather than copying — cheaper and simpler. */
    for (size_t i = 0; i < store->num_buckets; i++) {
        Entry *entry = store->buckets[i];
        while (entry != NULL) {
            Entry *next = entry->next;   /* save next BEFORE we relink entry */

            /* compute this entry's new bucket in the resized array */
            size_t new_idx = hash_string(entry->key) % new_num_buckets;

            /* push entry onto the front of its new chain (front insertion is
             * O(1) and order within a chain doesn't matter) */
            entry->next = new_buckets[new_idx];
            new_buckets[new_idx] = entry;

            entry = next;                /* advance to the saved next */
        }
    }

    /* swap in the new array and free the old (just the array of heads — the
     * Entry nodes themselves were moved, not freed). */
    free(store->buckets);
    store->buckets = new_buckets;
    store->num_buckets = new_num_buckets;
    return true;
}

/* --- Public API ----------------------------------------------------------- */

Store *store_create(void) {
    Store *store = malloc(sizeof(Store));
    if (store == NULL) {
        return NULL;
    }
    store->buckets = calloc(INITIAL_BUCKETS, sizeof(Entry *));
    if (store->buckets == NULL) {
        free(store);              /* clean up partial allocation */
        return NULL;
    }
    store->num_buckets = INITIAL_BUCKETS;
    store->num_items = 0;
    return store;
}

void store_free(Store *store) {
    if (store == NULL) {
        return;                   /* freeing NULL is a harmless no-op */
    }
    /* free every entry in every chain, then the bucket array, then the store */
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
    free(store->buckets);
    free(store);
}

bool store_set(Store *store, const char *key, const char *value) {
    size_t idx = bucket_index(store, key);

    /* First, check if the key already exists in its chain — if so, overwrite
     * its value in place (no new node, item count unchanged). */
    for (Entry *e = store->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            char *new_value = dup_string(value);
            if (new_value == NULL) {
                return false;         /* OOM: leave existing value untouched */
            }
            free(e->value);           /* free the old value */
            e->value = new_value;     /* install the new one */
            return true;
        }
    }

    /* Key is new. Before inserting, check whether adding one more item would
     * push us over the load factor; if so, grow first. We check BEFORE insert
     * so chains stay short even for this insertion. */
    if ((double)(store->num_items + 1) / (double)store->num_buckets > MAX_LOAD_FACTOR) {
        /* double the buckets; ignore failure — if resize fails we can still
         * insert into the current table, just with a longer chain. Correctness
         * is preserved either way. */
        store_resize(store, store->num_buckets * 2);
        idx = bucket_index(store, key);   /* recompute: bucket count changed */
    }

    /* Build the new entry with its own copies of key and value. */
    Entry *entry = malloc(sizeof(Entry));
    if (entry == NULL) {
        return false;
    }
    entry->key = dup_string(key);
    entry->value = dup_string(value);
    if (entry->key == NULL || entry->value == NULL) {
        /* partial allocation failure — free whatever succeeded, report OOM */
        free(entry->key);
        free(entry->value);
        free(entry);
        return false;
    }

    /* front-insert into the bucket's chain (O(1)) */
    entry->next = store->buckets[idx];
    store->buckets[idx] = entry;
    store->num_items++;
    return true;
}

const char *store_get(Store *store, const char *key) {
    size_t idx = bucket_index(store, key);
    for (Entry *e = store->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            return e->value;          /* found */
        }
    }
    return NULL;                      /* not present */
}

bool store_delete(Store *store, const char *key) {
    size_t idx = bucket_index(store, key);

    /* We need to unlink a node from a singly linked list, which means tracking
     * the PREVIOUS node so we can bridge over the one we remove. "prev == NULL"
     * means the node we're looking at is the head of the chain. */
    Entry *prev = NULL;
    Entry *e = store->buckets[idx];
    while (e != NULL) {
        if (strcmp(e->key, key) == 0) {
            if (prev == NULL) {
                store->buckets[idx] = e->next;   /* removing the head */
            } else {
                prev->next = e->next;            /* bridge over e */
            }
            free(e->key);
            free(e->value);
            free(e);
            store->num_items--;
            return true;
        }
        prev = e;
        e = e->next;
    }
    return false;                     /* key wasn't there */
}

size_t store_size(Store *store) {
    return store->num_items;
}
