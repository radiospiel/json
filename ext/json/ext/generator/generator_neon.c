#include "generator_internal.h"
#include <arm_neon.h>

/*
 * This file defines the search_escape_basic_neon(search_state *search) function.
 */

#ifdef HAVE_SIMD_NEON

static inline FORCE_INLINE unsigned char neon_next_match(search_state *search)
{
    uint64_t mask = search->matches_mask;
    uint32_t index = trailing_zeros64(mask) >> 2;

    // It is assumed escape_UTF8_char_basic will only ever increase search->ptr by at most one character.
    // If we want to use a similar approach for full escaping we'll need to ensure:
    //     search->chunk_base + index >= search->ptr
    // However, since we know escape_UTF8_char_basic only increases search->ptr by one, if the next match
    // is one byte after the previous match then:
    //     search->chunk_base + index == search->ptr
    search->ptr = search->chunk_base + index;
    mask &= mask - 1;
    search->matches_mask = mask;
    search_flush(search);
    return 1;
}

// See: https://community.arm.com/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
static inline FORCE_INLINE uint64_t neon_match_mask(uint8x16_t matches)
{
    const uint8x8_t res = vshrn_n_u16(vreinterpretq_u16_u8(matches), 4);
    const uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(res), 0);
    return mask & 0x8888888888888888ull;
}

static inline FORCE_INLINE uint64_t neon_rules_update(const char *ptr)
{
    uint8x16_t chunk = vld1q_u8((const unsigned char *)ptr);

    // Trick: c < 32 || c == 34 can be factored as c ^ 2 < 33
    // https://lemire.me/blog/2025/04/13/detect-control-characters-quotes-and-backslashes-efficiently-using-swar/
    const uint8x16_t too_low_or_dbl_quote = vcltq_u8(veorq_u8(chunk, vdupq_n_u8(2)), vdupq_n_u8(33));

    uint8x16_t has_backslash = vceqq_u8(chunk, vdupq_n_u8('\\'));
    uint8x16_t needs_escape  = vorrq_u8(too_low_or_dbl_quote, has_backslash);

    return neon_match_mask(needs_escape);
}

unsigned char search_escape_basic_neon(search_state *search)
{
    if (RB_UNLIKELY(search->has_matches)) {
        // There are more matches if search->matches_mask > 0.
        if (search->matches_mask > 0) {
            return neon_next_match(search);
        } else {
            // neon_next_match will only advance search->ptr up to the last matching character.
            // Skip over any characters in the last chunk that occur after the last match.
            search->has_matches = false;
            search->ptr = search->chunk_end;
        }
    }

    /*
    * The code below implements an SIMD-based algorithm to determine if N bytes at a time
    * need to be escaped.
    *
    * Assume the ptr = "Te\sting!" (the double quotes are included in the string)
    *
    * The explanation will be limited to the first 8 bytes of the string for simplicity. However
    * the vector insructions may work on larger vectors.
    *
    * First, we load three constants 'lower_bound', 'backslash' and 'dblquote" in vector registers.
    *
    * lower_bound: [20 20 20 20 20 20 20 20]
    * backslash:   [5C 5C 5C 5C 5C 5C 5C 5C]
    * dblquote:    [22 22 22 22 22 22 22 22]
    *
    * Next we load the first chunk of the ptr:
    * [22 54 65 5C 73 74 69 6E] ("  T  e  \  s  t  i  n)
    *
    * First we check if any byte in chunk is less than 32 (0x20). This returns the following vector
    * as no bytes are less than 32 (0x20):
    * [0 0 0 0 0 0 0 0]
    *
    * Next, we check if any byte in chunk is equal to a backslash:
    * [0 0 0 FF 0 0 0 0]
    *
    * Finally we check if any byte in chunk is equal to a double quote:
    * [FF 0 0 0 0 0 0 0]
    *
    * Now we have three vectors where each byte indicates if the corresponding byte in chunk
    * needs to be escaped. We combine these vectors with a series of logical OR instructions.
    * This is the needs_escape vector and it is equal to:
    * [FF 0 0 FF 0 0 0 0]
    *
    * Next we compute the bitwise AND between each byte and 0x1 and compute the horizontal sum of
    * the values in the vector. This computes how many bytes need to be escaped within this chunk.
    *
    * Finally we compute a mask that indicates which bytes need to be escaped. If the mask is 0 then,
    * no bytes need to be escaped and we can continue to the next chunk. If the mask is not 0 then we
    * have at least one byte that needs to be escaped.
    */
    while (search->ptr + sizeof(uint8x16_t) <= search->end) {
        uint64_t mask = neon_rules_update(search->ptr);

        if (!mask) {
            search->ptr += sizeof(uint8x16_t);
            continue;
        }
        search->matches_mask = mask;
        search->has_matches = true;
        search->chunk_base = search->ptr;
        search->chunk_end = search->ptr + sizeof(uint8x16_t);
        return neon_next_match(search);
    }

    // There are fewer than 16 bytes left.
    unsigned long remaining = (search->end - search->ptr);
    if (remaining >= SIMD_MINIMUM_THRESHOLD) {
        char *s = copy_remaining_bytes(search, sizeof(uint8x16_t), remaining);

        uint64_t mask = neon_rules_update(s);

        if (!mask) {
            // Nothing to escape, ensure search_flush doesn't do anything by setting
            // search->cursor to search->ptr.
            search->buffer->len += remaining;
            search->ptr = search->end;
            search->cursor = search->end;
            return 0;
        }

        search->matches_mask = mask;
        search->has_matches = true;
        search->chunk_end = search->end;
        search->chunk_base = search->ptr;
        return neon_next_match(search);
    }

    if (search->ptr < search->end) {
        // Fallback to basic search for the very few remaining bytes
        return search_escape_basic(search);
    }

    search_flush(search);
    return 0;
}

#endif /* HAVE_SIMD_NEON */
