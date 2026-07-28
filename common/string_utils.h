/*
 * USB Over Network - Safe String Helpers
 *
 * str_copy is a strlcpy-style bounded copy that ALWAYS null-terminates,
 * silencing -Wstringop-truncation and removing the footgun where strncpy
 * leaves the destination unterminated when the source fills the buffer.
 */

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

/* Copy src into dst (size dst_size), always null-terminating. Truncates
 * silently if src is longer than dst_size-1. Returns dst. Safe with
 * dst_size == 0 (no-op). */
static inline char* str_copy(char *dst, const char *src, size_t dst_size) {
    if (dst_size == 0) return dst;
    size_t i = 0;
    for (; i < dst_size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return dst;
}

#endif /* STRING_UTILS_H */
