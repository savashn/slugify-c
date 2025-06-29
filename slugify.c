#include "slugify.h"
#include <string.h>
#include <ctype.h>

/* Platform-specific includes */
#ifdef _WIN32
#include <windows.h>
#include <winnls.h>
#endif

/* UTF-8 utility functions */
static int utf8_char_length(unsigned char c)
{
    if (c < 0x80)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return 1; /* Invalid UTF-8, treat as single byte */
}

/* Check for overlong encodings and invalid sequences */
static int is_overlong_encoding(const char *str, size_t char_len, uint32_t codepoint)
{
    switch (char_len)
    {
    case 2:
        /* 2-byte sequence should encode values >= 0x80 */
        return codepoint < 0x80;
    case 3:
        /* 3-byte sequence should encode values >= 0x800 */
        return codepoint < 0x800;
    case 4:
        /* 4-byte sequence should encode values >= 0x10000 */
        return codepoint < 0x10000;
    default:
        return 0;
    }
}

/* Secure UTF-8 decoder with overlong protection */
static uint32_t utf8_decode_secure(const char *str, size_t *consumed, int *is_valid)
{
    unsigned char c = (unsigned char)str[0];
    uint32_t codepoint = 0;
    size_t char_len = 0;

    *is_valid = 1;
    *consumed = 1;

    if (c < 0x80)
    {
        return c;
    }
    else if ((c & 0xE0) == 0xC0)
    {
        char_len = 2;
        codepoint = (c & 0x1F) << 6;
        if ((str[1] & 0xC0) != 0x80)
        {
            *is_valid = 0;
            return 0;
        }
        codepoint |= (str[1] & 0x3F);
        *consumed = 2;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        char_len = 3;
        codepoint = (c & 0x0F) << 12;
        if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80)
        {
            *is_valid = 0;
            return 0;
        }
        codepoint |= ((str[1] & 0x3F) << 6);
        codepoint |= (str[2] & 0x3F);
        *consumed = 3;
    }
    else if ((c & 0xF8) == 0xF0)
    {
        char_len = 4;
        codepoint = (c & 0x07) << 18;
        if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80 || (str[3] & 0xC0) != 0x80)
        {
            *is_valid = 0;
            return 0;
        }
        codepoint |= ((str[1] & 0x3F) << 12);
        codepoint |= ((str[2] & 0x3F) << 6);
        codepoint |= (str[3] & 0x3F);
        *consumed = 4;
    }
    else
    {
        *is_valid = 0;
        return 0;
    }

    /* Check for overlong encodings */
    if (is_overlong_encoding(str, char_len, codepoint))
    {
        *is_valid = 0;
        return 0;
    }

    /* Additional security checks */
    if (codepoint > 0x10FFFF ||                         /* Beyond Unicode range */
        (codepoint >= 0xD800 && codepoint <= 0xDFFF) || /* UTF-16 surrogates */
        (codepoint >= 0xFDD0 && codepoint <= 0xFDEF) || /* Non-characters */
        (codepoint & 0xFFFE) == 0xFFFE)
    { /* Non-characters */
        *is_valid = 0;
        return 0;
    }

    return codepoint;
}

