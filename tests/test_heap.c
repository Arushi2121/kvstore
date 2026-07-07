/* test_heap.c — verifies the min-heap in isolation, before the store uses it. */
#include "test_harness.h"
#include "heap.h"

#include <stdlib.h>
#include <stdio.h>

static void test_heap_basic_order(void) {
    Heap *h = heap_create();
    /* push out of order */
    heap_push(h, "c", 30);
    heap_push(h, "a", 10);
    heap_push(h, "b", 20);
    CHECK(heap_size(h) == 3, "heap size is 3 after three pushes");

    uint64_t min;
    heap_peek_min(h, &min);
    CHECK(min == 10, "peek returns the smallest expiry (10)");

    /* popping should yield 10, 20, 30 in order */
    char *key; uint64_t exp;
    heap_pop_min(h, &key, &exp);
    CHECK(exp == 10, "first pop is 10"); free(key);
    heap_pop_min(h, &key, &exp);
    CHECK(exp == 20, "second pop is 20"); free(key);
    heap_pop_min(h, &key, &exp);
    CHECK(exp == 30, "third pop is 30"); free(key);

    CHECK(heap_size(h) == 0, "heap empty after popping all");
    CHECK(heap_pop_min(h, &key, &exp) == false, "pop on empty returns false");
    heap_free(h);
}

/* Property test: push N random expiry times, pop them all, assert they come out
 * in non-decreasing order. If the heap logic is wrong, order breaks here. */
static void test_heap_property_sorted_output(void) {
    Heap *h = heap_create();
    srand(999);
    const int N = 10000;
    for (int i = 0; i < N; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        heap_push(h, key, (uint64_t)(rand() % 100000));
    }

    bool sorted = true;
    uint64_t prev = 0;
    for (int i = 0; i < N; i++) {
        char *key; uint64_t exp;
        heap_pop_min(h, &key, &exp);
        free(key);
        if (exp < prev) { sorted = false; break; }  /* out of order! */
        prev = exp;
    }
    CHECK(sorted, "10000 random pushes pop out in non-decreasing order");
    heap_free(h);
}

int main(void) {
    RUN_TEST(test_heap_basic_order);
    RUN_TEST(test_heap_property_sorted_output);
    TEST_SUMMARY();
}
