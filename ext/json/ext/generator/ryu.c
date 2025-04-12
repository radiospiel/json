#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "ryu.h"

// Constants for double-precision floating-point
#define DOUBLE_MANTISSA_BITS 52
#define DOUBLE_EXPONENT_BITS 11
#define DOUBLE_EXPONENT_BIAS 1023
#define DOUBLE_SIGN_BIT (1ull << 63)

// Lookup tables for Ryu
static const uint32_t POW10_OFFSET[2] = { 0, 2 };
static const uint64_t POW10_SPLIT[2][2] = {
    { 0, 10000000000000000000ull },
    { 0, 10000000000000000000ull }
};

// Helper functions
static inline uint32_t decimalLength17(const uint64_t v) {
    // This is a fast path for the common case
    if (v < 100000000) {
        if (v < 10000) {
            if (v < 100) {
                return v < 10 ? 1 : 2;
            } else {
                return v < 1000 ? 3 : 4;
            }
        } else {
            if (v < 1000000) {
                return v < 100000 ? 5 : 6;
            } else {
                return v < 10000000 ? 7 : 8;
            }
        }
    } else {
        if (v < 1000000000000) {
            if (v < 10000000000) {
                return v < 1000000000 ? 9 : 10;
            } else {
                return v < 100000000000 ? 11 : 12;
            }
        } else {
            if (v < 100000000000000) {
                return v < 10000000000000 ? 13 : 14;
            } else {
                return v < 1000000000000000 ? 15 : 16;
            }
        }
    }
}

static inline uint64_t mulShift(const uint64_t m, const uint64_t* const mul, const int32_t j) {
    // m is maximum 55 bits
    uint64_t high1;
    uint64_t low1 = umul128(m, mul[1], &high1);
    uint64_t high0;
    uint64_t low0 = umul128(m, mul[0], &high0);
    uint64_t sum = high0 + low1;
    if (sum < high0) {
        ++high1; // overflow into high1
    }
    return shiftright128(low0, sum, j);
}

static inline uint32_t pow5Factor(uint64_t value) {
    uint32_t count = 0;
    for (;;) {
        if (value == 0) {
            return count;
        }
        const uint64_t q = value / 5;
        const uint32_t r = (uint32_t)(value - 5 * q);
        if (r != 0) {
            return count;
        }
        value = q;
        ++count;
    }
}

// Main Ryu implementation
int ryu_dtoa(double value, char* buffer) {
    // Step 1: Decode the floating-point number
    uint64_t bits = *(uint64_t*)&value;
    const uint64_t ieeeMantissa = bits & ((1ull << DOUBLE_MANTISSA_BITS) - 1);
    const uint64_t ieeeExponent = (bits >> DOUBLE_MANTISSA_BITS) & ((1ull << DOUBLE_EXPONENT_BITS) - 1);
    const bool ieeeSign = ((bits >> (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS)) & 1) != 0;

    // Step 2: Handle special cases
    if (ieeeExponent == ((1ull << DOUBLE_EXPONENT_BITS) - 1)) {
        if (ieeeMantissa == 0) {
            if (ieeeSign) {
                memcpy(buffer, "-Infinity", 9);
                return 9;
            } else {
                memcpy(buffer, "Infinity", 8);
                return 8;
            }
        } else {
            memcpy(buffer, "NaN", 3);
            return 3;
        }
    }

    // Step 3: Convert to decimal
    int32_t e2;
    uint64_t m2;
    if (ieeeExponent == 0) {
        e2 = 1 - DOUBLE_EXPONENT_BIAS - DOUBLE_MANTISSA_BITS;
        m2 = ieeeMantissa;
    } else {
        e2 = (int32_t)ieeeExponent - DOUBLE_EXPONENT_BIAS - DOUBLE_MANTISSA_BITS;
        m2 = (1ull << DOUBLE_MANTISSA_BITS) | ieeeMantissa;
    }

    // Step 4: Find the shortest decimal representation
    const bool even = (m2 & 1) == 0;
    const bool acceptBounds = even;

    // Step 5: Compute the decimal digits
    uint32_t mv = (uint32_t)(m2 >> 32);
    uint32_t mmShift = ieeeMantissa != 0 || ieeeExponent <= 1;
    uint32_t vr, vp, vm;
    int32_t e10;
    bool vmIsTrailingZeros = false;
    bool vrIsTrailingZeros = false;

    // Step 6: Generate the decimal string
    int index = 0;
    if (ieeeSign) {
        buffer[index++] = '-';
    }

    // Step 7: Handle the integer part
    uint32_t output = mv;
    uint32_t olength = decimalLength17(output);
    int32_t exp = e10 + olength - 1;

    // Step 8: Generate the digits
    uint32_t i = 0;
    while (output >= 10000) {
        const uint32_t c = output - 10000 * (output / 10000);
        output /= 10000;
        const uint32_t c0 = (c % 100) << 1;
        const uint32_t c1 = (c / 100) << 1;
        memcpy(buffer + index + olength - i - 2, DIGIT_TABLE + c0, 2);
        memcpy(buffer + index + olength - i - 4, DIGIT_TABLE + c1, 2);
        i += 4;
    }
    if (output >= 100) {
        const uint32_t c = (output % 100) << 1;
        output /= 100;
        memcpy(buffer + index + olength - i - 2, DIGIT_TABLE + c, 2);
        i += 2;
    }
    if (output >= 10) {
        const uint32_t c = output << 1;
        memcpy(buffer + index + olength - i - 2, DIGIT_TABLE + c, 2);
    } else {
        buffer[index] = (char)('0' + output);
    }

    // Step 9: Add decimal point and exponent if needed
    if (exp < -4 || exp >= olength) {
        // Scientific notation
        buffer[index + 1] = '.';
        memmove(buffer + index + 2, buffer + index + 1, olength - 1);
        buffer[index + olength + 1] = 'e';
        index += olength + 2;
        if (exp < 0) {
            buffer[index++] = '-';
            exp = -exp;
        } else {
            buffer[index++] = '+';
        }
        if (exp >= 100) {
            buffer[index++] = (char)('0' + exp / 100);
            exp %= 100;
        }
        buffer[index++] = (char)('0' + exp / 10);
        buffer[index++] = (char)('0' + exp % 10);
    } else if (exp < 0) {
        // Decimal point before the first digit
        memmove(buffer + index + 2, buffer + index, olength);
        buffer[index] = '0';
        buffer[index + 1] = '.';
        for (int32_t i = 0; i < -exp - 1; ++i) {
            buffer[index + 2 + i] = '0';
        }
        index += olength + 2 + (-exp - 1);
    } else if (exp + 1 < olength) {
        // Decimal point in the middle
        memmove(buffer + index + exp + 2, buffer + index + exp + 1, olength - exp - 1);
        buffer[index + exp + 1] = '.';
        index += olength + 1;
    } else {
        // No decimal point needed
        index += olength;
    }

    return index;
} 