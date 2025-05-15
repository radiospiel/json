#include "generator_basic.h"
#include <string.h> // For strchr and NULL (often included/defined here or in stddef.h)

/*
 * Escapes characters in a string 's' that are present in 'chars_to_escape'.
 * The escaped string is written to 'escaped_s'.
 *
 * Parameters:
 *   s: The input string. Must not be NULL.
 *   len: The length of the input string 's'.
 *   chars_to_escape: A null-terminated string containing characters to be escaped. Must not be NULL.
 *   escaped_s: The buffer to write the escaped string to. Must not be NULL and
 *              must be large enough to hold the result, including the null terminator.
 *              A safe buffer size is (2 * len + 1) if each character to be escaped
 *              is replaced by two characters (e.g., an escape char + original char).
 *
 * Returns:
 *   The length of the escaped string (excluding the null terminator).
 *   Returns -1 if any of the pointer arguments (s, chars_to_escape, escaped_s) are NULL.
 */
int search_escape_basic(const char *s, size_t len, const char *chars_to_escape, char *escaped_s) {
    if (!s || !chars_to_escape || !escaped_s) {
        return -1; // Indicate an error for NULL inputs
    }

    size_t dest_idx = 0; // Current index in the destination buffer escaped_s
    for (size_t src_idx = 0; src_idx < len; ++src_idx) {
        if (strchr(chars_to_escape, s[src_idx])) {
            escaped_s[dest_idx++] = '\\'; // Prepend an escape character (e.g., backslash)
        }
        escaped_s[dest_idx++] = s[src_idx];
    }
    escaped_s[dest_idx] = '\0'; // Null-terminate the resulting string

    return (int)dest_idx; // Return the length of the escaped content
}

static const unsigned char escape_table_basic[256] = {
    // ASCII Control Characters
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    // ASCII Characters
     0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // '"'
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, // '\\'
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static inline unsigned char search_escape_basic(search_state *search)
{
    while (search->ptr < search->end) {
        if (RB_UNLIKELY(escape_table_basic[(const unsigned char)*search->ptr])) {
            search_flush(search);
            return 1;
        } else {
            search->ptr++;
        }
    }
    search_flush(search);
    return 0;
}
