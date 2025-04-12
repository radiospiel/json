#include "ryu.h"

// Digit table for fast conversion
const char DIGIT_TABLE[200] = {
    '0','0', '0','1', '0','2', '0','3', '0','4', '0','5', '0','6', '0','7', '0','8', '0','9',
    '1','0', '1','1', '1','2', '1','3', '1','4', '1','5', '1','6', '1','7', '1','8', '1','9',
    '2','0', '2','1', '2','2', '2','3', '2','4', '2','5', '2','6', '2','7', '2','8', '2','9',
    '3','0', '3','1', '3','2', '3','3', '3','4', '3','5', '3','6', '3','7', '3','8', '3','9',
    '4','0', '4','1', '4','2', '4','3', '4','4', '4','5', '4','6', '4','7', '4','8', '4','9',
    '5','0', '5','1', '5','2', '5','3', '5','4', '5','5', '5','6', '5','7', '5','8', '5','9',
    '6','0', '6','1', '6','2', '6','3', '6','4', '6','5', '6','6', '6','7', '6','8', '6','9',
    '7','0', '7','1', '7','2', '7','3', '7','4', '7','5', '7','6', '7','7', '7','8', '7','9',
    '8','0', '8','1', '8','2', '8','3', '8','4', '8','5', '8','6', '8','7', '8','8', '8','9',
    '9','0', '9','1', '9','2', '9','3', '9','4', '9','5', '9','6', '9','7', '9','8', '9','9'
};

// Platform-specific implementation of 128-bit multiplication
uint64_t umul128(uint64_t a, uint64_t b, uint64_t* hi) {
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

// Platform-specific implementation of 128-bit right shift
uint64_t shiftright128(uint64_t lo, uint64_t hi, uint32_t dist) {
    // We don't need to handle the case when dist >= 64 because the caller
    // ensures that dist < 64.
    return (hi << (64 - dist)) | (lo >> dist);
} 