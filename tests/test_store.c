/* test_store.c
 *
 * Layer 1 verifier. Two kinds of tests:
 *
 *  1. Targeted tests   — a few hand-written cases for basic sanity and for the
 *                        resize path specifically (the "silently loses keys"
 *                        danger zone).
 *  2. Oracle property  — the backbone: thousands of RANDOM operation sequences
 *     test                applied to BOTH the real store and the dumb oracle,
 *                        asserting they never diverge. This finds bugs in cases
 *                        no human would think to hand-write.
 */
#include "test_harness.h"
#include "oracle.h"
#include "store.h"

#include <stdlib.h>
#include <stdio.h>

/* ---- Targeted sanity tests ---------------------------------------------- */

static void test_basic_set_get(void) {
    Store *s = store_create();
    CHECK(s != NULL, "store_create returns non-NULL");

    CHECK(store_set(s, "name", "arushi"), "set name=arushi succeeds");
    const char *v = store_get(s, "name");
    CHECK(v != NULL && strcmp(v, "arushi") == 0, "get name returns arushi");

    CHECK(store_get(s, "missing") == NULL, "get missing key returns NULL");
    CHECK(store_size(s) == 1, "size is 1 after one set");

    store_free(s);
}


static void test_overwrite(void) {
    Store *s = store_create();
    store_set(s, "k", "v1");
    store_set(s, "k", "v2");                       /* overwrite */
    const char *v = store_get(s, "k");
    CHECK(v != NULL && strcmp(v, "v2") == 0, "overwrite updates value");
    CHECK(store_size(s) == 1, "overwrite does not increase size");
    store_free(s);
}

static void test_delete(void) {
    Store *s = store_create();
    store_set(s, "k", "v");
    CHECK(store_delete(s, "k") == true, "delete existing key returns true");
    CHECK(store_get(s, "k") == NULL, "deleted key is gone");
    CHECK(store_size(s) == 0, "size is 0 after delete");
    CHECK(store_delete(s, "k") == false, "delete missing key returns false");
    store_free(s);
}

/* The resize danger-zone test: insert enough keys to force MULTIPLE doublings
 * (16 -> 32 -> 64 -> 128 ...), then assert EVERY key still retrieves correctly.
 * A resize bug that drops keys shows up here. */
static void test_resize_keeps_all_keys(void) {
    Store *s = store_create();
    const int N = 5000;   /* far past several resize thresholds */

    char key[32], val[32];
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(val, sizeof(val), "val%d", i);
        store_set(s, key, val);
    }
    CHECK(store_size(s) == (size_t)N, "size equals N after N distinct inserts");

    bool all_present = true;
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(val, sizeof(val), "val%d", i);
        const char *got = store_get(s, key);
        if (got == NULL || strcmp(got, val) != 0) {
            all_present = false;
            break;
        }
    }
    CHECK(all_present, "every key survives multiple resizes with correct value");
    store_free(s);
}

/* ---- The oracle property test (the backbone) ---------------------------- */

/* Run many random operations against both stores; assert agreement after each.
 * Small key pool (forces overwrites, deletes, re-inserts — the bug-prone
 * cases). Fixed seed = deterministic and reproducible (a failure can be
 * replayed exactly). */
static void test_oracle_property(void) {
    Store *s = store_create();
    Oracle *o = oracle_create();

    srand(12345);                 /* fixed seed: reproducible runs */
    const int OPS = 50000;        /* fifty thousand random operations */
    const int KEY_POOL = 50;      /* small pool -> lots of collisions/overwrites */

    bool agreed = true;
    for (int i = 0; i < OPS && agreed; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", rand() % KEY_POOL);
        int op = rand() % 3;      /* 0=set, 1=get, 2=delete */

        if (op == 0) {
            char val[16];
            snprintf(val, sizeof(val), "v%d", rand() % 1000);
            store_set(s, key, val);
            oracle_set(o, key, val);
        } else if (op == 1) {
            const char *sv = store_get(s, key);
            const char *ov = oracle_get(o, key);
            /* both NULL, or both non-NULL and equal */
            bool match = (sv == NULL && ov == NULL) ||
                         (sv != NULL && ov != NULL && strcmp(sv, ov) == 0);
            if (!match) agreed = false;
        } else {
            bool sd = store_delete(s, key);
            bool od = oracle_delete(o, key);
            if (sd != od) agreed = false;
        }

        /* after every op, the two stores must report the same size */
        if (store_size(s) != oracle_size(o)) agreed = false;
    }

    CHECK(agreed, "store matches oracle across 50000 random operations");
    store_free(s);
    oracle_free(o);
}

int main(void) {
    RUN_TEST(test_basic_set_get);
    RUN_TEST(test_overwrite);
    RUN_TEST(test_delete);
    RUN_TEST(test_resize_keeps_all_keys);
    RUN_TEST(test_oracle_property);
    TEST_SUMMARY();
}
