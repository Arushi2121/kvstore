#include "test_harness.h"

static void test_arithmetic_sanity(void) {
    CHECK(2 + 2 == 4, "basic arithmetic works");
    CHECK(10 - 3 == 7, "subtraction works");
}

static void test_string_sanity(void) {
    CHECK(strcmp("hello", "hello") == 0, "identical strings compare equal");
    CHECK(strcmp("hello", "world") != 0, "different strings compare unequal");
}

int main(void) {
    RUN_TEST(test_arithmetic_sanity);
    RUN_TEST(test_string_sanity);
    TEST_SUMMARY();
}
