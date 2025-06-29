#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "slugify.h"

typedef struct
{
    const char *test_name;
    unsigned char *input_bytes;
    size_t input_len;
    int should_succeed;
    const char *description;
    slugify_options_t custom_opts;
    int use_custom_opts;
} overlong_test_t;

void print_hex_bytes(const unsigned char *bytes, size_t len)
{
    printf("Input bytes: ");
    for (size_t i = 0; i < len; i++)
    {
        printf("0x%02X ", bytes[i]);
    }
    printf("(");
    for (size_t i = 0; i < len; i++)
    {
        printf("%c", bytes[i] >= 32 && bytes[i] < 127 ? bytes[i] : '.');
    }
    printf(")\n");
}

void print_options(const slugify_options_t *opts)
{
    printf("Options: separator='%c', max_length=%zu, preserve_case=%d\n",
           opts->separator, opts->max_length, opts->preserve_case);
}

int test_slugify_overlong(const overlong_test_t *test)
{
    printf("\n=== %s ===\n", test->test_name);
    printf("Description: %s\n", test->description);
    print_hex_bytes(test->input_bytes, test->input_len);

    // Create null-terminated string from bytes
    char *input_str = malloc(test->input_len + 1);
    memcpy(input_str, test->input_bytes, test->input_len);
    input_str[test->input_len] = '\0';

    // Use custom options if specified, otherwise use defaults
    slugify_options_t opts;
    if (test->use_custom_opts)
    {
        opts = test->custom_opts;
        print_options(&opts);
    }
    else
    {
        // Default options
        opts.preserve_case = false;
        opts.separator = '-';
        opts.max_length = 0;
    }

    char *result = slugify(input_str, &opts);

    int test_passed;
    if (test->should_succeed)
    {
        if (result != NULL)
        {
            printf("Expected success, got result: '%s'\n", result);
            test_passed = 1;
        }
        else
        {
            printf("Expected success, but got NULL\n");
            test_passed = 0;
        }
    }
    else
    {
        if (result == NULL)
        {
            printf("Expected failure, got NULL (correct rejection)\n");
            test_passed = 1;
        }
        else
        {
            printf("Expected failure, but got result: '%s'\n", result);
            test_passed = 0;
        }
    }

    if (result)
    {
        free(result);
    }
    free(input_str);

    printf("Test result: %s\n", test_passed ? "PASSED" : "FAILED");
    return test_passed;
}

