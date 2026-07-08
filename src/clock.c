/* clock.c — the two clock implementations. */
#include "clock.h"

#include <time.h>

/* Real clock: monotonic time in milliseconds. CLOCK_MONOTONIC never goes
 * backwards (unlike wall-clock time, which can jump when the system clock is
 * adjusted) — exactly what you want for measuring expiry. */
uint64_t real_clock_now(void *state) {
    (void)state;                    /* real clock has no state; silence warning */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* seconds -> ms, plus nanoseconds -> ms */
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

Clock clock_real(void) {
    Clock c;
    c.now = real_clock_now;
    c.state = NULL;
    return c;
}

/* Mock clock: just returns whatever's in mock->current. Tests set that field
 * directly to move time forward instantly. */
uint64_t mock_clock_now(void *state) {
    MockClock *mock = (MockClock *)state;
    return mock->current;
}

Clock clock_mock(MockClock *mock) {
    Clock c;
    c.now = mock_clock_now;
    c.state = mock;
    return c;
}
