#include "generator_internal.h"
#include <x86intrin.h>

#ifdef HAVE_SIMD_SSE2

#define _mm_cmpge_epu8(a, b) _mm_cmpeq_epi8(_mm_max_epu8(a, b), a)
#define _mm_cmple_epu8(a, b) _mm_cmpge_epu8(b, a)
#define _mm_cmpgt_epu8(a, b) _mm_xor_si128(_mm_cmple_epu8(a, b), _mm_set1_epi8(-1))
#define _mm_cmplt_epu8(a, b) _mm_cmpgt_epu8(b, a)

static inline FORCE_INLINE unsigned char sse2_next_match(search_state *search)
{
    int mask = search->matches_mask;
    int index = trailing_zeros(mask);

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

#if defined(__clang__) || defined(__GNUC__)
#define TARGET_SSE2 __attribute__((target("sse2")))
#else
#define TARGET_SSE2
#endif

static inline TARGET_SSE2 FORCE_INLINE int sse2_update(const char *ptr)
{
    __m128i chunk         = _mm_loadu_si128((__m128i const*)ptr);

    // Trick: c < 32 || c == 34 can be factored as c ^ 2 < 33
    // https://lemire.me/blog/2025/04/13/detect-control-characters-quotes-and-backslashes-efficiently-using-swar/
    __m128i too_low_or_dbl_quote = _mm_cmplt_epu8(_mm_xor_si128(chunk, _mm_set1_epi8(2)), _mm_set1_epi8(33));
    __m128i has_backslash = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\\'));
    __m128i needs_escape  = _mm_or_si128(too_low_or_dbl_quote, has_backslash);
    return _mm_movemask_epi8(needs_escape);
}

unsigned char search_escape_basic_sse2(search_state *search)
{
    if (RB_UNLIKELY(search->has_matches)) {
        // There are more matches if search->matches_mask > 0.
        if (search->matches_mask > 0) {
            return sse2_next_match(search);
        } else {
            // sse2_next_match will only advance search->ptr up to the last matching character.
            // Skip over any characters in the last chunk that occur after the last match.
            search->has_matches = false;
            if (RB_UNLIKELY(search->chunk_base + sizeof(__m128i) >= search->end)) {
                search->ptr = search->end;
            } else {
                search->ptr = search->chunk_base + sizeof(__m128i);
            }
        }
    }

    while (search->ptr + sizeof(__m128i) <= search->end) {
        int needs_escape_mask = sse2_update(search->ptr);

        if (needs_escape_mask == 0) {
            search->ptr += sizeof(__m128i);
            continue;
        }

        search->has_matches = true;
        search->matches_mask = needs_escape_mask;
        search->chunk_base = search->ptr;
        return sse2_next_match(search);
    }

    // There are fewer than 16 bytes left.
    unsigned long remaining = (search->end - search->ptr);
    if (remaining >= SIMD_MINIMUM_THRESHOLD) {
        char *s = copy_remaining_bytes(search, sizeof(__m128i), remaining);

        int needs_escape_mask = sse2_update(s);

        if (needs_escape_mask == 0) {
            // Nothing to escape, ensure search_flush doesn't do anything by setting
            // search->cursor to search->ptr.
            search->buffer->len += remaining;
            search->ptr = search->end;
            search->cursor = search->end;
            return 0;
        }

        search->has_matches = true;
        search->matches_mask = needs_escape_mask;
        search->chunk_base = search->ptr;
        return sse2_next_match(search);
    }

    if (search->ptr < search->end) {
        return search_escape_basic(search);
    }

    search_flush(search);
    return 0;
}

#endif /* HAVE_SIMD_SSE2 */