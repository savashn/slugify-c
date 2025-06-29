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

int test_slugify_overlong(const overlong_test_t *test)
{
    printf("\n=== %s ===\n", test->test_name);
    printf("Description: %s\n", test->description);
    print_hex_bytes(test->input_bytes, test->input_len);

    // Create null-terminated string from bytes
    char *input_str = malloc(test->input_len + 1);
    memcpy(input_str, test->input_bytes, test->input_len);
    input_str[test->input_len] = '\0';

    // Test with default options
    slugify_options_t opts = {0};
    opts.flags = LOWERCASE;
    opts.separator = '-';
    opts.max_length = 0;

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

    // Test cases - these should all be REJECTED by a secure implementation
    overlong_test_t tests[] = {
        {"Valid ASCII 'A'",
         (unsigned char[]){0x41}, 1,
         1, // Should succeed
         "Normal ASCII character - baseline test"},
        {"Overlong 2-byte encoding of 'A'",
         (unsigned char[]){0xC1, 0x81}, 2,
         0, // Should fail
         "0x41 ('A') encoded as 0xC1 0x81 instead of 0x41"},
        {"Overlong 3-byte encoding of 'A'",
         (unsigned char[]){0xE0, 0x81, 0x81}, 3,
         0, // Should fail
         "0x41 ('A') encoded as 0xE0 0x81 0x81 instead of 0x41"},
        {"Overlong 4-byte encoding of 'A'",
         (unsigned char[]){0xF0, 0x80, 0x81, 0x81}, 4,
         0, // Should fail
         "0x41 ('A') encoded as 0xF0 0x80 0x81 0x81 instead of 0x41"},
        {"Overlong encoding of '/' (path traversal risk)",
         (unsigned char[]){0xC0, 0xAF}, 2,
         0, // Should fail
         "0x2F ('/') encoded as 0xC0 0xAF - used in path traversal attacks"},
        {"Overlong encoding of '.' (path traversal risk)",
         (unsigned char[]){0xC0, 0xAE}, 2,
         0, // Should fail
         "0x2E ('.') encoded as 0xC0 0xAE - used in path traversal attacks"},
        {"Overlong encoding of NUL byte",
         (unsigned char[]){0xC0, 0x80}, 2,
         0, // Should fail
         "0x00 (NUL) encoded as 0xC0 0x80 - can bypass string length checks"},
        {"Overlong encoding of space",
         (unsigned char[]){0xC0, 0xA0}, 2,
         0, // Should fail
         "0x20 (space) encoded as 0xC0 0xA0"},
        {"Valid 2-byte UTF-8 'ñ'",
         (unsigned char[]){0xC3, 0xB1}, 2,
         1, // Should succeed
         "U+00F1 (ñ) properly encoded as 0xC3 0xB1"},
        {"Overlong 3-byte encoding of 'ñ'",
         (unsigned char[]){0xE0, 0x83, 0xB1}, 3,
         0, // Should fail
         "U+00F1 (ñ) encoded as 0xE0 0x83 0xB1 instead of 0xC3 0xB1"},
        {"Valid 3-byte UTF-8 '€'",
         (unsigned char[]){0xE2, 0x82, 0xAC}, 3,
         1, // Should succeed
         "U+20AC (€) properly encoded as 0xE2 0x82 0xAC"},
        {"Overlong 4-byte encoding of '€'",
         (unsigned char[]){0xF0, 0x82, 0x82, 0xAC}, 4,
         0, // Should fail
         "U+20AC (€) encoded as 0xF0 0x82 0x82 0xAC instead of 0xE2 0x82 0xAC"},
        {"String with overlong sequence",
         (unsigned char[]){'h', 'e', 'l', 'l', 'o', 0xC0, 0x80, 'w', 'o', 'r', 'l', 'd'}, 12,
         0, // Should fail
         "Normal string with embedded overlong NUL - entire string should be rejected"},
        {"Mixed valid and overlong",
         (unsigned char[]){0xC3, 0xB1, 0xC0, 0xAF, 0x41}, 5,
         0, // Should fail
         "Valid ñ followed by overlong / and valid A - should reject entire input"}};

    int total_tests = sizeof(tests) / sizeof(tests[0]);
    int passed_tests = 0;

    for (int i = 0; i < total_tests; i++)
    {
        if (test_slugify_overlong(&tests[i]))
        {
            passed_tests++;
        }
    }

    printf("\n=== FINAL RESULTS ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    printf("Success rate: %.1f%%\n", (float)passed_tests / total_tests * 100.0f);

    if (passed_tests == total_tests)
    {
        printf("\n ALL TESTS PASSED!\n");
        printf("The slugify() function correctly rejects overlong UTF-8 encodings.\n");
        printf("This indicates good security against overlong encoding attacks.\n");
    }
    else
    {
        printf("\n SOME TESTS FAILED!\n");
        printf("The slugify() function may be vulnerable to overlong encoding attacks.\n");
        printf("Review the UTF-8 validation logic in the implementation.\n");
    }

    printf("\n=== SECURITY NOTES ===\n");
    printf("• Overlong encodings can bypass security filters\n");
    printf("• They're often used in path traversal attacks (/../)\n");
    printf("• Can bypass string matching and validation\n");
    printf("• A secure implementation must reject ALL overlong sequences\n");

    return (passed_tests == total_tests) ? 0 : 1;
}
