#ifndef RYU_H
#define RYU_H

#include <stdint.h>
#include <stdbool.h>

// External interface for the Ryu algorithm
int ryu_dtoa(double value, char* buffer);

// Helper functions that need to be implemented by the platform
static inline uint64_t umul128(uint64_t a, uint64_t b, uint64_t* hi) {
#ifdef __SIZEOF_INT128__
    // Use native 128-bit support if available
    __uint128_t r = (__uint128_t)a * (__uint128_t)b;
    *hi = (uint64_t)(r >> 64);
    return (uint64_t)r;
#else
    // Fallback implementation using 64-bit arithmetic
    const uint64_t a_lo = (uint32_t)a;
    const uint64_t a_hi = a >> 32;
    const uint64_t b_lo = (uint32_t)b;
    const uint64_t b_hi = b >> 32;

    const uint64_t p0 = a_lo * b_lo;
    const uint64_t p1 = a_lo * b_hi;
    const uint64_t p2 = a_hi * b_lo;
    const uint64_t p3 = a_hi * b_hi;

    const uint64_t p0_hi = p0 >> 32;
    const uint64_t p1_lo = (uint32_t)p1;
    const uint64_t p1_hi = p1 >> 32;
    const uint64_t p2_lo = (uint32_t)p2;
    const uint64_t p2_hi = p2 >> 32;

    uint64_t lo = p0_hi + p1_lo + p2_lo;
    uint64_t hi = p3 + p1_hi + p2_hi + (lo >> 32);
    lo = (lo << 32) | (uint32_t)p0;

    *hi = hi;
    return lo;
#endif
}

static inline uint64_t shiftright128(uint64_t lo, uint64_t hi, uint32_t dist) {
    // We don't need to handle the case when dist >= 64 because the caller
    // ensures that dist < 64.
    return (hi << (64 - dist)) | (lo >> dist);
}

// Digit table for fast conversion
extern const char DIGIT_TABLE[200];

#endif // RYU_H 