/* test_store.c — store core tests (now clock-aware) plus TTL tests. */
#include "test_harness.h"
#include "oracle.h"
#include "store.h"
#include "clock.h"

#include <stdlib.h>
#include <stdio.h>

/* A real clock is fine for the non-TTL tests (they never check expiry). */
static void test_basic_set_get(void) {
    Store *s = store_create(clock_real());
    CHECK(s != NULL, "store_create returns non-NULL");
    CHECK(store_set(s, "name", "arushi"), "set name=arushi succeeds");
    const char *v = store_get(s, "name");
    CHECK(v != NULL && strcmp(v, "arushi") == 0, "get name returns arushi");
    CHECK(store_get(s, "missing") == NULL, "get missing key returns NULL");
    CHECK(store_size(s) == 1, "size is 1 after one set");
    store_free(s);
}

static void test_overwrite(void) {
    Store *s = store_create(clock_real());
    store_set(s, "k", "v1");
    store_set(s, "k", "v2");
    const char *v = store_get(s, "k");
    CHECK(v != NULL && strcmp(v, "v2") == 0, "overwrite updates value");
    CHECK(store_size(s) == 1, "overwrite does not increase size");
    store_free(s);
}

static void test_delete(void) {
    Store *s = store_create(clock_real());
    store_set(s, "k", "v");
    CHECK(store_delete(s, "k") == true, "delete existing key returns true");
    CHECK(store_get(s, "k") == NULL, "deleted key is gone");
    CHECK(store_size(s) == 0, "size is 0 after delete");
    CHECK(store_delete(s, "k") == false, "delete missing key returns false");
    store_free(s);
}

static void test_resize_keeps_all_keys(void) {
    Store *s = store_create(clock_real());
    const int N = 5000;
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
        if (got == NULL || strcmp(got, val) != 0) { all_present = false; break; }
    }
    CHECK(all_present, "every key survives multiple resizes with correct value");
    store_free(s);
}

/* Oracle property test uses a real clock and no TTLs, so behavior matches the
 * (TTL-free) oracle exactly. */
static void test_oracle_property(void) {
    Store *s = store_create(clock_real());
    Oracle *o = oracle_create();
    srand(12345);
    const int OPS = 50000;
    const int KEY_POOL = 50;
    bool agreed = true;
    for (int i = 0; i < OPS && agreed; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", rand() % KEY_POOL);
        int op = rand() % 3;
        if (op == 0) {
            char val[16];
            snprintf(val, sizeof(val), "v%d", rand() % 1000);
            store_set(s, key, val);
            oracle_set(o, key, val);
        } else if (op == 1) {
            const char *sv = store_get(s, key);
            const char *ov = oracle_get(o, key);
            bool match = (sv == NULL && ov == NULL) ||
                         (sv != NULL && ov != NULL && strcmp(sv, ov) == 0);
            if (!match) agreed = false;
        } else {
            bool sd = store_delete(s, key);
            bool od = oracle_delete(o, key);
            if (sd != od) agreed = false;
        }
        if (store_size(s) != oracle_size(o)) agreed = false;
    }
    CHECK(agreed, "store matches oracle across 50000 random operations");
    store_free(s);
    oracle_free(o);
}

/* ---- TTL tests using the MOCK CLOCK (instant, deterministic) ------------- */

static void test_ttl_expires_at_deadline(void) {
    MockClock mock = { .current = 1000 };
    Store *s = store_create(clock_mock(&mock));

    store_set_ttl(s, "session", "token", 100);   /* expires at 1000+100 = 1100 */

    mock.current = 1050;   /* before deadline */
    CHECK(store_get(s, "session") != NULL, "key alive before deadline");

    mock.current = 1100;   /* exactly at deadline: expire_at <= now => expired */
    CHECK(store_get(s, "session") == NULL, "key expired exactly at deadline");

    store_free(s);
}

static void test_ttl_not_expired_before(void) {
    MockClock mock = { .current = 0 };
    Store *s = store_create(clock_mock(&mock));
    store_set_ttl(s, "k", "v", 10000);
    mock.current = 9999;
    const char *v = store_get(s, "k");
    CHECK(v != NULL && strcmp(v, "v") == 0, "key still alive one tick before expiry");
    store_free(s);
}

static void test_plain_set_never_expires(void) {
    MockClock mock = { .current = 0 };
    Store *s = store_create(clock_mock(&mock));
    store_set(s, "immortal", "v");           /* no TTL */
    mock.current = UINT64_MAX - 1;           /* far, far future */
    CHECK(store_get(s, "immortal") != NULL, "plain SET key never expires");
    store_free(s);
}

static void test_set_clears_prior_ttl(void) {
    MockClock mock = { .current = 0 };
    Store *s = store_create(clock_mock(&mock));
    store_set_ttl(s, "k", "v1", 100);        /* would expire at 100 */
    store_set(s, "k", "v2");                  /* plain SET should clear TTL */
    mock.current = 1000;                      /* well past the old TTL */
    const char *v = store_get(s, "k");
    CHECK(v != NULL && strcmp(v, "v2") == 0, "plain SET after TTL makes key immortal");
    store_free(s);
}

static void test_active_expire_cycle_reclaims(void) {
    MockClock mock = { .current = 0 };
    Store *s = store_create(clock_mock(&mock));
    /* three keys expiring at 10, 20, 30 */
    store_set_ttl(s, "a", "1", 10);
    store_set_ttl(s, "b", "2", 20);
    store_set_ttl(s, "c", "3", 30);
    CHECK(store_size(s) == 3, "three keys before expiry");

    mock.current = 25;   /* a and b expired, c not */
    size_t reclaimed = store_expire_cycle(s);
    CHECK(reclaimed == 2, "active cycle reclaims exactly the 2 expired keys");
    CHECK(store_size(s) == 1, "one key (c) remains after cycle");
    CHECK(store_get(s, "c") != NULL, "c still retrievable");
    store_free(s);
}

static void test_expire_cycle_handles_stale_heap_entry(void) {
    MockClock mock = { .current = 0 };
    Store *s = store_create(clock_mock(&mock));
    store_set_ttl(s, "k", "v1", 10);   /* heap reminder for time 10 */
    store_set(s, "k", "v2");            /* overwrite: now immortal, heap entry stale */
    mock.current = 100;
    size_t reclaimed = store_expire_cycle(s);   /* should NOT delete k */
    CHECK(reclaimed == 0, "stale heap entry does not delete a reset key");
    CHECK(store_get(s, "k") != NULL, "reset key survives despite stale reminder");
    store_free(s);
}

int main(void) {
    RUN_TEST(test_basic_set_get);
    RUN_TEST(test_overwrite);
    RUN_TEST(test_delete);
    RUN_TEST(test_resize_keeps_all_keys);
    RUN_TEST(test_oracle_property);
    RUN_TEST(test_ttl_expires_at_deadline);
    RUN_TEST(test_ttl_not_expired_before);
    RUN_TEST(test_plain_set_never_expires);
    RUN_TEST(test_set_clears_prior_ttl);
    RUN_TEST(test_active_expire_cycle_reclaims);
    RUN_TEST(test_expire_cycle_handles_stale_heap_entry);
    TEST_SUMMARY();
}