int main()
{
    printf("=== SLUGIFY OVERLONG ENCODING SECURITY TEST ===\n");
    printf("Testing whether slugify() properly rejects overlong UTF-8 sequences\n");
    printf("with both default and custom options\n");

    // Test cases - these should all be REJECTED by a secure implementation
    overlong_test_t tests[] = {
        // Basic tests with default options
        {"Valid ASCII 'A'",
         (unsigned char[]){0x41},
         1,
         1, // Should succeed
         "Normal ASCII character - baseline test",
         {0},
         0}, // No custom options

        {"Overlong 2-byte encoding of 'A'",
         (unsigned char[]){0xC1, 0x81},
         2,
         0, // Should fail
         "0x41 ('A') encoded as 0xC1 0x81 instead of 0x41",
         {0},
         0}, // No custom options

        {"Overlong 3-byte encoding of 'A'",
         (unsigned char[]){0xE0, 0x81, 0x81},
         3,
         0, // Should fail
         "0x41 ('A') encoded as 0xE0 0x81 0x81 instead of 0x41",
         {0},
         0}, // No custom options

        {"Overlong 4-byte encoding of 'A'",
         (unsigned char[]){0xF0, 0x80, 0x81, 0x81},
         4,
         0, // Should fail
         "0x41 ('A') encoded as 0xF0 0x80 0x81 0x81 instead of 0x41",
         {0},
         0}, // No custom options

        {"Overlong encoding of '/' (path traversal risk)",
         (unsigned char[]){0xC0, 0xAF},
         2,
         0, // Should fail
         "0x2F ('/') encoded as 0xC0 0xAF - used in path traversal attacks",
         {0},
         0}, // No custom options

        {"Overlong encoding of '.' (path traversal risk)",
         (unsigned char[]){0xC0, 0xAE},
         2,
         0, // Should fail
         "0x2E ('.') encoded as 0xC0 0xAE - used in path traversal attacks",
         {0},
         0}, // No custom options

        {"Overlong encoding of NUL byte",
         (unsigned char[]){0xC0, 0x80},
         2,
         0, // Should fail
         "0x00 (NUL) encoded as 0xC0 0x80 - can bypass string length checks",
         {0},
         0}, // No custom options

        {"Overlong encoding of space",
         (unsigned char[]){0xC0, 0xA0},
         2,
         0, // Should fail
         "0x20 (space) encoded as 0xC0 0xA0",
         {0},
         0}, // No custom options

        {"Valid 2-byte UTF-8 'ñ'",
         (unsigned char[]){0xC3, 0xB1},
         2,
         1, // Should succeed
         "U+00F1 (ñ) properly encoded as 0xC3 0xB1",
         {0},
         0}, // No custom options

        {"Overlong 3-byte encoding of 'ñ'",
         (unsigned char[]){0xE0, 0x83, 0xB1},
         3,
         0, // Should fail
         "U+00F1 (ñ) encoded as 0xE0 0x83 0xB1 instead of 0xC3 0xB1",
         {0},
         0}, // No custom options

        {"Valid 3-byte UTF-8 '€'",
         (unsigned char[]){0xE2, 0x82, 0xAC},
         3,
         1, // Should succeed
         "U+20AC (€) properly encoded as 0xE2 0x82 0xAC",
         {0},
         0}, // No custom options

        {"Overlong 4-byte encoding of '€'",
         (unsigned char[]){0xF0, 0x82, 0x82, 0xAC},
         4,
         0, // Should fail
         "U+20AC (€) encoded as 0xF0 0x82 0x82 0xAC instead of 0xE2 0x82 0xAC",
         {0},
         0}, // No custom options

        {"String with overlong sequence",
         (unsigned char[]){'h', 'e', 'l', 'l', 'o', 0xC0, 0x80, 'w', 'o', 'r', 'l', 'd'},
         12,
         0, // Should fail
         "Normal string with embedded overlong NUL - entire string should be rejected",
         {0},
         0}, // No custom options

        {"Mixed valid and overlong",
         (unsigned char[]){0xC3, 0xB1, 0xC0, 0xAF, 0x41},
         5,
         0, // Should fail
         "Valid ñ followed by overlong / and valid A - should reject entire input",
         {0},
         0}, // No custom options

        // ===== CUSTOM OPTIONS TESTS =====

        {"Overlong 'A' with preserve_case=1",
         (unsigned char[]){0xC1, 0x81},
         2,
         0, // Should still fail
         "Overlong encoding should be rejected regardless of preserve_case setting",
         {.separator = '-', .max_length = 0, .preserve_case = true},
         1}, // Custom options

        {"Overlong '/' with custom separator '_'",
         (unsigned char[]){0xC0, 0xAF},
         2,
         0, // Should still fail
         "Overlong encoding should be rejected regardless of separator choice",
         {.separator = '_', .max_length = 0, .preserve_case = false},
         1}, // Custom options

        {"Overlong NUL with max_length=5",
         (unsigned char[]){0xC0, 0x80},
         2,
         0, // Should still fail
         "Overlong encoding should be rejected regardless of max_length setting",
         {.separator = '-', .max_length = 5, .preserve_case = false},
         1}, // Custom options

        {"Valid 'Hello' with max_length=3",
         (unsigned char[]){'H', 'e', 'l', 'l', 'o'},
         5,
         1, // Should succeed (but truncated)
         "Valid string should work with max_length, result should be 'hel'",
         {.separator = '-', .max_length = 3, .preserve_case = false},
         1}, // Custom options

        {"Valid 'Hello' with preserve_case=1 and separator='_'",
         (unsigned char[]){'H', 'e', 'l', 'l', 'o'},
         5,
         1, // Should succeed
         "Valid string with custom options should work, result should be 'Hello'",
         {.separator = '_', .max_length = 0, .preserve_case = true},
         1}, // Custom options

        {"Overlong in 'Hello' + overlong '/' + 'World'",
         (unsigned char[]){'H', 'e', 'l', 'l', 'o', 0xC0, 0xAF, 'W', 'o', 'r', 'l', 'd'},
         12,
         0, // Should fail
         "Mixed string with overlong should be rejected with custom separator",
         {.separator = '_', .max_length = 20, .preserve_case = true},
         1}, // Custom options

        {"Multiple overlong sequences with all custom options",
         (unsigned char[]){0xC0, 0x80, 0xC1, 0x81, 0xE0, 0x81, 0x81},
         7,
         0, // Should fail
         "Multiple overlong sequences should be rejected regardless of options",
         {.separator = '|', .max_length = 100, .preserve_case = true},
         1}, // Custom options

        {"Valid UTF-8 'café' with preserve_case=1",
         (unsigned char[]){'c', 'a', 'f', 0xC3, 0xA9},
         5, // 'café' in UTF-8
         1, // Should succeed
         "Valid UTF-8 should work with preserve_case, no transliteration needed",
         {.separator = '-', .max_length = 0, .preserve_case = true},
         1}, // Custom options

        {"Valid UTF-8 'café' with preserve_case=0",
         (unsigned char[]){'c', 'a', 'f', 0xC3, 0xA9},
         5, // 'café' in UTF-8
         1, // Should succeed
         "Valid UTF-8 should work, might be transliterated to 'cafe'",
         {.separator = '-', .max_length = 0, .preserve_case = false},
         1} // Custom options
    };

    int total_tests = sizeof(tests) / sizeof(tests[0]);
    int passed_tests = 0;
    int default_tests = 0;
    int custom_tests = 0;
    int default_passed = 0;
    int custom_passed = 0;

    for (int i = 0; i < total_tests; i++)
    {
        if (test_slugify_overlong(&tests[i]))
        {
            passed_tests++;
            if (tests[i].use_custom_opts)
                custom_passed++;
            else
                default_passed++;
        }

        if (tests[i].use_custom_opts)
            custom_tests++;
        else
            default_tests++;
    }

    printf("\n=== FINAL RESULTS ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("  Default options tests: %d (passed: %d)\n", default_tests, default_passed);
    printf("  Custom options tests: %d (passed: %d)\n", custom_tests, custom_passed);
    printf("Overall passed: %d\n", passed_tests);
    printf("Overall failed: %d\n", total_tests - passed_tests);
    printf("Success rate: %.1f%%\n", (float)passed_tests / total_tests * 100.0f);

    if (passed_tests == total_tests)
    {
        printf("\nALL TESTS PASSED!\n");
        printf("The slugify() function correctly rejects overlong UTF-8 encodings\n");
        printf("with both default and custom options.\n");
        printf("This indicates good security against overlong encoding attacks.\n");
    }
    else
    {
        printf("\nSOME TESTS FAILED!\n");
        printf("The slugify() function may be vulnerable to overlong encoding attacks.\n");
        printf("Review the UTF-8 validation logic in the implementation.\n");

        if (default_passed != default_tests)
            printf("Default options tests failed: %d/%d\n", default_tests - default_passed, default_tests);
        if (custom_passed != custom_tests)
            printf("Custom options tests failed: %d/%d\n", custom_tests - custom_passed, custom_tests);
    }

    printf("\n=== SECURITY NOTES ===\n");
    printf("Overlong encodings can bypass security filters\n");
    printf("They're often used in path traversal attacks (/../)\n");
    printf("Can bypass string matching and validation\n");
    printf("A secure implementation must reject ALL overlong sequences\n");
    printf("Security should be consistent across all option combinations\n");
    printf("Custom options should not weaken overlong detection\n");

    return (passed_tests == total_tests) ? 0 : 1;
}
