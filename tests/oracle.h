/* oracle.h + oracle.c combined as a header-only helper for tests.
 *
 * The oracle is the "known-correct reference" the real store is tested against.
 * It is deliberately DUMB: a flat array of key-value pairs with linear search.
 * O(n) slow, and we do not care — its only job is to be so simple it is
 * OBVIOUSLY correct, so we can trust it to judge the real (fast, complex) store.
 *
 * It exposes the same conceptual operations as the store: set, get, delete,
 * size. If the real store ever disagrees with the oracle, the real store has
 * a bug (the oracle is too simple to be wrong).
 *
 * Header-only (everything "static") to keep the test build trivial: no extra
 * compile unit, it's just #included into the test file.
 */
#ifndef ORACLE_H
#define ORACLE_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ORACLE_MAX 100000   /* plenty for our test key pool */

/* strdup is not in standard C11 and using it under -std=c11 causes an implicit
 * declaration (which truncates its pointer return to int -> crash). So the
 * oracle uses its own standard-C copy helper, same as the store does. */
static char *oracle_dup (const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy != NULL) memcpy(copy, s, len);
    return copy;
}

typedef struct {
    char *keys[ORACLE_MAX];
    char *values[ORACLE_MAX];
    size_t count;
} Oracle;

static Oracle *oracle_create(void) {
    Oracle *o = calloc(1, sizeof(Oracle));  /* calloc: count=0, all ptrs NULL */
    return o;
}

static void oracle_free(Oracle *o) {
    if (o == NULL) return;
    for (size_t i = 0; i < o->count; i++) {
        free(o->keys[i]);
        free(o->values[i]);
    }
    free(o);
}

/* linear scan for a key; returns its index or -1 if absent */
static long oracle_find(Oracle *o, const char *key) {
    for (size_t i = 0; i < o->count; i++) {
        if (strcmp(o->keys[i], key) == 0) {
            return (long)i;
        }
    }
    return -1;
}

static void oracle_set(Oracle *o, const char *key, const char *value) {
    long idx = oracle_find(o, key);
    if (idx >= 0) {
        /* overwrite existing value */
        free(o->values[idx]);
        o->values[idx] = oracle_dup (value);
        return;
    }
    /* append new pair */
    o->keys[o->count] = oracle_dup (key);
    o->values[o->count] =oracle_dup (value);
    o->count++;
}

static const char *oracle_get(Oracle *o, const char *key) {
    long idx = oracle_find(o, key);
    return idx >= 0 ? o->values[idx] : NULL;
}

static bool oracle_delete(Oracle *o, const char *key) {
    long idx = oracle_find(o, key);
    if (idx < 0) return false;
    free(o->keys[idx]);
    free(o->values[idx]);
    /* fill the hole with the last element to keep the array dense */
    o->keys[idx] = o->keys[o->count - 1];
    o->values[idx] = o->values[o->count - 1];
    o->count--;
    return true;
}

static size_t oracle_size(Oracle *o) {
    return o->count;
}

#endif /* ORACLE_H */
