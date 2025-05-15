
#ifndef GENERATOR_INTERNAL_H
#define GENERATOR_INTERNAL_H

#include "ruby.h"
#include "../fbuffer/fbuffer.h"
#include "simd.h" // For SIMD_MINIMUM_THRESHOLD, trailing_zeros64, trailing_zeros, etc.

// Define RB_UNLIKELY if not already defined (ruby.h usually provides this)
#ifndef RB_UNLIKELY
#define RB_UNLIKELY(cond) (cond)
#endif

// Define FORCE_INLINE if not already defined
#if (defined(__GNUC__ ) || defined(__clang__))
#define FORCE_INLINE __attribute__((always_inline))
#else
#define FORCE_INLINE
#endif

// search_state struct definition
typedef struct _search_state {
    const char *ptr;
    const char *end;
    const char *cursor;
    FBuffer *buffer;

#ifdef HAVE_SIMD
    const char *chunk_base;
    const char *chunk_end;
    bool has_matches;

#if defined(HAVE_SIMD_NEON)
    uint64_t matches_mask;
#elif defined(HAVE_SIMD_SSE2)
    int matches_mask;
#else
#error "Unknown SIMD Implementation."
#endif /* HAVE_SIMD_NEON */
#endif /* HAVE_SIMD */
} search_state;

// Function prototypes defined in generator.c but used by SIMD code
static inline FORCE_INLINE void search_flush(search_state *search);
static inline FORCE_INLINE char *copy_remaining_bytes(search_state *search, unsigned long vec_len, unsigned long len);
unsigned char search_escape_basic(search_state *search); // Prototype for the basic search function

int search_escape_basic(const char *s, size_t len, const char *chars_to_escape, char *escaped_s);

// Prototype for the NEON search function (will be defined in generator_neon.c)
#ifdef HAVE_SIMD_NEON
unsigned char search_escape_basic_neon(search_state *search);
#endif

// Prototype for the SSE2 search function (will be defined in generator_sse2.c)
#ifdef HAVE_SIMD_SSE2
unsigned char search_escape_basic_sse2(search_state *search);
#endif

#endif // GENERATOR_INTERNAL_H