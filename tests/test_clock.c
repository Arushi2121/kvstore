/* test_clock.c — verifies the mock clock returns exactly the time we set. */
#include "test_harness.h"
#include "clock.h"

static void test_mock_clock_controllable(void) {
    MockClock mock = { .current = 1000 };
    Clock clk = clock_mock(&mock);

    CHECK(clock_now(&clk) == 1000, "mock clock reads initial time 1000");

    mock.current = 5000;            /* move time forward by hand */
    CHECK(clock_now(&clk) == 5000, "mock clock reflects updated time 5000");

    mock.current += 1;              /* tick forward by 1 */
    CHECK(clock_now(&clk) == 5001, "mock clock ticks to 5001");
}

static void test_real_clock_moves_forward(void) {
    Clock clk = clock_real();
    uint64_t t1 = clock_now(&clk);
    /* busy a tiny bit so time advances */
    for (volatile int i = 0; i < 10000000; i++) { }
    uint64_t t2 = clock_now(&clk);
    CHECK(t2 >= t1, "real monotonic clock never goes backwards");
}

int main(void) {
    RUN_TEST(test_mock_clock_controllable);
    RUN_TEST(test_real_clock_moves_forward);
    TEST_SUMMARY();
}
