/* clock.h
 *
 * An injectable clock. The store never calls the OS time functions directly;
 * instead it holds a Clock and asks IT for the time. In production we give it a
 * real clock (reads the OS). In tests we give it a mock clock whose time WE set
 * by hand — so expiry logic is tested instantly and deterministically, with no
 * sleeping and no flakiness.
 *
 * This is "dependency injection": the code depends on time, so we inject time
 * into it rather than letting it reach out and grab the real clock.
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/* A Clock is a function that returns "the current time" as a uint64_t, plus an
 * optional pointer to its own private state (used by the mock clock to hold the
 * settable time). This "function pointer + context" pattern is how C does
 * pluggable behavior.
 *
 * "now" is a POINTER TO A FUNCTION: given the clock's own state, it returns the
 * current time. Different clocks plug in different functions here. */
typedef struct Clock {
    uint64_t (*now)(void *state);   /* the time-reading function */
    void *state;                    /* clock-specific data (NULL for real clock)*/
} Clock;

/* Ask the clock for the current time. Thin wrapper so callers write
 * clock_now(&clk) instead of poking at the struct. */
static inline uint64_t clock_now(Clock *clk) {
    return clk->now(clk->state);
}

/* --- Real clock: reads the OS monotonic time in milliseconds. --- */
uint64_t real_clock_now(void *state);

/* Convenience: a ready-made real clock. */
Clock clock_real(void);

/* --- Mock clock: returns a time you set by hand. For tests. --- */
/* The mock's state is just a settable uint64_t. */
typedef struct MockClock {
    uint64_t current;
} MockClock;

uint64_t mock_clock_now(void *state);

/* Build a Clock backed by the given MockClock. Set mock->current to control
 * "what time it is". */
Clock clock_mock(MockClock *mock);

#endif /* CLOCK_H */
