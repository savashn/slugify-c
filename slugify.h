#ifndef SLUGIFY_H
#define SLUGIFY_H

#include <stdlib.h>
#include <stdint.h>

/* Error codes */
#define SLUGIFY_SUCCESS 0
#define SLUGIFY_ERROR_BUFFER 1
#define SLUGIFY_ERROR_INVALID 2
#define SLUGIFY_ERROR_EMPTY 3
#define SLUGIFY_ERROR_MEMORY 4

/* Options flags */
#define LOWERCASE (1 << 0)     /* Convert to lowercase (default) */
#define PRESERVE_CASE (1 << 1) /* Preserve original case */
#define ALLOW_UNICODE (1 << 3) /* Allow Unicode characters */

/* Unicode validation limits */
#define UNICODE_MAX_CODEPOINT 0x10FFFF
#define UNICODE_SURROGATE_HIGH_START 0xD800
#define UNICODE_SURROGATE_LOW_END 0xDFFF

typedef struct
{
    uint32_t flags;
    char separator;    /* Default: '-' */
    size_t max_length; /* Max output length, 0 = no limit */
} slugify_options_t;

/* Transliteration table entry */
typedef struct
{
    uint32_t unicode;
    const char *ascii;
} transliteration_entry_t;

/**
 * slugify - converts a UTF-8 string into a slug.
 *
 * Parameters:
 *   input    - NUL-terminated UTF-8 string to slugify
 *   output   - buffer to write the slug into
 *   out_size - size of the output buffer
 *
 * Returns:
 *   Allocates and returns a slug string.
 *   Caller must free the returned string.
 */

char *slugify(const char *input, const slugify_options_t *options);

#endif /* SLUGIFY_H */