static int is_utf8_valid(const char *str, size_t len)
{
    for (size_t i = 0; i < len;)
    {
        unsigned char c = (unsigned char)str[i];
        int char_len = utf8_char_length(c);

        if (i + char_len > len)
            return 0;

        uint32_t codepoint = 0;

        /* Validate continuation bytes and decode codepoint */
        if (char_len == 1)
        {
            codepoint = c;
        }
        else if (char_len == 2)
        {
            if ((str[i + 1] & 0xC0) != 0x80)
                return 0;
            codepoint = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
        }
        else if (char_len == 3)
        {
            if ((str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80)
                return 0;
            codepoint = ((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
        }
        else if (char_len == 4)
        {
            if ((str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80 || (str[i + 3] & 0xC0) != 0x80)
                return 0;
            codepoint = ((c & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) | ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
        }
        else
        {
            return 0; /* Invalid first byte */
        }

        /* Check for overlong encodings */
        if (is_overlong_encoding(&str[i], char_len, codepoint))
            return 0;

        /* Check for invalid Unicode ranges */
        if (codepoint > 0x10FFFF)
            return 0; /* Beyond valid Unicode range */

        /* Check for UTF-16 surrogates (invalid in UTF-8) */
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            return 0;

        /* Check for non-characters */
        if ((codepoint >= 0xFDD0 && codepoint <= 0xFDEF) ||
            (codepoint & 0xFFFE) == 0xFFFE)
            return 0;

        i += char_len;
    }
    return 1;
}

/* Comprehensive transliteration table */
static const struct
{
    uint32_t unicode;
    const char *ascii;
} transliteration_table[] = {
    /* Symbols */
    {0x24, "dollar"},
    {0x25, "percent"},
    {0x26, "and"},
    {0x3C, "less"},
    {0x3E, "greater"},
    {0x7C, "or"},
    {0xA2, "cent"},
    {0xA3, "pound"},
    {0xA4, "currency"},
    {0xA5, "yen"},
    {0xA9, "(c)"},
    {0xAA, "a"},
    {0xAE, "(r)"},
    {0xBA, "o"},

    /* Latin Extended */
    {0xC0, "A"},
    {0xC1, "A"},
    {0xC2, "A"},
    {0xC3, "A"},
    {0xC4, "A"},
    {0xC5, "A"}, /* À-Å */
    {0xC6, "AE"},
    {0xC7, "C"}, /* Æ, Ç */
    {0xC8, "E"},
    {0xC9, "E"},
    {0xCA, "E"},
    {0xCB, "E"}, /* È-Ë */
    {0xCC, "I"},
    {0xCD, "I"},
    {0xCE, "I"},
    {0xCF, "I"}, /* Ì-Ï */
    {0xD0, "D"},
    {0xD1, "N"}, /* Ð, Ñ */
    {0xD2, "O"},
    {0xD3, "O"},
    {0xD4, "O"},
    {0xD5, "O"},
    {0xD6, "O"},
    {0xD8, "O"}, /* Ò-Ö, Ø */
    {0xD9, "U"},
    {0xDA, "U"},
    {0xDB, "U"},
    {0xDC, "U"}, /* Ù-Ü */
    {0xDD, "Y"},
    {0xDE, "TH"},
    {0xDF, "ss"}, /* Ý, Þ, ß */

    /* Lowercase versions */
    {0xE0, "a"},
    {0xE1, "a"},
    {0xE2, "a"},
    {0xE3, "a"},
    {0xE4, "a"},
    {0xE5, "a"},
    {0xE6, "ae"},
    {0xE7, "c"},
    {0xE8, "e"},
    {0xE9, "e"},
    {0xEA, "e"},
    {0xEB, "e"},
    {0xEC, "i"},
    {0xED, "i"},
    {0xEE, "i"},
    {0xEF, "i"},
    {0xF0, "d"},
    {0xF1, "n"},
    {0xF2, "o"},
    {0xF3, "o"},
    {0xF4, "o"},
    {0xF5, "o"},
    {0xF6, "o"},
    {0xF8, "o"},
    {0xF9, "u"},
    {0xFA, "u"},
    {0xFB, "u"},
    {0xFC, "u"},
    {0xFD, "y"},
    {0xFE, "th"},
    {0xFF, "y"},

    /* Extended Latin A */
    {0x100, "A"},
    {0x101, "a"},
    {0x102, "A"},
    {0x103, "a"},
    {0x104, "A"},
    {0x105, "a"},
    {0x106, "C"},
    {0x107, "c"},
    {0x10C, "C"},
    {0x10D, "c"},
    {0x10E, "D"},
    {0x10F, "d"},
    {0x110, "DJ"},
    {0x111, "dj"},
    {0x112, "E"},
    {0x113, "e"},
    {0x116, "E"},
    {0x117, "e"},
    {0x118, "e"},
    {0x119, "e"},
    {0x11A, "E"},
    {0x11B, "e"},
    {0x11E, "G"},
    {0x11F, "g"},
    {0x122, "G"},
    {0x123, "g"},
    {0x128, "I"},
    {0x129, "i"},
    {0x12A, "i"},
    {0x12B, "i"},
    {0x12E, "I"},
    {0x12F, "i"},
    {0x130, "I"},
    {0x131, "i"},
    {0x136, "k"},
    {0x137, "k"},
    {0x13B, "L"},
    {0x13C, "l"},
    {0x13D, "L"},
    {0x13E, "l"},
    {0x141, "L"},
    {0x142, "l"},
    {0x143, "N"},
    {0x144, "n"},
    {0x145, "N"},
    {0x146, "n"},
    {0x147, "N"},
    {0x148, "n"},
    {0x14C, "O"},
    {0x14D, "o"},
    {0x150, "O"},
    {0x151, "o"},
    {0x152, "OE"},
    {0x153, "oe"},
    {0x154, "R"},
    {0x155, "r"},
    {0x158, "R"},
    {0x159, "r"},
    {0x15A, "S"},
    {0x15B, "s"},
    {0x15E, "S"},
    {0x15F, "s"},
    {0x160, "S"},
    {0x161, "s"},
    {0x162, "T"},
    {0x163, "t"},
    {0x164, "T"},
    {0x165, "t"},
    {0x168, "U"},
    {0x169, "u"},
    {0x16A, "u"},
    {0x16B, "u"},
    {0x16E, "U"},
    {0x16F, "u"},
    {0x170, "U"},
    {0x171, "u"},
    {0x172, "U"},
    {0x173, "u"},
    {0x174, "W"},
    {0x175, "w"},
    {0x176, "Y"},
    {0x177, "y"},
    {0x178, "Y"},
    {0x179, "Z"},
    {0x17A, "z"},
    {0x17B, "Z"},
    {0x17C, "z"},
    {0x17D, "Z"},
    {0x17E, "z"},

    /* Extended Latin B */
    {0x18F, "E"},
    {0x192, "f"},
    {0x1A0, "O"},
    {0x1A1, "o"},
    {0x1AF, "U"},
    {0x1B0, "u"},
    {0x1C8, "LJ"},
    {0x1C9, "lj"},
    {0x1CB, "NJ"},
    {0x1CC, "nj"},
    {0x218, "S"},
    {0x219, "s"},
    {0x21A, "T"},
    {0x21B, "t"},
    {0x259, "e"},
    {0x2DA, "o"},

    /* Greek */
    {0x386, "A"},
    {0x388, "E"},
    {0x389, "H"},
    {0x38A, "I"},
    {0x38C, "O"},
    {0x38E, "Y"},
    {0x38F, "W"},
    {0x390, "i"},
    {0x391, "A"},
    {0x392, "B"},
    {0x393, "G"},
    {0x394, "D"},
    {0x395, "E"},
    {0x396, "Z"},
    {0x397, "H"},
    {0x398, "8"},
    {0x399, "I"},
    {0x39A, "K"},
    {0x39B, "L"},
    {0x39C, "M"},
    {0x39D, "N"},
    {0x39E, "3"},
    {0x39F, "O"},
    {0x3A0, "P"},
    {0x3A1, "R"},
    {0x3A3, "S"},
    {0x3A4, "T"},
    {0x3A5, "Y"},
    {0x3A6, "F"},
    {0x3A7, "X"},
    {0x3A8, "PS"},
    {0x3A9, "W"},
    {0x3AA, "I"},
    {0x3AB, "Y"},
    {0x3AC, "a"},
    {0x3AD, "e"},
    {0x3AE, "h"},
    {0x3AF, "i"},
    {0x3B0, "y"},
    {0x3B1, "a"},
    {0x3B2, "b"},
    {0x3B3, "g"},
    {0x3B4, "d"},
    {0x3B5, "e"},
    {0x3B6, "z"},
    {0x3B7, "h"},
    {0x3B8, "8"},
    {0x3B9, "i"},
    {0x3BA, "k"},
    {0x3BB, "l"},
    {0x3BC, "m"},
    {0x3BD, "n"},
    {0x3BE, "3"},
    {0x3BF, "o"},
    {0x3C0, "p"},
    {0x3C1, "r"},
    {0x3C2, "s"},
    {0x3C3, "s"},
    {0x3C4, "t"},
    {0x3C5, "y"},
    {0x3C6, "f"},
    {0x3C7, "x"},
    {0x3C8, "ps"},
    {0x3C9, "w"},
    {0x3CA, "i"},
    {0x3CB, "y"},
    {0x3CC, "o"},
    {0x3CD, "y"},
    {0x3CE, "w"},

    /* Cyrillic */
    {0x401, "Yo"},
    {0x402, "DJ"},
    {0x404, "Ye"},
    {0x406, "I"},
    {0x407, "Yi"},
    {0x408, "J"},
    {0x409, "LJ"},
    {0x40A, "NJ"},
    {0x40B, "C"},
    {0x40F, "DZ"},
    {0x410, "A"},
    {0x411, "B"},
    {0x412, "V"},
    {0x413, "G"},
    {0x414, "D"},
    {0x415, "E"},
    {0x416, "Zh"},
    {0x417, "Z"},
    {0x418, "I"},
    {0x419, "J"},
    {0x41A, "K"},
    {0x41B, "L"},
    {0x41C, "M"},
    {0x41D, "N"},
    {0x41E, "O"},
    {0x41F, "P"},
    {0x420, "R"},
    {0x421, "S"},
    {0x422, "T"},
    {0x423, "U"},
    {0x424, "F"},
    {0x425, "H"},
    {0x426, "C"},
    {0x427, "Ch"},
    {0x428, "Sh"},
    {0x429, "Sh"},
    {0x42A, "U"},
    {0x42B, "Y"},
    {0x42C, ""},
    {0x42D, "E"},
    {0x42E, "Yu"},
    {0x42F, "Ya"},
    {0x430, "a"},
    {0x431, "b"},
    {0x432, "v"},
    {0x433, "g"},
    {0x434, "d"},
    {0x435, "e"},
    {0x436, "zh"},
    {0x437, "z"},
    {0x438, "i"},
    {0x439, "j"},
    {0x43A, "k"},
    {0x43B, "l"},
    {0x43C, "m"},
    {0x43D, "n"},
    {0x43E, "o"},
    {0x43F, "p"},
    {0x440, "r"},
    {0x441, "s"},
    {0x442, "t"},
    {0x443, "u"},
    {0x444, "f"},
    {0x445, "h"},
    {0x446, "c"},
    {0x447, "ch"},
    {0x448, "sh"},
    {0x449, "sh"},
    {0x44A, "u"},
    {0x44B, "y"},
    {0x44C, ""},
    {0x44D, "e"},
    {0x44E, "yu"},
    {0x44F, "ya"},
    {0x451, "yo"},
    {0x452, "dj"},
    {0x454, "ye"},
    {0x456, "i"},
    {0x457, "yi"},
    {0x458, "j"},
    {0x459, "lj"},
    {0x45A, "nj"},
    {0x45B, "c"},
    {0x45D, "u"},
    {0x45F, "dz"},
    {0x490, "G"},
    {0x491, "g"},
    {0x492, "GH"},
    {0x493, "gh"},
    {0x49A, "KH"},
    {0x49B, "kh"},
    {0x4A2, "NG"},
    {0x4A3, "ng"},
    {0x4AE, "UE"},
    {0x4AF, "ue"},
    {0x4B0, "U"},
    {0x4B1, "u"},
    {0x4BA, "H"},
    {0x4BB, "h"},
    {0x4D8, "AE"},
    {0x4D9, "ae"},
    {0x4E8, "OE"},
    {0x4E9, "oe"},

    /* Arabic */
    {0x621, "a"},
    {0x622, "aa"},
    {0x623, "a"},
    {0x624, "u"},
    {0x625, "i"},
    {0x626, "e"},
    {0x627, "a"},
    {0x628, "b"},
    {0x629, "h"},
    {0x62A, "t"},
    {0x62B, "th"},
    {0x62C, "j"},
    {0x62D, "h"},
    {0x62E, "kh"},
    {0x62F, "d"},
    {0x630, "th"},
    {0x631, "r"},
    {0x632, "z"},
    {0x633, "s"},
    {0x634, "sh"},
    {0x635, "s"},
    {0x636, "dh"},
    {0x637, "t"},
    {0x638, "z"},
    {0x639, "a"},
    {0x63A, "gh"},
    {0x641, "f"},
    {0x642, "q"},
    {0x643, "k"},
    {0x644, "l"},
    {0x645, "m"},
    {0x646, "n"},
    {0x647, "h"},
    {0x648, "w"},
    {0x649, "a"},
    {0x64A, "y"},
    {0x64B, "an"},
    {0x64C, "on"},
    {0x64D, "en"},
    {0x64E, "a"},
    {0x64F, "u"},
    {0x650, "e"},
    {0x651, ""},
    {0x660, "0"},
    {0x661, "1"},
    {0x662, "2"},
    {0x663, "3"},
    {0x664, "4"},
    {0x665, "5"},
    {0x666, "6"},
    {0x667, "7"},
    {0x668, "8"},
    {0x669, "9"},
    {0x67E, "p"},
    {0x686, "ch"},
    {0x698, "zh"},
    {0x6A9, "k"},
    {0x6AF, "g"},
    {0x6CC, "y"},
    {0x6F0, "0"},
    {0x6F1, "1"},
    {0x6F2, "2"},
    {0x6F3, "3"},
    {0x6F4, "4"},
    {0x6F5, "5"},
    {0x6F6, "6"},
    {0x6F7, "7"},
    {0x6F8, "8"},
    {0x6F9, "9"},

    /* Georgian */
    {0x10D0, "a"},
    {0x10D1, "b"},
    {0x10D2, "g"},
    {0x10D3, "d"},
    {0x10D4, "e"},
    {0x10D5, "v"},
    {0x10D6, "z"},
    {0x10D7, "t"},
    {0x10D8, "i"},
    {0x10D9, "k"},
    {0x10DA, "l"},
    {0x10DB, "m"},
    {0x10DC, "n"},
    {0x10DD, "o"},
    {0x10DE, "p"},
    {0x10DF, "zh"},
    {0x10E0, "r"},
    {0x10E1, "s"},
    {0x10E2, "t"},
    {0x10E3, "u"},
    {0x10E4, "f"},
    {0x10E5, "k"},
    {0x10E6, "gh"},
    {0x10E7, "q"},
    {0x10E8, "sh"},
    {0x10E9, "ch"},
    {0x10EA, "ts"},
    {0x10EB, "dz"},
    {0x10EC, "ts"},
    {0x10ED, "ch"},
    {0x10EE, "kh"},
    {0x10EF, "j"},
    {0x10F0, "h"},

    /* Vietnamese Extended */
    {0x1EA0, "A"},
    {0x1EA1, "a"},
    {0x1EA2, "A"},
    {0x1EA3, "a"},
    {0x1EA4, "A"},
    {0x1EA5, "a"},
    {0x1EA6, "A"},
    {0x1EA7, "a"},
    {0x1EA8, "A"},
    {0x1EA9, "a"},
    {0x1EAA, "A"},
    {0x1EAB, "a"},
    {0x1EAC, "A"},
    {0x1EAD, "a"},
    {0x1EAE, "A"},
    {0x1EAF, "a"},
    {0x1EB0, "A"},
    {0x1EB1, "a"},
    {0x1EB2, "A"},
    {0x1EB3, "a"},
    {0x1EB4, "A"},
    {0x1EB5, "a"},
    {0x1EB6, "A"},
    {0x1EB7, "a"},
    {0x1EB8, "E"},
    {0x1EB9, "e"},
    {0x1EBA, "E"},
    {0x1EBB, "e"},
    {0x1EBC, "E"},
    {0x1EBD, "e"},
    {0x1EBE, "E"},
    {0x1EBF, "e"},
    {0x1EC0, "E"},
    {0x1EC1, "e"},
    {0x1EC2, "E"},
    {0x1EC3, "e"},
    {0x1EC4, "E"},
    {0x1EC5, "e"},
    {0x1EC6, "E"},
    {0x1EC7, "e"},
    {0x1EC8, "I"},
    {0x1EC9, "i"},
    {0x1ECA, "I"},
    {0x1ECB, "i"},
    {0x1ECC, "O"},
    {0x1ECD, "o"},
    {0x1ECE, "O"},
    {0x1ECF, "o"},
    {0x1ED0, "O"},
    {0x1ED1, "o"},
    {0x1ED2, "O"},
    {0x1ED3, "o"},
    {0x1ED4, "O"},
    {0x1ED5, "o"},
    {0x1ED6, "O"},
    {0x1ED7, "o"},
    {0x1ED8, "O"},
    {0x1ED9, "o"},
    {0x1EDA, "O"},
    {0x1EDB, "o"},
    {0x1EDC, "O"},
    {0x1EDD, "o"},
    {0x1EDE, "O"},
    {0x1EDF, "o"},
    {0x1EE0, "O"},
    {0x1EE1, "o"},
    {0x1EE2, "O"},
    {0x1EE3, "o"},
    {0x1EE4, "U"},
    {0x1EE5, "u"},
    {0x1EE6, "U"},
    {0x1EE7, "u"},
    {0x1EE8, "U"},
    {0x1EE9, "u"},
    {0x1EEA, "U"},
    {0x1EEB, "u"},
    {0x1EEC, "U"},
    {0x1EED, "u"},
    {0x1EEE, "U"},
    {0x1EEF, "u"},
    {0x1EF0, "U"},
    {0x1EF1, "u"},
    {0x1EF2, "Y"},
    {0x1EF3, "y"},
    {0x1EF4, "Y"},
    {0x1EF5, "y"},
    {0x1EF6, "Y"},
    {0x1EF7, "y"},
    {0x1EF8, "Y"},
    {0x1EF9, "y"},

    /* Punctuation and symbols */
    {0x2013, "-"},
    {0x2018, "'"},
    {0x2019, "'"},
    {0x201C, "\""},
    {0x201D, "\""},
    {0x201E, "\""},
    {0x2020, "+"},
    {0x2022, "*"},
    {0x2026, "..."},
    {0x20A0, "ecu"},
    {0x20A2, "cruzeiro"},
    {0x20A3, "french franc"},
    {0x20A4, "lira"},
    {0x20A5, "mill"},
    {0x20A6, "naira"},
    {0x20A7, "peseta"},
    {0x20A8, "rupee"},
    {0x20A9, "won"},
    {0x20AA, "new shequel"},
    {0x20AB, "dong"},
    {0x20AC, "euro"},
    {0x20AD, "kip"},
    {0x20AE, "tugrik"},
    {0x20AF, "drachma"},
    {0x20B0, "penny"},
    {0x20B1, "peso"},
    {0x20B2, "guarani"},
    {0x20B3, "austral"},
    {0x20B4, "hryvnia"},
    {0x20B5, "cedi"},
    {0x20B8, "kazakhstani tenge"},
    {0x20B9, "indian rupee"},
    {0x20BA, "turkish lira"},
    {0x20BD, "russian ruble"},
    {0x20BF, "bitcoin"},
    {0x2120, "sm"},
    {0x2122, "tm"},
    {0x2202, "d"},
    {0x2206, "delta"},
    {0x2211, "sum"},
    {0x221E, "infinity"},
    {0x2665, "love"},
    {0x5143, "yuan"},
    {0x5186, "yen"},
    {0xFDFC, "rial"},
    {0xFDF5, "laa"},
    {0xFDF7, "laa"},
    {0xFDF9, "lai"},
    {0xFDFB, "la"},

    {0, NULL} /* End marker */
};

/* Binary search in transliteration table */
static const char *transliterate_char(uint32_t codepoint)
{
    int lo = 0, hi = (sizeof(transliteration_table) / sizeof(transliteration_table[0])) - 2;
    while (lo <= hi)
    {
        int mid = (lo + hi) >> 1;
        uint32_t u = transliteration_table[mid].unicode;
        if (u == codepoint)
            return transliteration_table[mid].ascii;
        if (u < codepoint)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return NULL;
}

static uint32_t utf8_decode(const char *str, size_t *consumed)
{
    unsigned char c = (unsigned char)str[0];
    uint32_t codepoint = 0;

    if (c < 0x80)
    {
        *consumed = 1;
        return c;
    }
    else if ((c & 0xE0) == 0xC0)
    {
        *consumed = 2;
        codepoint = (c & 0x1F) << 6;
        codepoint |= (str[1] & 0x3F);
    }
    else if ((c & 0xF0) == 0xE0)
    {
        *consumed = 3;
        codepoint = (c & 0x0F) << 12;
        codepoint |= ((str[1] & 0x3F) << 6);
        codepoint |= (str[2] & 0x3F);
    }
    else if ((c & 0xF8) == 0xF0)
    {
        *consumed = 4;
        codepoint = (c & 0x07) << 18;
        codepoint |= ((str[1] & 0x3F) << 12);
        codepoint |= ((str[2] & 0x3F) << 6);
        codepoint |= (str[3] & 0x3F);
    }
    else
    {
        *consumed = 1;
        return c; /* Invalid UTF-8 */
    }

    return codepoint;
}

static slugify_options_t slugify_default_options(void)
{
    slugify_options_t opts = {0};
    opts.preserve_case = false;
    opts.separator = '-';
    opts.max_length = 0;
    return opts;
}

static size_t slugify_length(const char *input, const slugify_options_t *options)
{
    if (!input)
        return 0;

    slugify_options_t opts = options ? *options : slugify_default_options();
    size_t estimated = 0;

    for (size_t i = 0; input[i] != '\0';)
    {
        size_t consumed;
        uint32_t codepoint = utf8_decode(&input[i], &consumed);

        if (codepoint < 128)
        {
            char c = (char)codepoint;
            if (isalnum(c))
            {
                estimated++;
            }
            else
            {
                const char *trans = transliterate_char(codepoint);
                if (trans)
                {
                    estimated += strlen(trans);
                }
                else if (isspace((unsigned char)c) || ispunct((unsigned char)c))
                {
                    estimated++;
                }
            }
        }
        else if (opts.preserve_case)
        {
            estimated += consumed;
        }
        else
        {
            const char *trans = transliterate_char(codepoint);
            if (trans)
            {
                estimated += strlen(trans);
            }
        }

        i += consumed;
    }

    return estimated + 1; /* +1 for null terminator */
}

static int slugify_ex(const char *input, char *output, size_t out_size,
                      const slugify_options_t *options)
{
    if (!input || !output || out_size == 0)
    {
        return SLUGIFY_ERROR_INVALID;
    }

    slugify_options_t opts = options ? *options : slugify_default_options();
    size_t j = 0; // output index

    size_t input_len = strlen(input);
    if (!is_utf8_valid(input, input_len))
    {
        return SLUGIFY_ERROR_INVALID;
    }

    for (size_t i = 0; input[i] != '\0' && j < out_size - 1;)
    {
        size_t consumed = 0;
        uint32_t codepoint = utf8_decode(&input[i], &consumed);

        if (opts.max_length > 0 && j >= opts.max_length)
            break;

        if (codepoint < 128)
        {
            char c = (char)codepoint;

            if (isalnum((unsigned char)c))
            {
                char out_char = (opts.preserve_case) ? c : (char)tolower((unsigned char)c);

                if (j + 1 >= out_size)
                    return SLUGIFY_ERROR_BUFFER;

                output[j++] = out_char;
            }
            else
            {
                const char *trans = transliterate_char(codepoint);
                if (trans)
                {
                    size_t trans_len = strlen(trans);
                    if (j + trans_len >= out_size)
                        return SLUGIFY_ERROR_BUFFER;

                    for (size_t k = 0; k < trans_len; k++)
                    {
                        char c2 = (opts.preserve_case) ? trans[k] : (char)tolower((unsigned char)trans[k]);
                        output[j++] = c2;

                        if (opts.max_length > 0 && j >= opts.max_length)
                            break;
                    }
                }
                else if (isspace((unsigned char)c) || c == '-' || c == '_' || ispunct((unsigned char)c))
                {
                    // Collapse multiple separators into one
                    if (j > 0 && output[j - 1] != opts.separator)
                    {
                        if (j + 1 >= out_size)
                            return SLUGIFY_ERROR_BUFFER;
                        output[j++] = opts.separator;
                    }
                }
                // Ignore other punctuation characters without transliteration
            }
        }
        else
        {
            // Non-ASCII characters

            if (opts.preserve_case)
            {
                // Copy UTF-8 bytes directly
                if (j + consumed >= out_size)
                    return SLUGIFY_ERROR_BUFFER;

                for (size_t k = 0; k < consumed; k++)
                {
                    output[j++] = input[i + k];
                }
            }
            else
            {
                // Attempt to transliterate
                const char *trans = transliterate_char(codepoint);
                if (trans)
                {
                    size_t trans_len = strlen(trans);
                    if (j + trans_len >= out_size)
                        return SLUGIFY_ERROR_BUFFER;

                    for (size_t k = 0; k < trans_len; k++)
                    {
                        char c2 = (opts.preserve_case) ? trans[k] : (char)tolower((unsigned char)trans[k]);
                        output[j++] = c2;

                        if (opts.max_length > 0 && j >= opts.max_length)
                            break;
                    }
                }
                // If no transliteration, skip the character (do not add anything)
            }
        }

        i += consumed;
    }

    // Remove trailing separator if present
    if (j > 0 && output[j - 1] == opts.separator)
    {
        j--;
    }

    if (j == 0)
    {
        return SLUGIFY_ERROR_EMPTY;
    }

    output[j] = '\0';
    return SLUGIFY_SUCCESS;
}

char *slugify(const char *input, const slugify_options_t *options)
{
    // If options is NULL, use default options
    slugify_options_t opts = options ? *options : slugify_default_options();

    // Calculate the required buffer length
    size_t len = slugify_length(input, &opts);
    if (len == 0)
        return NULL;

    // Allocate the buffer
    char *buf = malloc(len);
    if (!buf)
        return NULL;

    // Generate the slug
    int rc = slugify_ex(input, buf, len, &opts);
    if (rc != SLUGIFY_SUCCESS)
    {
        free(buf);
        return NULL;
    }

    // Return the buffer on success
    return buf;
}
