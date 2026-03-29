// Copyright 2025 Nigel Tao.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

// --------

/// clloverview is a command-line tool that prints an overview (a table of
/// contents) of source code written in C-like languages.

// --------

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "./generated.h"

// --------

// Uncomment any or all of these lines to dump debug information to stderr.

// #define DEBUG_DUMP_DEFINES
// #define DEBUG_DUMP_HASH_TABLE
// #define DEBUG_DUMP_STRINGS
// #define DEBUG_DUMP_TOKENS

// --------

typedef const char* error_message_t;

#define TRY(expr)                   \
  do {                              \
    error_message_t try_err = expr; \
    if (try_err != NULL) {          \
      return try_err;               \
    }                               \
  } while (0)

// clang-format off

static error_message_t err_ana_toomancb = "analyze: too many { curly brackets";
static error_message_t err_ana_unbalacb = "analyze: unbalanced } curly bracket";
static error_message_t err_com_namelong = "command line: input filename is too long";
static error_message_t err_com_unsuflag = "command line: unsupported flag";
static error_message_t err_com_unsuname = "command line: unsupported input filename";
static error_message_t err_int_badargum = "internal: bad argument";
static error_message_t err_int_endoffil = "internal: end of file";
static error_message_t err_int_notaraws = "internal: not a raw string";
static error_message_t err_pre_baddirec = "preprocess: bad directive";
static error_message_t err_pre_badmacde = "preprocess: bad macro definition";
static error_message_t err_pre_badmacun = "preprocess: bad macro undefinition";
static error_message_t err_pre_linewsnt = "preprocess: line was not terminated";
static error_message_t err_pre_macroatl = "preprocess: macros are too large";
static error_message_t err_pre_misdirec = "preprocess: missing directive";
static error_message_t err_tok_backswnt = "tokenize: backslash was not terminated";
static error_message_t err_tok_itlbytes = "tokenize: input is too large (in terms of bytes)";
static error_message_t err_tok_itllines = "tokenize: input is too large (in terms of lines)";
static error_message_t err_tok_itlnestd = "tokenize: input is too large (in terms of nesting depth)";
static error_message_t err_tok_itltoken = "tokenize: input is too large (in terms of tokens)";
static error_message_t err_tok_itluniqs = "tokenize: input is too large (in terms of unique strings)";
static error_message_t err_tok_nulbytee = "tokenize: NUL byte encountered";
static error_message_t err_tok_quotewem = "tokenize: quoted literal was empty";
static error_message_t err_tok_quotewnt = "tokenize: quoted literal was not terminated";
static error_message_t err_tok_slashwnt = "tokenize: slash-star comment was not terminated";
static error_message_t err_tok_tokistol = "tokenize: token is too long";
static error_message_t err_tok_unsuunic = "tokenize: unsupported Unicode input";

// clang-format on

// --------

static const char* hex_digits = "0123456789ABCDEF";

static const uint8_t alpha_numeric_lut[256] = {
    /// Bit 0x01 means ASCII alphabetic or underscore.
    ///
    /// Bit 0x02 means ASCII digit.
    ///
    /// Bit 0x04 means 'A'-'F' or 'a'-'f'.
    ///
    /// Bit 0x08 means single quote or underscore, which can be found inside
    /// numeric literals in various languages. For example, 123'456 or 123_456.

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0,  // '\''
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,  // '0'-'9'

    0, 5, 5, 5, 5, 5, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 'A'-'O'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 9,  // 'P'-'Z', '_'
    0, 5, 5, 5, 5, 5, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 'a'-'o'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  // 'p'-'z'

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
};

static const uint8_t utf8_length_lut[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //

    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //

    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  //
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  //
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  //
    4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0,  //
};

// --------

static inline uint32_t  //
min_u32(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}

static inline uint16_t  //
peek_u16le(const void* ptr) {
  const uint8_t* p = (const uint8_t*)ptr;
  return (((uint16_t)p[0]) << 0) | (((uint16_t)p[1]) << 8);
}

static inline void  //
poke_u16le(void* ptr, uint16_t x) {
  uint8_t* p = (uint8_t*)ptr;
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
}

// --------

static inline uint32_t  //
count_utf8_runes(const char* s_ptr, size_t s_len) {
  /// Returns the number of runes (Unicode Code Points) in s. It assumes that s
  /// is valid UTF-8.

  uint32_t n = 0;
  while (s_len--) {
    if (0x80 != (0xC0 & *s_ptr++)) {
      n++;
    }
  }
  return n;
}

static inline uint64_t  //
decode_utf8_rune(const char* s_ptr, const char* s_end) {
  /// Returns the next UTF-8 rune (Unicode Code Point) in s. Invalid UTF-8
  /// produces U+FFFD REPLACEMENT CHARACTER.
  ///
  /// The uint64_t value returned packs two uint32_t values:
  ///
  /// - The high uint32_t is the rune length in bytes.
  /// - The low  uint32_t is the rune value.
  ///
  /// For implementation simplicity, it allows UTF-8 overlong encodings. It
  /// allows a superset of valid UTF-8.

  if (s_ptr >= s_end) {
    return 0x00000FFFDul;
  }
  uint32_t v = 0xFF & *s_ptr++;
  uint32_t n = utf8_length_lut[v];
  if (n == 1u) {
    return 0x100000000ul | ((uint64_t)(v));
  } else if (n < 1u) {
    return 0x10000FFFDul;
  }

  static const uint32_t masks[5] = {0x00, 0x00, 0x1F, 0x0F, 0x07};
  v &= masks[n];
  for (uint32_t i = 1u; i < n; i++) {
    if (s_ptr >= s_end) {
      return (((uint64_t)(i + 0u)) << 32) | 0xFFFDul;
    } else if (0x80 != (0xC0 & *s_ptr)) {
      return (((uint64_t)(i + 1u)) << 32) | 0xFFFDul;
    }
    v = (v << 6) | ((uint32_t)(0x3F & *s_ptr++));
  }
  return (((uint64_t)(n)) << 32) | ((uint64_t)(v));
}

static inline bool  //
is_namey_rune(uint8_t mask, uint32_t rune) {
  /// Returns whether the rune (the Unicode Code Point) is:
  ///
  /// - alphabetic          or an underscore, if mask is 0x01.
  /// - alphabetic, numeric or an underscore, if mask is 0x03.
  ///
  /// Numeric means '0' ..= '9'. Underscore means '_'. Alphabetic means 'A' ..=
  /// 'Z' or 'a' ..= 'z' or "a non-ASCII letter".
  ///
  /// "A non-ASCII Letter" should, strictly speaking, mean "a non-ASCII rune
  /// with the Unicode category [L]". But, for implementation simplicity, we
  /// approximate this as the union of certain Unicode ranges:
  ///
  /// U+00C0 ..= U+02AF covers these Unicode blocks:
  ///
  /// - Latin-1 Supplement (U+00C0 or higher)
  /// - Latin Extended-A
  /// - Latin Extended-B
  /// - IPA Extensions
  ///
  /// U+0370 ..= U+04FF covers these Unicode blocks:
  ///
  /// - Greek and Coptic
  /// - Cyrillic
  ///
  /// U+1E00 ..= U+1EFF covers these Unicode blocks:
  ///
  /// - Latin Extended Additional

  if (rune < 0x0080u) {
    return mask & alpha_numeric_lut[rune];
  }
  return ((0x00C0u <= rune) && (rune <= 0x02AFu)) ||
         ((0x0370u <= rune) && (rune <= 0x04FFu)) ||
         ((0x1E00u <= rune) && (rune <= 0x1EFFu));
}

// --------

typedef uint32_t token_t;
/// The bit-packing:
///
/// - The highest 1 bit  is  the major category (0 = indirect, 1 = direct).
/// - The next    1 bit  is  the minor category.
/// - The middle  9 bits are the token's string representation's length.
/// - The low    21 bits are the payload.
///
/// The high two bits combine to give four categories:
///
/// - 0x0???????u is an integery token, like "45".
/// - 0x4???????u is a  namey token, like "foobar".
/// - 0x8???????u is a  punctuation token, like "=".
/// - 0xC???????u is a  macro argument token, like "the 2nd macro argument".
///
/// The maximum (inclusive) token's string representation's length is 511, not
/// counting a trailing NUL byte (if stored in g_string_contents). This is the
/// length of the string used when printing the token, not when parsing the
/// token. The two are often the same but for macro arguments, when parsing
/// "#define apply_twice(f,x) f(f(x))", the later "f" and "x" instances are
/// parsed as "the 1st macro argument" or "the 2nd macro argument", whose
/// string forms when printing ("¤000000", "¤000001") are both 8 bytes long.
///
/// An indirect (integery or namey) token's payload is an offset into the
/// g_string_contents array.
///
/// A punctuation token's payload holds up to three 7-bit ASCII characters. The
/// first character is in the lowest 7 bits.
///
/// A macro argument token's payload is the N in "the Nth macro argument".
///
/// 0x00000000u is an invalid token, because a token must have positive length.
/// 0xFFFFFFFFu is also an invalid token, because a macro argument token's
/// length must be 8, e.g. "¤234567".

#define HIGH_11_BITS_MASK 0xFFE00000u
#define LOW_21_BITS_MASK 0x001FFFFFu

static token_t  //
make_macro_argument_token(uint32_t i) {
  return ((uint32_t)(LOW_21_BITS_MASK & i)) | 0xC1000000u;
}

static token_t  //
make_one_byte_punctuation_token(char c) {
  return ((uint32_t)(0x7F & c)) | 0x80200000u;
}

static bool  //
token_is_indirect(token_t t) {
  return (t >> 31) == 0u;
}

static bool  //
token_is_namey(token_t t) {
  return (t >> 30) == 1u;
}

static uint32_t  //
token_length(token_t t) {
  return 0x1FFu & (t >> 21);
}

// clang-format off

#define TOKEN_FOR_U000A_LINE_FEED               0x8020000Au
#define TOKEN_FOR_U0023_NUMBER_SIGN             0x80200023u
#define TOKEN_FOR_U0026_AMPERSAND               0x80200026u
#define TOKEN_FOR_U0028_LEFT_PARENTHESIS        0x80200028u
#define TOKEN_FOR_U0029_RIGHT_PARENTHESIS       0x80200029u
#define TOKEN_FOR_U002A_ASTERISK                0x8020002Au
#define TOKEN_FOR_U002C_COMMA                   0x8020002Cu
#define TOKEN_FOR_U0027_APOSTROPHE              0x80200027u
#define TOKEN_FOR_U002E_FULL_STOP               0x8020002Eu
#define TOKEN_FOR_U003A_COLON                   0x8020003Au
#define TOKEN_FOR_U003B_SEMICOLON               0x8020003Bu
#define TOKEN_FOR_U003C_LESS_THAN_SIGN          0x8020003Cu
#define TOKEN_FOR_U003D_EQUALS_SIGN             0x8020003Du
#define TOKEN_FOR_U003E_GREATER_THAN_SIGN       0x8020003Eu
#define TOKEN_FOR_U0040_COMMERCIAL_AT           0x80200040u
#define TOKEN_FOR_U005B_LEFT_SQUARE_BRACKET     0x8020005Bu
#define TOKEN_FOR_U005D_RIGHT_SQUARE_BRACKET    0x8020005Du
#define TOKEN_FOR_U007B_LEFT_CURLY_BRACKET      0x8020007Bu
#define TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET     0x8020007Du

#define TOKEN_FOR_OPERATOR_COLON_COLON          0x80401D3Au
#define TOKEN_FOR_OPERATOR_MINUS_MINUS          0x804016ADu
#define TOKEN_FOR_OPERATOR_PLUS_PLUS            0x804015ABu

#define TOKEN_FOR_PLACEHOLDER_CHAR              0x8069D2A7u
#define TOKEN_FOR_PLACEHOLDER_FLOATING_POINT    0x806C9731u
#define TOKEN_FOR_PLACEHOLDER_STRING            0x806892A2u
/// Placeholders are '?', 1.2 and "?".

// clang-format on

typedef uint64_t line_number_and_token_t;
/// Line number is in the high 32 bits, token is in the low 32 bits.

static void  //
emit_one(line_number_and_token_t lnat);

// --------

// Global variables, named g_foobar or n_foobar. n_foobar is g_foobar's length
// (the number of g_foobar elements used).

static const char* g_color_code0 = NULL;
static const char* g_color_code1 = NULL;

static const char* g_progname_ptr = NULL;
static const char* g_filename_ptr = NULL;
static size_t g_filename_len = 0u;

static const char* g_input_ptr = NULL;
static const char* g_input_end = NULL;

static token_t g_token = 0u;
static uint32_t g_line_number = 0u;
#define MAX_EXCL_LINE_NUMBER 0x80000000u

static uint32_t i_emittable_definitions = 0u;
static uint32_t n_emittable_definitions = 0u;
static uint32_t n_macro_definitions = 0u;
static uint32_t n_hash_table = 0u;
static uint32_t n_lnats = 0u;
static uint32_t n_string_contents = 0u;

static bool g_has_preprocess_directive = false;
static bool g_has_rust_attribute = false;

static bool g_looks_rusty_prior_to_tokenization = false;

#define ABLED_ENABLED 0u
#define ABLED_DISABLED_AND_PREVIOUSLY_ENABLED 1u
#define ABLED_DISABLED_AND_NOT_PREVIOUSLY_ENABLED 3u

#define ABLED_BIT_DISABLED 1u
#define ABLED_BIT_NEVER_BEEN_ENABLED 2u

#define G_PREPRO_IF_ELSE_ABLED_SIZE 64u
static struct {
  uint32_t n_disabled;
  uint32_t n_abled;
  uint8_t abled[G_PREPRO_IF_ELSE_ABLED_SIZE];
} g_prepro_if_else = {0};
/// The stack of active "#if", "ifdef", "#ifndef", "#else", "#elif" and
/// "#endif" preprocessor directives.
///
/// abled[i] being an ABLED_XXX constant means that token output is enabled or
/// disabled for this branch. Output is disabled inside an "#if 0" or "#elif 0"
/// branch, or for an "#else" or "#elif" branch following an enabled branch.
///
/// This program treats anything that's not "#if 0", (such as "#if foo",
/// "#ifdef foo" or "#if bar > baz") as equivalent to "#if 1".
///
/// (§). In the future, we may be more sophisticated in the way we treat "#if
/// defined(foo)", depending on whether or not "foo" is actually defined. But
/// for now, the simple heuristic of always taking the first non-"#if 0"
/// branch works well enough, similar to the ctags command-line tool.
///
/// n_abled is the abled stack depth. n_disabled is the number of stack entries
/// that are disabled (they have the ABLED_BIT_DISABLED set).

#define G_PREFIX_MARKS_SIZE 64u
static struct {
  uint32_t n_marks;
  uint32_t n_array;
  uint16_t marks[G_PREFIX_MARKS_SIZE];
  char array[4096];
} g_prefix = {0};
/// The "foo.bar." in the output line `foo.bar.qux = "d/e/filename.c:12";`.
///
/// g_prefix.array holds the NUL-terminated string and in this "foo.bar."
/// example, n_array is 8, the string's length (excluding the NUL byte).
/// n_marks is 2, the number of dots in the string. There are two marks.
/// g_prefix.marks[0] is 0, the offset of the 'f', and g_prefix.marks[1] is 4,
/// the offset of the 'b'.

#define G_EMITTABLE_DEFINITIONS_SIZE 16384
static line_number_and_token_t
    g_emittable_definitions[G_EMITTABLE_DEFINITIONS_SIZE] = {0};
/// A list (sorted by line number) of "#define foo bar" lines to emit. The
/// token is the "foo".

#define G_MACRO_DEFINITIONS_SIZE 65536
static token_t g_macro_definitions[G_MACRO_DEFINITIONS_SIZE] = {0};
/// g_macro_definitions[0] is always 0u (meaning an undefined macro).
///
/// g_macro_definitions[1] and g_macro_definitions[2] are always 0xFFFFFFFFu
/// and 0u (meaning a defined-but-empty macro).
///
/// Then follows a series of 0u-terminated token-strings. Each token-string
/// begins with the bitwise-not of the number of arguments in the macro. For
/// example, after
///
/// #define MY_MACRO(a, b) a + k * b
///
/// Then the token for "MY_MACRO" would index into g_string_contents, pointing
/// to a char-string that starts with a u16le index into g_macro_definitions,
/// pointing to a token-string. That token-string would start with 0xFFFFFFFDu,
/// meaning a macro with two arguments, followed by 1st_macro_argument, plus,
/// k, star, 2nd_macro_argument, 0.
///
/// For now, g_macro_definitions is currently written to but never read (unless
/// DEBUG_DUMP_DEFINES is true). In the future, we may be more sophisticated
/// about #if/#else macro processing, per (§).
///
/// 256 KiB = 64 Ki elements × 4 bytes per element.

#define G_HASH_TABLE_SIZE 262144
static uint32_t g_hash_table[G_HASH_TABLE_SIZE] = {0};
/// A hash map from alpha-numeric strings (using the Jenkins hash function with
/// linear probing) to indirect tokens.
///
/// 1 MiB = 256 Ki elements × 4 bytes per element.

#define G_LNATS_SIZE 1048576
static line_number_and_token_t g_lnats[G_LNATS_SIZE] = {0};
/// 8 MiB = 1 Mi elements × 8 bytes per element.

#define TOKEN_AT(l) ((token_t)(g_lnats[l]))

static char g_string_contents[2097152] = {0};
/// 2 MiB. Offsets into this array fit into 21 bits.

static void  //
reset_global_tokenizer_state(void) {
  if (n_hash_table > 0u) {
    memset(g_hash_table, 0, sizeof(g_hash_table));
  }

  g_token = 0u;
  g_line_number = 1u;

  i_emittable_definitions = 0u;
  n_emittable_definitions = 0u;
  n_macro_definitions = 3u;
  n_hash_table = 0u;
  n_lnats = 0u;
  n_string_contents = 0u;

  g_has_preprocess_directive = false;
  g_has_rust_attribute = false;

  g_prepro_if_else.n_disabled = 0u;
  g_prepro_if_else.n_abled = 0u;

  g_prefix.n_marks = 0u;
  g_prefix.n_array = 0u;
  g_prefix.array[0] = 0;

  g_macro_definitions[0] = 0u;
  g_macro_definitions[1] = 0xFFFFFFFFu;
  g_macro_definitions[2] = 0u;
}

// --------

static uint32_t  //
jenkins(const char* s_ptr, size_t s_len) {
  /// This implements https://en.wikipedia.org/wiki/Jenkins_hash_function

  uint32_t hash = 0u;
  while (s_len--) {
    hash += ((uint8_t)(*s_ptr++));
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

static error_message_t  //
internalize(const char* s_ptr, size_t s_len, bool namey) {
  /// Sets g_token to the integery or namey token for the string s, inserting
  /// the string into g_hash_table and g_string_contents if not seen before.
  /// Internalizing is also known as string interning.

  if (s_len == 0u) {
    return err_int_badargum;
  } else if (s_len >= 512u) {
    return err_tok_tokistol;
  }
  uint32_t high_bits = (((uint32_t)(s_len)) << 21) | (namey ? 0x40000000u : 0u);

  const uint32_t hash_mask = G_HASH_TABLE_SIZE - 1;
  uint32_t h = jenkins(s_ptr, s_len) & hash_mask;
  for (;; h = (h + 1) & hash_mask) {
    uint32_t v = g_hash_table[h];
    if (v == 0u) {
      break;
    } else if ((v & HIGH_11_BITS_MASK) != high_bits) {
      continue;
    }

    uint32_t offset = v & LOW_21_BITS_MASK;
    if (!memcmp(&g_string_contents[offset + 2], s_ptr, s_len)) {
      g_token = v;
      return NULL;
    }
  }

  size_t capacity = sizeof(g_string_contents) - n_string_contents;
  if ((capacity < (s_len + 3u)) || (n_hash_table >= (G_HASH_TABLE_SIZE / 2))) {
    return err_tok_itluniqs;
  }
  n_hash_table++;
  g_token = high_bits | ((uint32_t)(n_string_contents));
  g_hash_table[h] = g_token;

  poke_u16le(g_string_contents + n_string_contents, 0u);
  n_string_contents += 2u;

  memcpy(g_string_contents + n_string_contents, s_ptr, s_len);
  n_string_contents += ((uint32_t)(s_len));

  g_string_contents[n_string_contents] = 0;
  n_string_contents += 1u;

  return NULL;
}

static const char*  //
token_as_cstring(token_t t) {
  /// Returns a C-style (NUL-byte terminated) printable string for t. The
  /// string is temporary, in that the backing array can be re-used on the next
  /// call to this function.

  static char buffer[16] = {0};

  switch (t >> 30) {
    case 2:
      buffer[0] = 0x7Fu & (t >> 0);
      buffer[1] = 0x7Fu & (t >> 7);
      buffer[2] = 0x7Fu & (t >> 14);
      buffer[3 & (t >> 21)] = 0;
      return buffer;

    case 3:
      t &= LOW_21_BITS_MASK;
      // "¤", U+00B5 CURRENCY SIGN, in UTF-8 is 0xC2 0xA4.
      buffer[0] = ((char)(0xC2));
      buffer[1] = ((char)(0xA4));
      for (int i = 0; i < 6; i++) {
        buffer[7 - i] = hex_digits[t & 15u];
        t >>= 4;
      }
      buffer[8] = 0x00;
      return buffer;
  }

  uint32_t offset = t & LOW_21_BITS_MASK;
  return &g_string_contents[offset + 2];
}

static bool  //
token_cstring_is_macro_namey(const char* p) {
  /// Returns whether p is ALL_CAPS and at least 2 characters long.

  const char* p0 = p;
  for (; *p; p++) {
    if ((*p != '_') && ((*p < 'A') || ('Z' < *p))) {
      return false;
    }
  }

  return (p - p0) > 1;
}

static const char*  //
next_rust_attribute_token(const char* p) {
  const char c0 = ((p + 0) < g_input_end) ? p[0] : 0;
  const char c1 = ((p + 1) < g_input_end) ? p[1] : 0;
  if (c0 == '[') {
    p += 1;
  } else if ((c0 == '!') && (c1 == '[')) {
    p += 2;
  } else {
    return NULL;
  }

  uint32_t bracket_depth = 1u;
  for (; p < g_input_end; p++) {
    if (*p == '[') {
      bracket_depth++;
    } else if (*p == ']') {
      bracket_depth--;
      if (bracket_depth == 0u) {
        return p + 1;
      }
    } else if (*p < ' ') {
      if ((*p != '\t') && (*p != '\n') && (*p != '\r')) {
        return NULL;
      }
    }
    // TODO: brackets inside quoted strings.
  }
  return p;
}

static error_message_t  //
next_numeric_token(void) {
  /// Parses the next token, a numeric one, like "123", "45ul", "6.78" or
  /// "0x9A". The result might be a numbery (indirect) token for integers, or
  /// it might be a placeholder 'punctuation' (direct) token for floating point
  /// numbers. As a side effect, it updates g_input_ptr and g_token.

  uint8_t lut_bits = 0x0A;

  const char* p = g_input_ptr;
  if (*p++ == '0') {
    if (p >= g_input_end) {
      g_input_ptr = g_input_end;
      return internalize("0", 1u, false);
    } else if ((*p == 'b') || (*p == 'x')) {
      p++;
      lut_bits = 0x0E;
    }
  }

  while ((p < g_input_end) && (lut_bits & alpha_numeric_lut[0xFF & *p])) {
    p++;
  }

  if ((p < g_input_end) && (*p == '.')) {
    p++;
    while ((p < g_input_end) && (lut_bits & alpha_numeric_lut[0xFF & *p])) {
      p++;
    }

    if ((p < g_input_end) && ((*p | 0x20) == 'e')) {
      p++;
      if ((p < g_input_end) && ((*p | 0x02) == '-')) {
        p++;
      }
      while ((p < g_input_end) && (0x02 & alpha_numeric_lut[0xFF & *p])) {
        p++;
      }
    }

    g_input_ptr = p;
    g_token = TOKEN_FOR_PLACEHOLDER_FLOATING_POINT;
    return NULL;
  }

  if ((p < g_input_end) && ((*p | 0x20) == 'u')) {
    p++;
  }
  while ((p < g_input_end) && ((*p | 0x20) == 'l')) {
    p++;
  }

  const char* original_p = g_input_ptr;
  g_input_ptr = p;
  size_t len = ((size_t)(p - original_p));
  return internalize(original_p, len, false);
}

static error_message_t  //
next_quote_token(const char* p, const char quote) {
  if (quote == '\'') {
    if (p >= g_input_end) {
      return err_tok_quotewnt;
    } else if (p[0] == '\'') {
      return err_tok_quotewem;
    } else if (('_' == p[0]) || (('a' <= p[0]) && (p[0] <= 'z'))) {
      if ((p + 1) >= g_input_end) {
        return err_tok_quotewnt;
      } else if (p[1] == '\'') {
        // We have a char literal, such as 'a' or 's'.
        g_input_ptr = p + 2;
        g_token = TOKEN_FOR_PLACEHOLDER_CHAR;
        return NULL;
      }
      // We have something lifetime-like, such as 'a or 'static. The next token
      // is just an apostrophe.
      g_input_ptr = p;
      g_token = TOKEN_FOR_U0027_APOSTROPHE;
      return NULL;
    }
  }

  for (bool backslash = false; p < g_input_end; p++) {
    if (*p == '\n') {
      g_line_number++;
      if (g_line_number >= MAX_EXCL_LINE_NUMBER) {
        return err_tok_itllines;
      }
    }

    if (backslash) {
      backslash = false;
      continue;
    } else if (*p == quote) {
      g_input_ptr = p + 1;
      g_token = (quote == '"') ? TOKEN_FOR_PLACEHOLDER_STRING
                               : TOKEN_FOR_PLACEHOLDER_CHAR;
      return NULL;
    }

    backslash = *p == '\\';
  }
  return err_tok_quotewnt;
}

static error_message_t  //
next_rust_raw_string_token(const char* p) {
  uint32_t num_hashes = 0u;
  for (; (p < g_input_end) && (*p == '#'); p++) {
    num_hashes++;
  }
  if (p >= g_input_end) {
    return err_tok_quotewnt;
  } else if (*p++ != '"') {
    return err_int_notaraws;
  }

  while (true) {
    while (true) {
      if (p >= g_input_end) {
        return err_tok_quotewnt;
      } else if (*p++ == '"') {
        break;
      }
    }

    const char* q = p;
    for (uint32_t n = num_hashes;; n--) {
      if (n == 0) {
        g_input_ptr = q;
        g_token = TOKEN_FOR_PLACEHOLDER_STRING;
        return NULL;
      } else if (q >= g_input_end) {
        return err_tok_quotewnt;
      } else if (*q++ != '#') {
        break;
      }
    }
  }
}

static error_message_t  //
next_token(bool return_line_feed_as_a_token) {
  /// Parses the next token. As a side effect, it updates g_input_ptr, g_token
  /// and g_line_number.

  const char* p = g_input_ptr;

restart_next_token:

  // Consume any leading whitespace.
  while (true) {
    if (p >= g_input_end) {
      return err_int_endoffil;
    }
    uint32_t u = 0xFF & *p;
    if (u > ' ') {
      break;
    }
    p++;
    if (u == '\n') {
      g_line_number++;
      if (g_line_number >= MAX_EXCL_LINE_NUMBER) {
        return err_tok_itllines;
      } else if (return_line_feed_as_a_token) {
        g_input_ptr = p;
        g_token = TOKEN_FOR_U000A_LINE_FEED;
        return NULL;
      }
    } else if (u == '\x00') {
      return err_tok_nulbytee;
    }
  }

  // Handle name and number tokens.
  uint64_t packed_utf8 = decode_utf8_rune(p, g_input_end);
  if (is_namey_rune(0x01, ((uint32_t)(packed_utf8)))) {
    if ((((uint32_t)(packed_utf8)) == 'r') &&   //
        g_looks_rusty_prior_to_tokenization &&  //
        ((p + 1) < g_input_end) &&              //
        ((p[1] == '"') || (p[1] == '#'))) {
      error_message_t err = next_rust_raw_string_token(p + 1);
      if (err != err_int_notaraws) {
        return err;
      }
      p++;
      packed_utf8 = decode_utf8_rune(p, g_input_end);
    }

    const char* original_p = p;
    while (true) {
      p += ((uint32_t)(packed_utf8 >> 32));
      if (p >= g_input_end) {
        break;
      }
      packed_utf8 = decode_utf8_rune(p, g_input_end);
      if (!is_namey_rune(0x03, ((uint32_t)(packed_utf8)))) {
        break;
      }
    }
    g_input_ptr = p;
    size_t len = ((size_t)(p - original_p));
    return internalize(original_p, len, true);

  } else if (('0' <= *p) && (*p <= '9')) {
    g_input_ptr = p;
    return next_numeric_token();
  }

  // Handle quote tokens.
  const char c = *p++;
  if ((c == '"') || (c == '\'')) {
    return next_quote_token(p, c);
  }

  // Handle C/C++ macros (which are just TOKEN_FOR_U0023_NUMBER_SIGN) and Rust
  // attributes (which this program treats as a comment).
  if (c == '#') {
    const char* q = next_rust_attribute_token(p);
    if (q == NULL) {
      g_input_ptr = p;
      g_token = TOKEN_FOR_U0023_NUMBER_SIGN;
      return NULL;
    }
    p = q;
    g_has_rust_attribute = true;
    goto restart_next_token;
  }

  // From here onwards, it's comments (or a backslash-line-feed), which
  // restarts parsing the next token, or punctuation.

  if (c == '/') {
    if (p >= g_input_end) {
      // No-op.

    } else if (*p == '/') {  // Slash-slash comment.
      p++;
      for (; (p < g_input_end) && (*p != '\n'); p++) {
      }
      goto restart_next_token;

    } else if (*p == '*') {  // Slash-star comment.
      p++;
      for (uint32_t depth = 1u; depth > 0u;) {
        if (p >= (g_input_end - 1)) {
          return err_tok_slashwnt;
        } else if ((p[0] == '*') && (p[1] == '/')) {
          p += 2;
          depth--;
          continue;
        } else if ((p[0] == '/') && (p[1] == '*') &&
                   g_looks_rusty_prior_to_tokenization) {
          p += 2;
          depth++;
          continue;
        }

        if (*p++ == '\n') {
          g_line_number++;
          if (g_line_number >= MAX_EXCL_LINE_NUMBER) {
            return err_tok_itllines;
          }
        }
      }
      goto restart_next_token;
    }

  } else if (c == '\\') {
    for (; p < g_input_end; p++) {
      if (*p == '\n') {
        p++;
        g_line_number++;
        if (g_line_number >= MAX_EXCL_LINE_NUMBER) {
          return err_tok_itllines;
        }
        goto restart_next_token;
      }
    }
    return err_tok_backswnt;

  } else if (c == '+') {
    if ((p < g_input_end) && (*p == '+')) {
      g_input_ptr = p + 1;
      g_token = TOKEN_FOR_OPERATOR_PLUS_PLUS;
      return NULL;
    }

  } else if (c == '-') {
    if ((p < g_input_end) && (*p == '-')) {
      g_input_ptr = p + 1;
      g_token = TOKEN_FOR_OPERATOR_MINUS_MINUS;
      return NULL;
    }

  } else if (c == ':') {
    if ((p < g_input_end) && (*p == ':')) {
      p++;

      // As a hack, tokenize the "::~" in the C++ destructor "Foo::~Foo" as a
      // TOKEN_FOR_OPERATOR_COLON_COLON, ignoring the "~". This simplifies the
      // other code that deals with TOKEN_FOR_OPERATOR_COLON_COLON. clloverview
      // doesn't discriminate between C++ constructors and destructors but
      // would otherwise have to care about the tilde.
      for (; p < g_input_end; p++) {
        uint32_t u = 0xFF & *p;
        if (u == '~') {
          p++;
          break;
        } else if ((u != '\t') && (u != ' ')) {
          break;
        }
      }

      g_input_ptr = p;
      g_token = TOKEN_FOR_OPERATOR_COLON_COLON;
      return NULL;
    }

  } else if (c >= 0x80) {
    return err_tok_unsuunic;
  }

  g_input_ptr = p;
  g_token = make_one_byte_punctuation_token(c);
  return NULL;
}

// --------

static token_t  //
substitute_macro_argument_tokens(token_t t,
                                 token_t* args_ptr,
                                 uint32_t args_len) {
  for (uint32_t i = 0u; i < args_len; i++) {
    if (t == args_ptr[i]) {
      return make_macro_argument_token(i);
    }
  }
  return t;
}

static error_message_t  //
preprocess_define(void) {
  static const uint32_t args_size = 64u;
  token_t args[64];
  uint32_t num_args = 0u;

  int state = 0;
  uint32_t index = 0u;
  token_t name = 0u;
  uint32_t n = n_macro_definitions;

  while (true) {
    error_message_t err = next_token(true);
    if (err == NULL) {
      // No-op.
    } else if (err != err_int_endoffil) {
      return err;
    } else {
      return err_pre_linewsnt;
    }

    if (g_token == TOKEN_FOR_U000A_LINE_FEED) {
      break;
    }

    switch (state) {
      case 0:  // Initial state.
        if (!token_is_namey(g_token)) {
          return err_pre_badmacde;
        } else if ((n++ >= G_MACRO_DEFINITIONS_SIZE) ||
                   (n_emittable_definitions >= G_EMITTABLE_DEFINITIONS_SIZE)) {
          return err_pre_macroatl;
        }
        index = n_macro_definitions;
        name = g_token;
        g_emittable_definitions[n_emittable_definitions++] =
            (((uint64_t)(g_line_number)) << 32) | ((uint64_t)(name));
        if ((g_input_ptr < g_input_end) && (*g_input_ptr == '(')) {
          g_input_ptr++;
          state = 1;
          continue;
        }
        state = 4;
        continue;

      case 1:  // After the '(' introducing macro arguments.
        if (g_token == TOKEN_FOR_U0029_RIGHT_PARENTHESIS) {
          state = 4;
          continue;
        }
        // fallthrough

      case 2:  // Before a macro argument.
        if (!token_is_namey(g_token)) {
          return err_pre_badmacde;
        } else if (num_args >= args_size) {
          return err_pre_macroatl;
        }
        args[num_args++] = g_token;
        state = 3;
        continue;

      case 3:  // After a macro argument.
        if (g_token == TOKEN_FOR_U002C_COMMA) {
          state = 2;
          continue;
        } else if (g_token == TOKEN_FOR_U0029_RIGHT_PARENTHESIS) {
          state = 4;
          continue;
        }
        return err_pre_badmacde;

      case 4:  // After the ')' concluding macro arguments, if it even exists.
        break;
    }

    if (n >= G_MACRO_DEFINITIONS_SIZE) {
      return err_pre_macroatl;
    }
    g_macro_definitions[n++] =
        substitute_macro_argument_tokens(g_token, args, num_args);
  }

  if (state != 4) {
    return err_pre_badmacde;
  } else if (n >= G_MACRO_DEFINITIONS_SIZE) {
    return err_pre_macroatl;
  } else if ((num_args == 0u) && (n == (index + 1u))) {
    // All empty macros can point to the same token-string at index 1u.
    index = 1u;
  } else {
    g_macro_definitions[index] = ~num_args;
    g_macro_definitions[n++] = 0u;
    n_macro_definitions = n;
  }

  uint32_t offset = LOW_21_BITS_MASK & name;
  poke_u16le(g_string_contents + offset, ((uint16_t)(index)));

#ifdef DEBUG_DUMP_DEFINES
  fprintf(stderr,
          "#define  0x%04" PRIX32 ":%02" PRIX32 " 0x%08" PRIX32 " ++ %s\n",
          index, num_args, name, token_as_cstring(name));
  for (uint32_t i = index + 1u;;) {
    token_t t = g_macro_definitions[i++];
    if (t == 0u) {
      break;
    }
    fprintf(stderr, "                   0x%08" PRIX32 "    %s\n", t,
            token_as_cstring(t));
  }
#endif

  return NULL;
}

static token_t g_preprocess_readline_result = 0u;

static error_message_t  //
preprocess_readline(void) {
  /// Consumes the rest of the preprocessor directive (to the end of the line).
  /// As a side effect, it sets g_preprocess_readline_result to the token for
  /// that rest of the line, if the line was a single token. For example, it
  /// will return the "foobar" for "#if foobar" or "#undef foobar".

  g_preprocess_readline_result = 0u;

  for (bool first = true;; first = false) {
    error_message_t err = next_token(true);
    if (err == NULL) {
      // No-op.
    } else if (err != err_int_endoffil) {
      return err;
    } else {
      return err_pre_linewsnt;
    }

    if (g_token == TOKEN_FOR_U000A_LINE_FEED) {
      break;
    } else if (first) {
      g_preprocess_readline_result = g_token;
    } else {
      g_preprocess_readline_result = 0u;
    }
  }

  return NULL;
}

static error_message_t  //
preprocess_undef(void) {
  TRY(preprocess_readline());
  token_t name = g_preprocess_readline_result;

  if ((name == 0u) || !token_is_namey(name)) {
    return err_pre_badmacun;
  }

  uint32_t offset = LOW_21_BITS_MASK & name;
  uint32_t index = peek_u16le(g_string_contents + offset);
  poke_u16le(g_string_contents + offset, 0u);

#ifdef DEBUG_DUMP_DEFINES
  uint32_t num_args = (index != 0u) ? ~g_macro_definitions[index] : 0u;
  fprintf(stderr,
          "#undef   0x%04" PRIX32 ":%02" PRIX32 " 0x%08" PRIX32 " -- %s\n",
          index, num_args, name, token_as_cstring(name));
#else
  (void)index;
#endif

  return NULL;
}

static error_message_t  //
preprocess_elif(bool arg_is_zero) {
  if (g_prepro_if_else.n_abled <= 0u) {
    return NULL;
  }

  uint32_t i = g_prepro_if_else.n_abled - 1u;
  switch (g_prepro_if_else.abled[i]) {
    case ABLED_ENABLED:
      g_prepro_if_else.abled[i] = ABLED_DISABLED_AND_PREVIOUSLY_ENABLED;
      g_prepro_if_else.n_disabled++;
      break;

    case ABLED_DISABLED_AND_PREVIOUSLY_ENABLED:
      break;

    case ABLED_DISABLED_AND_NOT_PREVIOUSLY_ENABLED:
      if (!arg_is_zero) {
        g_prepro_if_else.abled[i] = ABLED_ENABLED;
        g_prepro_if_else.n_disabled--;
      }
      break;
  }

  return NULL;
}

static error_message_t  //
preprocess_if(token_t directive) {
  TRY(preprocess_readline());
  token_t argument = g_preprocess_readline_result;

  if (directive == g_token_for_else) {
    return preprocess_elif(false);

  } else if (directive == g_token_for_elif) {
    return preprocess_elif(argument == g_token_for_0);

  } else if (directive == g_token_for_endif) {
    if (g_prepro_if_else.n_abled > 0u) {
      uint32_t i = g_prepro_if_else.n_abled - 1u;
      g_prepro_if_else.n_abled = i;
      g_prepro_if_else.n_disabled -=
          g_prepro_if_else.abled[i] & ABLED_BIT_DISABLED;
    }
    return NULL;
  }

  if ((g_prepro_if_else.n_abled + 1u) >= G_PREFIX_MARKS_SIZE) {
    return err_tok_itlnestd;
  }

  bool enabled = (directive == g_token_for_ifdef) ||   //
                 (directive == g_token_for_ifndef) ||  //
                 (argument != g_token_for_0);
  g_prepro_if_else.abled[g_prepro_if_else.n_abled] =
      enabled ? ABLED_ENABLED : ABLED_DISABLED_AND_NOT_PREVIOUSLY_ENABLED;
  g_prepro_if_else.n_abled++;
  g_prepro_if_else.n_disabled += enabled ? 0u : 1u;

  return NULL;
}

static error_message_t  //
preprocess(void) {
  error_message_t err = next_token(false);
  if (err == NULL) {
    // No-op.
  } else if (err != err_int_endoffil) {
    return err;
  } else {
    return err_pre_misdirec;
  }

  token_t directive = g_token;
  if (!token_is_namey(directive)) {
    return err_pre_baddirec;
  } else if ((directive == g_token_for_elif) ||   //
             (directive == g_token_for_else) ||   //
             (directive == g_token_for_endif) ||  //
             (directive == g_token_for_if) ||     //
             (directive == g_token_for_ifdef) ||  //
             (directive == g_token_for_ifndef)) {
    return preprocess_if(directive);
  }

  if (g_prepro_if_else.n_disabled > 0u) {
    // No-op.
  } else if (directive == g_token_for_define) {
    return preprocess_define();
  } else if (directive == g_token_for_undef) {
    return preprocess_undef();
  }

  while (true) {
    err = next_token(true);
    if (err == NULL) {
      // No-op.
    } else if (err != err_int_endoffil) {
      return err;
    } else {
      return err_pre_linewsnt;
    }

    if (g_token == TOKEN_FOR_U000A_LINE_FEED) {
      break;
    }
  }

  return NULL;
}

// --------

static error_message_t  //
tokenize(void) {
  /// Populates the g_lnats array and sets n_lnats.

  reset_global_tokenizer_state();
  INTERNALIZE_WELL_KNOWN_NAMES();

#ifdef DEBUG_DUMP_TOKENS
  fprintf(stderr, "# index    line    token\n");
#endif

  while (true) {
    error_message_t err = next_token(false);
    if (err == NULL) {
      // No-op.
    } else if (err != err_int_endoffil) {
      return err;
    } else {
      break;
    }

    if (g_token == TOKEN_FOR_U0023_NUMBER_SIGN) {
      TRY(preprocess());
      g_has_preprocess_directive = true;
      continue;
    }

    if (g_prepro_if_else.n_disabled > 0u) {
      continue;
    }

#ifdef DEBUG_DUMP_TOKENS
    fprintf(stderr, "#%06" PRIu32 "    %4" PRIu32 "    0x%08" PRIX32 "    %s\n",
            n_lnats, g_line_number, g_token, token_as_cstring(g_token));
#endif

    if (n_lnats >= (sizeof(g_lnats) / sizeof(line_number_and_token_t))) {
      return err_tok_itltoken;
    }
    g_lnats[n_lnats++] =
        (((uint64_t)(g_line_number)) << 32) | ((uint64_t)(g_token));
  }

#ifdef DEBUG_DUMP_HASH_TABLE
  for (uint32_t h = 0; h < G_HASH_TABLE_SIZE; h++) {
    uint32_t v = g_hash_table[h];
    if (v == 0u) {
      continue;
    }
    char* ptr = &g_string_contents[v & LOW_21_BITS_MASK];
    uint16_t m = peek_u16le(ptr);
    fprintf(stderr,
            "hash    h: 0x%05" PRIX32 "    v: 0x%08" PRIX32
            "    m: 0x%04" PRIX32 "    s: %s\n",
            // h=hash_key, v=value, m=macro, s=string.
            h, v, ((uint32_t)(m)), ptr + 2);
  }
#endif

#ifdef DEBUG_DUMP_STRINGS
  fprintf(stderr, "strings       0123456789ABCDEF\n");
  static char dump_strings_buffer[57] =
      "0123456789ABCDEF    00112233 44556677 8899AABB CCDDEEFF\n";
  for (uint32_t i = 0; i < n_string_contents; i += 16) {
    for (uint32_t j = 0; j < 16; j++) {
      uint32_t k = i + j;
      char c = (k < n_string_contents) ? g_string_contents[k] : 0;
      dump_strings_buffer[j] = (0x03 & alpha_numeric_lut[0xFF & c]) ? c : '.';
      dump_strings_buffer[20 + (2 * j) + (j / 4)] = hex_digits[15 & (c >> 4)];
      dump_strings_buffer[21 + (2 * j) + (j / 4)] = hex_digits[15 & (c >> 0)];
    }
    fprintf(stderr, "0x%08" PRIX32 "    %s", i, dump_strings_buffer);
  }
#endif

  return NULL;
}

// --------

static void  //
prefix_pop(void) {
  /// Pops "bar." off the prefix stack.

  if (g_prefix.n_marks > 0u) {
    g_prefix.n_marks--;
    g_prefix.n_array = g_prefix.marks[g_prefix.n_marks];
    g_prefix.array[g_prefix.n_array] = 0;
  }
}

static error_message_t  //
prefix_push_empty(void) {
  /// Pushes "" on the prefix stack.

  if ((g_prefix.n_marks + 1u) >= G_PREFIX_MARKS_SIZE) {
    return err_tok_itlnestd;
  }

  g_prefix.marks[g_prefix.n_marks] = ((uint16_t)(g_prefix.n_array));
  g_prefix.n_marks++;

  return NULL;
}

static error_message_t  //
prefix_push_string(const char* s_ptr, size_t s_len) {
  /// Pushes s and then a "." on the prefix stack.

  if (!s_ptr || !s_len) {
    return err_int_badargum;
  } else if ((g_prefix.n_marks + 1u) >= G_PREFIX_MARKS_SIZE) {
    return err_tok_itlnestd;
  }

  size_t capacity = sizeof(g_prefix.array) - g_prefix.n_array;
  if (capacity < 2u) {
    return err_tok_itlnestd;
  }
  capacity -= 2u;  // For the trailng ".\x00".

  if (s_len > capacity) {
    return err_tok_itlnestd;
  }

  g_prefix.marks[g_prefix.n_marks] = ((uint16_t)(g_prefix.n_array));
  g_prefix.n_marks++;

  char* dst = &g_prefix.array[g_prefix.n_array];
  memcpy(dst, s_ptr, s_len);
  dst += s_len;
  *dst++ = '.';
  *dst++ = 0;
  g_prefix.n_array += ((uint32_t)(s_len)) + 1u;
  return NULL;
}

static error_message_t  //
prefix_push_token(token_t t) {
  /// Pushes "bar." on the prefix stack, where "bar" is t's string form.

  return prefix_push_string(token_as_cstring(t), token_length(t));
}

static error_message_t  //
prefix_push_range(uint32_t l0, uint32_t l1) {
  /// Pushes "x.y.z." on the prefix stack, from the "x::y::z" that is the token
  /// range l0 .. l1.

  if ((l0 + 1u) == l1) {
    return prefix_push_token(TOKEN_AT(l0));
  } else if ((l0 + 1u) > l1) {
    return err_int_badargum;
  }

  static char buffer[4096] = {0};
  char* dst_ptr = &buffer[0];
  bool prepend_dot = false;
  for (uint32_t l = l0; l < l1; l++) {
    token_t t = TOKEN_AT(l);
    if (t == TOKEN_FOR_OPERATOR_COLON_COLON) {
      continue;
    }

    if (prepend_dot) {
      size_t remaining = ((size_t)(&buffer[sizeof(buffer)] - dst_ptr));
      if (remaining <= 0u) {
        return err_tok_itlnestd;
      }
      *dst_ptr++ = '.';
    }
    prepend_dot = true;

    const char* src_ptr = token_as_cstring(t);
    size_t src_len = token_length(t);
    size_t remaining = ((size_t)(&buffer[sizeof(buffer)] - dst_ptr));
    if (remaining < src_len) {
      return err_tok_itlnestd;
    }
    memcpy(dst_ptr, src_ptr, src_len);
    dst_ptr += src_len;
  }
  return prefix_push_string(&buffer[0], ((size_t)(dst_ptr - &buffer[0])));
}

// --------

static void  //
emit_ignoring_definitions(line_number_and_token_t lnat,
                          const char* prefix_ptr,
                          uint32_t prefix_len) {
  /// Prints one output line, `foo.bar.qux = "d/e/filename.c:12";`.
  ///
  /// The "foo.bar." part comes from prefix. The "qux" and "12" parts come from
  /// lnat. The "d/e/filename.c" is g_filename_ptr.

  static const char* eight_spaces_etc = "        = \"";

  uint32_t line_number = ((uint32_t)(lnat >> 32));
  token_t token = ((token_t)(lnat));
  const char* token_str = token_as_cstring(token);

  uint32_t n = count_utf8_runes(token_str, token_length(token));
  if (prefix_len) {
    n += count_utf8_runes(prefix_ptr, prefix_len);
  }

  if (*token_str == '#') {
    // Assume that we have a "r#foo" Rust raw identifier, which was tokenized
    // as a "#foo" namey token.
    token_str++;
    n--;
  }

  printf("%s%s%s%s%s%s:%" PRIu32 "\";\n",     //
         prefix_ptr,                          //
         g_color_code0 ? g_color_code0 : "",  //
         token_str,                           //
         g_color_code1 ? g_color_code1 : "",  //
         &eight_spaces_etc[n & 7u],           //
         g_filename_ptr,                      //
         line_number);
}

static void  //
emit_one(line_number_and_token_t lnat) {
  /// Like emit_ignoring_definitions but also first emits any preceding
  /// g_emittable_definitions. Preceding means one with a smaller line number.

  for (; i_emittable_definitions < n_emittable_definitions;
       i_emittable_definitions++) {
    line_number_and_token_t ed_lnat =
        g_emittable_definitions[i_emittable_definitions];
    if ((ed_lnat >> 32) >= (lnat >> 32)) {
      break;
    }
    emit_ignoring_definitions(ed_lnat, "", 0u);
  }

  emit_ignoring_definitions(lnat, g_prefix.array, g_prefix.n_array);
}

static void  //
emit_comma_separated_names(uint32_t l0, uint32_t l1) {
  /// Emits the "x", "y" and "z" from the "x, y, z int = blah" that is the
  /// token range l0 .. l1.

  for (uint32_t l = l0; l < l1;) {
    if (!token_is_namey(TOKEN_AT(l))) {
      break;
    }
    emit_one(g_lnats[l++]);
    if ((l >= l1) || (TOKEN_AT(l++) != TOKEN_FOR_U002C_COMMA)) {
      break;
    }
  }
}

static error_message_t  //
emit_colon_colon_separated(uint32_t l0, uint32_t l1) {
  /// Like emit_one but the "qux" can contain dots, like "x.y.z", derived from
  /// the "x::y::z" that is the token range l0 .. l1.

  int num_push = 0;
  uint32_t l = l0;
  for (; (l + 2u) <= l1; l += 2u) {
    token_t t0 = TOKEN_AT(l + 0u);
    token_t t1 = TOKEN_AT(l + 1u);
    if (!token_is_namey(t0) || (t1 != TOKEN_FOR_OPERATOR_COLON_COLON)) {
      break;
    }
    TRY(prefix_push_token(t0));
    num_push++;
  }
  emit_one(g_lnats[l]);
  while (num_push--) {
    prefix_pop();
  }
  return NULL;
}

// --------

static uint32_t  //
skip_brackets_pointy(uint32_t l0, uint32_t l1, bool pointy) {
  uint32_t bracket_depth = 1u;
  for (uint32_t l = l0; l < l1;) {
    token_t t = TOKEN_AT(l++);
    switch (t) {
      case TOKEN_FOR_U003C_LESS_THAN_SIGN:
        if (!pointy) {
          continue;
        }
        // fallthrough

      case TOKEN_FOR_U0028_LEFT_PARENTHESIS:
      case TOKEN_FOR_U005B_LEFT_SQUARE_BRACKET:
      case TOKEN_FOR_U007B_LEFT_CURLY_BRACKET:
        bracket_depth++;
        break;

      case TOKEN_FOR_U003E_GREATER_THAN_SIGN:
        if (!pointy) {
          continue;
        }
        // fallthrough

      case TOKEN_FOR_U0029_RIGHT_PARENTHESIS:
      case TOKEN_FOR_U005D_RIGHT_SQUARE_BRACKET:
      case TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET:
        bracket_depth--;
        if (bracket_depth == 0u) {
          return l;
        }
        break;
    }
  }
  return l1;
}

static uint32_t  //
skip_brackets(uint32_t l0, uint32_t l1) {
  /// Returns the smallest l, in the range (l0 + 1) ..= l1, such that the token
  /// range (l0 - 1) .. l forms a balance (the same number of left and right
  /// brackets) of round / square / curly brackets.
  ///
  /// The token just seen, at (l0 - 1), is assumed to be a left-bracket.
  ///
  /// If no such l exists, it returns l1.

  return skip_brackets_pointy(l0, l1, false);
}

static uint32_t  //
skip_past_semicolon(uint32_t l0, uint32_t l1) {
  /// Returns the smallest l, in the range (l0 + 1) ..= l1, such that the token
  /// at (l - 1) is a semi-colon and is also at the same "curly-bracket depth"
  /// as the token at l0.
  ///
  /// If no such l exists, it returns l1.

  uint32_t bracket_depth = 0u;
  for (uint32_t l = l0; l < l1;) {
    token_t t = TOKEN_AT(l++);
    if (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
      bracket_depth++;
    } else if ((t == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET) &&
               (bracket_depth > 0u)) {
      bracket_depth--;
    } else if ((t == TOKEN_FOR_U003B_SEMICOLON) && (bracket_depth == 0u)) {
      return l;
    }
  }
  return l1;
}

static uint32_t  //
skip_past_go_semicolon(uint32_t l0, uint32_t l1) {
  /// Returns the smallest l, in the range (l0 + 1) ..= l1, such that the token
  /// at (l - 1) is an explicit semi-colon, or there is an implicit semi-colon
  /// between the tokens at (l - 1) and at l, and is also at the same
  /// "curly-bracket depth" as the token at l0.
  ///
  /// If no such l exists, it returns l1.
  ///
  /// This is like skip_past_semicolon but, in Go, semi-colons can be implicit:
  /// https://go.dev/ref/spec#Semicolons

  for (uint32_t l = l0; l < l1;) {
    line_number_and_token_t lnat0 = g_lnats[l++];
    uint32_t line0 = ((uint32_t)(lnat0 >> 32));
    token_t token0 = ((token_t)(lnat0));

    if (l >= l1) {
      break;

    } else if (token0 == TOKEN_FOR_U003B_SEMICOLON) {
      return l;

    } else if ((token0 == TOKEN_FOR_U0029_RIGHT_PARENTHESIS) ||
               (token0 == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET)) {
      return l - 1;

    } else if ((token0 == TOKEN_FOR_U0028_LEFT_PARENTHESIS) ||
               (token0 == TOKEN_FOR_U005B_LEFT_SQUARE_BRACKET) ||
               (token0 == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET)) {
      l = skip_brackets(l, l1);
      if (l >= l1) {
        break;
      }
      lnat0 = g_lnats[l - 1u];
      line0 = ((uint32_t)(lnat0 >> 32));
      token0 = ((token_t)(lnat0));
    }

    line_number_and_token_t lnat1 = g_lnats[l];
    uint32_t line1 = ((uint32_t)(lnat1 >> 32));
    if (line0 == line1) {
      continue;
    }

    switch (token0) {
      case TOKEN_FOR_U0029_RIGHT_PARENTHESIS:
      case TOKEN_FOR_U005D_RIGHT_SQUARE_BRACKET:
      case TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET:
      case TOKEN_FOR_OPERATOR_MINUS_MINUS:
      case TOKEN_FOR_OPERATOR_PLUS_PLUS:
      case TOKEN_FOR_PLACEHOLDER_CHAR:
      case TOKEN_FOR_PLACEHOLDER_FLOATING_POINT:
      case TOKEN_FOR_PLACEHOLDER_STRING:
        return l;
    }

    if (token_is_indirect(token0)) {
      return l;
    }
  }
  return l1;
}

// --------

static uint32_t  //
parse_enum_fields(uint32_t l) {
  while (l < n_lnats) {
    token_t t = TOKEN_AT(l++);
    if (t == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET) {
      break;
    } else if ((t == TOKEN_FOR_U0028_LEFT_PARENTHESIS) ||     //
               (t == TOKEN_FOR_U005B_LEFT_SQUARE_BRACKET) ||  //
               (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET)) {
      l = skip_brackets(l, n_lnats);
    } else if (token_is_namey(t)) {
      emit_one(g_lnats[l - 1u]);
    }
  }
  return l;
}

// --------

static error_message_t  //
analyze_c_thing(uint32_t l0, uint32_t l1) {
  /// Analyzes a 'line' of C / C++ / etc code, the token range l0 .. l1, where
  /// 'line' means roughly something up until a ';' or '{'.

  uint32_t prev_l = 0xFFFFFFFFu;
  for (uint32_t l = l0; l < l1;) {
    token_t t = TOKEN_AT(l++);

    if (token_is_namey(t)) {
      if (l < l1) {
        switch (TOKEN_AT(l)) {
          case TOKEN_FOR_U0028_LEFT_PARENTHESIS:
            if (token_cstring_is_macro_namey(token_as_cstring(t))) {
              uint32_t l_after_bracket = skip_brackets(l + 1u, l1);
              if (l_after_bracket < l1) {
                l = l_after_bracket;
                continue;
              }
            }
            break;

          case TOKEN_FOR_U002C_COMMA:
          case TOKEN_FOR_U003B_SEMICOLON:
          case TOKEN_FOR_U003D_EQUALS_SIGN:
            if ((prev_l != 0xFFFFFFFFu) &&
                token_cstring_is_macro_namey(token_as_cstring(t))) {
              continue;
            }
            break;
        }
      }

      prev_l = l - 1u;
      for (; (l + 2u) <= l1; l += 2u) {
        token_t t0 = TOKEN_AT(l + 0u);
        token_t t1 = TOKEN_AT(l + 1u);
        if ((t0 != TOKEN_FOR_OPERATOR_COLON_COLON) || !token_is_namey(t1)) {
          break;
        }
      }
      continue;

    } else if (t == TOKEN_FOR_U005B_LEFT_SQUARE_BRACKET) {
      l = skip_brackets(l, l1);
      continue;

    } else if (t == TOKEN_FOR_U003C_LESS_THAN_SIGN) {
      l = skip_brackets_pointy(l, l1, true);
      continue;

    } else if (t == TOKEN_FOR_U0028_LEFT_PARENTHESIS) {
      if ((l < l1) && (TOKEN_AT(l) == TOKEN_FOR_U002A_ASTERISK)) {
        l++;
        continue;
      }

    } else if ((t != TOKEN_FOR_U002C_COMMA) &&
               (t != TOKEN_FOR_U003B_SEMICOLON) &&
               (t != TOKEN_FOR_U003D_EQUALS_SIGN)) {
      continue;
    }

    if (prev_l != 0xFFFFFFFFu) {
      TRY(emit_colon_colon_separated(prev_l, l - 1u));
    }
    prev_l = 0xFFFFFFFFu;

    if (t != TOKEN_FOR_U002C_COMMA) {
      break;
    }
  }

  return NULL;
}

static error_message_t  //
analyze_c(void) {
  uint32_t l0 = 0u;
  for (uint32_t l = 0u; l < n_lnats;) {
    token_t t = TOKEN_AT(l++);

    if ((t == TOKEN_FOR_U003B_SEMICOLON) ||
        (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET)) {
      TRY(analyze_c_thing(l0, l));
      if (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
        l = skip_brackets(l, n_lnats);
      }
      l0 = l;
      continue;

    } else if (t == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET) {
      prefix_pop();
      l0 = l;
      continue;

    } else if (t == TOKEN_FOR_U003C_LESS_THAN_SIGN) {
      l = skip_brackets_pointy(l, n_lnats, true);
      continue;

    } else if ((t == g_token_for_import) || (t == g_token_for_using)) {
      while (true) {
        if (l >= n_lnats) {
          return NULL;
        } else if (TOKEN_AT(l++) == TOKEN_FOR_U003B_SEMICOLON) {
          break;
        }
      }
      l0 = l;
      continue;

    } else if (t == g_token_for_enum) {
      if (l >= n_lnats) {
        return NULL;
      }
      bool is_enum_class = TOKEN_AT(l) == g_token_for_class;
      token_t type_name = 0;
      if (token_is_namey(TOKEN_AT(l))) {
        while (((l + 1u) < n_lnats) && (token_is_namey(TOKEN_AT(l + 1u)))) {
          l++;
        }
        emit_one(g_lnats[l]);
        type_name = is_enum_class ? TOKEN_AT(l) : 0;
        l++;
      }

      while (true) {
        if (l >= n_lnats) {
          return NULL;
        }

        token_t t = TOKEN_AT(l++);
        if (t == TOKEN_FOR_U003B_SEMICOLON) {
          break;
        } else if (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
          if (type_name != 0) {
            TRY(prefix_push_token(type_name));
          }
          l = parse_enum_fields(l);
          if (type_name != 0) {
            prefix_pop();
          }
          break;
        }
      }
      l0 = l;
      continue;

    } else if (t == g_token_for_extern) {
      if (((l + 1u) < n_lnats) &&
          (TOKEN_AT(l + 0u) == TOKEN_FOR_PLACEHOLDER_STRING) &&
          (TOKEN_AT(l + 1u) == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET)) {
        TRY(prefix_push_empty());
        l += 2u;
      }
      continue;

    } else if ((t != g_token_for_class) &&   //
               (t != g_token_for_struct) &&  //
               (t != g_token_for_namespace)) {
      continue;

    } else if (l >= n_lnats) {
      return NULL;
    }

    token_t class_name = TOKEN_AT(l);
    if (t != g_token_for_namespace) {
      if (class_name == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
        uint32_t l_after_bracket = skip_brackets(l + 1u, n_lnats);
        if ((l_after_bracket < n_lnats) &&
            token_is_namey(TOKEN_AT(l_after_bracket))) {
          TRY(prefix_push_token(TOKEN_AT(l_after_bracket)));
        } else {
          TRY(prefix_push_string("_type_name_", 11));
        }
      } else {
        while (((l + 1u) < n_lnats) && (token_is_namey(TOKEN_AT(l + 1u)))) {
          class_name = TOKEN_AT(l + 1u);
          l++;
        }
        emit_one(g_lnats[l++]);
        TRY(prefix_push_token(class_name));
      }

    } else if (class_name == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
      TRY(prefix_push_string("_anonymous_", 11));

    } else {
      uint32_t start_l = l++;
      for (; (l + 2u) <= n_lnats; l += 2u) {
        token_t t0 = TOKEN_AT(l + 0u);
        token_t t1 = TOKEN_AT(l + 1u);
        if ((t0 != TOKEN_FOR_OPERATOR_COLON_COLON) || !token_is_namey(t1)) {
          break;
        }
      }
      TRY(prefix_push_range(start_l, l));
    }

    while (true) {
      if (l >= n_lnats) {
        return NULL;
      }
      token_t t = TOKEN_AT(l++);
      if (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
        break;
      } else if (t == TOKEN_FOR_U003B_SEMICOLON) {
        prefix_pop();
        break;
      }
    }

    l0 = l;
  }
  return NULL;
}

// --------

static error_message_t  //
analyze_csharp_java(void) {
  if (n_lnats < 2u) {
    return NULL;
  }

  uint32_t l = 0u;
  if (TOKEN_AT(0) == g_token_for_package) {
    l++;
    while (l < (n_lnats - 1u)) {
      token_t t0 = TOKEN_AT(l++);
      if (token_is_namey(t0)) {
        TRY(prefix_push_token(t0));
      }
      token_t t1 = TOKEN_AT(l++);
      if (t1 == TOKEN_FOR_U003B_SEMICOLON) {
        break;
      }
    }
  }

  bool is_enumerating = false;
  uint32_t bracket_depth = 0u;
  while (l < n_lnats) {
    token_t t = TOKEN_AT(l++);

    if (t == TOKEN_FOR_U0028_LEFT_PARENTHESIS) {
      if ((l >= 2u) && !is_enumerating) {
        emit_one(g_lnats[l - 2u]);
      }
      l = skip_brackets(l, n_lnats);
      if (l >= n_lnats) {
        break;
      } else if (TOKEN_AT(l) == TOKEN_FOR_U003B_SEMICOLON) {
        l++;
        is_enumerating = false;
      } else if (TOKEN_AT(l) == g_token_for_default) {
        l = skip_past_semicolon(l + 1u, n_lnats);
        is_enumerating = false;
      }

    } else if ((t == TOKEN_FOR_U005B_LEFT_SQUARE_BRACKET) ||
               (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET)) {
      l = skip_brackets(l, n_lnats);

    } else if ((t == TOKEN_FOR_U003B_SEMICOLON) ||
               (t == TOKEN_FOR_U003D_EQUALS_SIGN)) {
      if (l >= 2u) {
        emit_one(g_lnats[l - 2u]);
      }
      l = skip_past_semicolon(l - 1u, n_lnats);
      is_enumerating = false;

    } else if (t == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET) {
      if (bracket_depth <= 0u) {
        return err_ana_unbalacb;
      }
      bracket_depth--;
      prefix_pop();
      is_enumerating = false;

    } else if (t == TOKEN_FOR_U0040_COMMERCIAL_AT) {
      if (l < n_lnats) {
        if (TOKEN_AT(l) == g_token_for_interface) {
          continue;
        }
        l++;
      }
      if ((l < n_lnats) && (TOKEN_AT(l) == TOKEN_FOR_U0028_LEFT_PARENTHESIS)) {
        l = skip_brackets(l + 1u, n_lnats);
      }

    } else if ((t == g_token_for_import) ||  //
               (t == g_token_for_using)) {
      l = skip_past_semicolon(l + 1u, n_lnats);
      is_enumerating = false;

    } else if ((t == g_token_for_class) ||      //
               (t == g_token_for_enum) ||       //
               (t == g_token_for_interface) ||  //
               (t == g_token_for_namespace)) {
      if (l >= n_lnats) {
        return NULL;
      }
      is_enumerating = (t == g_token_for_enum);

      if (t != g_token_for_namespace) {
        emit_one(g_lnats[l]);
      }
      TRY(prefix_push_token(TOKEN_AT(l++)));
      if ((t == g_token_for_namespace) &&  //
          (l < n_lnats) && (TOKEN_AT(l) == TOKEN_FOR_U003B_SEMICOLON)) {
        l++;
        continue;
      }

      if (bracket_depth >= 0xFFFFu) {
        return err_ana_toomancb;
      }
      bracket_depth++;

      while (true) {
        if (l >= n_lnats) {
          return NULL;
        } else if (TOKEN_AT(l++) == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
          break;
        }
      }

    } else if (is_enumerating && token_is_namey(t)) {
      emit_one(g_lnats[l - 1u]);
    }
  }

  return NULL;
}

// --------

static token_t  //
parse_go_receiver_type(uint32_t l0, uint32_t l1) {
  /// Returns the "T" out of "T", "x T", "x *T" for the token range l0 .. l1.

  token_t ret = 0;
  int num_names_seen = 0;
  for (uint32_t l = l0; l < l1;) {
    token_t t = TOKEN_AT(l++);
    if (!token_is_namey(t)) {
      continue;
    }
    ret = t;
    num_names_seen++;
    if (num_names_seen == 2) {
      break;
    }
  }
  return ret;
}

static error_message_t  //
analyze_go(void) {
  if (n_lnats < 2u) {
    return NULL;
  }
  TRY(prefix_push_token(TOKEN_AT(1)));

  for (uint32_t l = 2u; l < n_lnats;) {
    token_t keyword = TOKEN_AT(l++);
    if (l >= n_lnats) {
      return NULL;
    }

    if (keyword == g_token_for_import) {
      goto skip_to_next_top_level_declaration;

    } else if (keyword == g_token_for_func) {
      token_t recv_type = 0u;
      token_t func_name = TOKEN_AT(l);
      if (func_name == TOKEN_FOR_U0028_LEFT_PARENTHESIS) {
        uint32_t l0 = l + 1u;
        l = skip_brackets(l0, n_lnats);
        if (l >= n_lnats) {
          return NULL;
        }
        recv_type = parse_go_receiver_type(l0, l);
        if (!token_is_namey(recv_type)) {
          return NULL;
        }
        func_name = TOKEN_AT(l);
      }
      if (!token_is_namey(func_name)) {
        return NULL;
      }

      if (recv_type != 0u) {
        TRY(prefix_push_token(recv_type));
      }
      emit_one(g_lnats[l++]);
      if (recv_type != 0u) {
        prefix_pop();
      }

      goto skip_to_next_top_level_declaration;
    }

    bool parenthesized_keyword =
        TOKEN_AT(l) == TOKEN_FOR_U0028_LEFT_PARENTHESIS;
    if (parenthesized_keyword) {
      l++;
      if (l >= n_lnats) {
        return NULL;
      }
    }

    if ((keyword == g_token_for_const) ||  //
        (keyword == g_token_for_var)) {
      while (true) {
        uint32_t l1 = skip_past_go_semicolon(l, n_lnats);
        emit_comma_separated_names(l, l1);
        l = l1;

        if (!parenthesized_keyword) {
          goto skip_to_next_top_level_declaration;
        } else if (l >= n_lnats) {
          return NULL;
        } else if (TOKEN_AT(l) == TOKEN_FOR_U0029_RIGHT_PARENTHESIS) {
          l++;
          goto skip_to_next_top_level_declaration;
        }
      }

    } else if (keyword == g_token_for_type) {
      token_t type_name = TOKEN_AT(l);
      if (!token_is_namey(type_name)) {
        return NULL;
      }
      emit_one(g_lnats[l++]);
      while ((l < n_lnats) &&
             (TOKEN_AT(l) == TOKEN_FOR_U005B_LEFT_SQUARE_BRACKET)) {
        l = skip_brackets(l + 1u, n_lnats);
      }
      if (((l + 1u) < n_lnats) &&                      //
          (TOKEN_AT(l + 0u) == g_token_for_struct) &&  //
          (TOKEN_AT(l + 1u) == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET)) {
        l += 2u;
        TRY(prefix_push_token(type_name));
        while (true) {
          uint32_t l1 = skip_past_go_semicolon(l, n_lnats);
          emit_comma_separated_names(l, l1);
          l = l1;

          if (l >= n_lnats) {
            return NULL;
          } else if (TOKEN_AT(l) == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET) {
            l++;
            break;
          }
        }
        prefix_pop();
      }

    } else {
      return NULL;
    }

  skip_to_next_top_level_declaration:
    while (l < n_lnats) {
      token_t t = TOKEN_AT(l);
      if ((t == g_token_for_const) ||   //
          (t == g_token_for_func) ||    //
          (t == g_token_for_import) ||  //
          (t == g_token_for_type) ||    //
          (t == g_token_for_var)) {
        break;
      }
      l++;
    }
  }

  return NULL;
}

// --------

static uint32_t  //
namey_token_or_impl(uint32_t l) {
  if (token_is_namey(TOKEN_AT(l))) {
    return l;
  }
  for (; l > 0u; l--) {
    if (TOKEN_AT(l) == g_token_for_impl) {
      return l;
    }
  }
  return l;
}

static uint32_t  //
parse_rust_struct_fields(uint32_t l) {
  while (l < n_lnats) {
    token_t t = TOKEN_AT(l++);
    if (t == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET) {
      break;
    } else if (t == TOKEN_FOR_U003A_COLON) {
      emit_one(g_lnats[l - 2u]);
    }
  }
  return l;
}

static error_message_t  //
analyze_rust(void) {
  uint32_t bracket_depth = 0u;
  for (uint32_t l = 0u; l < n_lnats;) {
    token_t keyword = TOKEN_AT(l++);
    if (l >= n_lnats) {
      return NULL;
    }

    if (keyword == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
      l = skip_brackets(l, n_lnats);

    } else if (keyword == TOKEN_FOR_U007D_RIGHT_CURLY_BRACKET) {
      if (bracket_depth <= 0u) {
        return err_ana_unbalacb;
      }
      bracket_depth--;
      prefix_pop();

    } else if (keyword == g_token_for_use) {
      l = skip_past_semicolon(l + 1u, n_lnats);
      continue;

    } else if (keyword == g_token_for_type) {
      emit_one(g_lnats[l]);
      l = skip_past_semicolon(l + 1u, n_lnats);
      continue;

    } else if (keyword == g_token_for_fn) {
      if (!token_is_namey(TOKEN_AT(l))) {
        return NULL;
      }
      emit_one(g_lnats[l++]);

      while (l < n_lnats) {
        token_t t = TOKEN_AT(l++);
        if (t == TOKEN_FOR_U0028_LEFT_PARENTHESIS) {
          l = skip_brackets(l, n_lnats);
          break;
        }
      }

      while (l < n_lnats) {
        token_t t = TOKEN_AT(l++);
        if (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
          l = skip_brackets(l, n_lnats);
          break;
        } else if (t == TOKEN_FOR_U003B_SEMICOLON) {
          break;
        }
      }
      continue;

    } else if ((keyword == g_token_for_enum) ||  //
               (keyword == g_token_for_struct)) {
      emit_one(g_lnats[l]);
      token_t type_name = TOKEN_AT(l++);

      while (true) {
        if (l >= n_lnats) {
          return NULL;
        }

        token_t t = TOKEN_AT(l++);
        if (t == TOKEN_FOR_U003B_SEMICOLON) {
          break;
        } else if (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
          TRY(prefix_push_token(type_name));
          l = (keyword == g_token_for_enum) ? parse_enum_fields(l)
                                            : parse_rust_struct_fields(l);
          prefix_pop();
          break;
        }
      }

    } else if ((keyword == g_token_for_impl) ||  //
               (keyword == g_token_for_mod) ||   //
               (keyword == g_token_for_trait)) {
      if (TOKEN_AT(l) == TOKEN_FOR_U003C_LESS_THAN_SIGN) {
        l = skip_brackets_pointy(l + 1u, n_lnats, true);
        if (l >= n_lnats) {
          return NULL;
        }
      }
      while ((l < n_lnats) && (TOKEN_AT(l) == TOKEN_FOR_U0026_AMPERSAND)) {
        l++;
        if (((l + 1u) < n_lnats) &&
            (TOKEN_AT(l) == TOKEN_FOR_U0027_APOSTROPHE)) {
          l += 2u;
        }
      }
      uint32_t l_of_typename = namey_token_or_impl(l++);

      while (true) {
        if (l >= n_lnats) {
          return NULL;
        }

        token_t t = TOKEN_AT(l++);
        if (t == TOKEN_FOR_U007B_LEFT_CURLY_BRACKET) {
          uint32_t l0 = l_of_typename + 0u;
          uint32_t l1 = l_of_typename + 1u;

          if (keyword == g_token_for_trait) {
            emit_one(g_lnats[l_of_typename]);
          } else if (keyword == g_token_for_impl) {
            while (((l1 + 1u) < n_lnats) &&
                   (TOKEN_AT(l1) == TOKEN_FOR_OPERATOR_COLON_COLON) &&
                   (token_is_namey(TOKEN_AT(l1 + 1u)))) {
              l1 += 2u;
            }
          }

          TRY(prefix_push_range(l0, l1));
          if (bracket_depth >= 0xFFFFu) {
            return err_ana_toomancb;
          }
          bracket_depth++;
          break;

        } else if (t == g_token_for_for) {
          while ((l < n_lnats) && (TOKEN_AT(l) == TOKEN_FOR_U0026_AMPERSAND)) {
            l++;
            if (((l + 1u) < n_lnats) &&
                (TOKEN_AT(l) == TOKEN_FOR_U0027_APOSTROPHE)) {
              l += 2u;
            }
          }
          if (l < n_lnats) {
            l_of_typename = namey_token_or_impl(l++);
          }

        } else if ((bracket_depth == 0u) && (t == TOKEN_FOR_U003B_SEMICOLON)) {
          break;
        }
      }
    }
  }

  return NULL;
}

// --------

typedef error_message_t (*analyzer)(void);

static analyzer  //
guess_language_family(void) {
  if (g_has_preprocess_directive) {
    return &analyze_c;
  } else if (g_has_rust_attribute) {
    return &analyze_rust;
  }

  token_t t0 = (n_lnats > 0u) ? TOKEN_AT(0) : 0u;
  token_t t1 = (n_lnats > 1u) ? TOKEN_AT(1) : 0u;
  token_t t2 = (n_lnats > 2u) ? TOKEN_AT(2) : 0u;

  if (t0 == g_token_for_package) {
    if (!token_is_namey(t1)) {
      return NULL;
    } else if ((t2 == TOKEN_FOR_U002E_FULL_STOP) ||
               (t2 == TOKEN_FOR_U003B_SEMICOLON)) {
      return &analyze_csharp_java;
    }
    return &analyze_go;

  } else if ((t0 == g_token_for_using) ||      //
             (t0 == g_token_for_namespace) ||  //
             (t0 == g_token_for_class) ||      //
             (t0 == g_token_for_interface) ||  //
             (t1 == g_token_for_class) ||      //
             (t1 == g_token_for_interface)) {
    return &analyze_csharp_java;

  } else if ((t0 == g_token_for_enum) ||  //
             (t1 == g_token_for_enum)) {
    return g_looks_rusty_prior_to_tokenization ? &analyze_rust
                                               : &analyze_csharp_java;
  }

  uint32_t n = min_u32(n_lnats, 64u);
  for (uint32_t l = 0u; l < n;) {
    token_t t = TOKEN_AT(l++);
    if ((t == g_token_for_char) ||  //
        (t == g_token_for_int) ||   //
        (t == g_token_for_void)) {
      return &analyze_c;
    } else if ((t == g_token_for_fn) ||    //
               (t == g_token_for_impl) ||  //
               (t == g_token_for_mod) ||   //
               (t == g_token_for_pub)) {
      return &analyze_rust;
    }
  }

  return NULL;
}

static bool  //
guess_rustiness_prior_to_tokenization(const char* data_ptr,
                                      const size_t data_len) {
  size_t n = data_len;
  if (n > 32768u) {
    n = 32768u;
  } else if (n < 8u) {
    return false;
  }
  const char* p = data_ptr;
  const char* q = data_ptr + (n - 8u);

  int32_t score = 0;
  while (p != q) {
    switch (*p) {
      case '#':
        if ((p[1] == '!') || (p[1] == '[')) {
          score++;
        } else if (!memcmp(p, "defi", 4) ||  //
                   !memcmp(p, "ifde", 4) ||  //
                   !memcmp(p, "ifnd", 4) ||  //
                   !memcmp(p, "impo", 4) ||  //
                   !memcmp(p, "incl", 4) ||  //
                   !memcmp(p, "prag", 4)) {
          score--;
        }
        break;

      case 'c':
        if (!memcmp(p, "char ", 5)) {
          score--;
        } else if (!memcmp(p, "class ", 6)) {
          score--;
        }
        break;

      case 'f':
        if (!memcmp(p, "fn ", 3)) {
          score++;
        }
        break;

      case 'i':
        if (!memcmp(p, "impl ", 5)) {
          score++;
        } else if (!memcmp(p, "int ", 4)) {
          score--;
        }
        break;

      case 'm':
        if (!memcmp(p, "mod ", 4)) {
          score++;
        }
        break;

      case 'n':
        if (!memcmp(p, "namesp", 6)) {
          score--;
        }
        break;

      case 'p':
        if (!memcmp(p, "packag", 6)) {
          score--;
        } else if (!memcmp(p, "pub ", 4) ||  //
                   !memcmp(p, "pub(", 4)) {
          score++;
        } else if (!memcmp(p, "publ", 4)) {
          score--;
        }
        break;

      case 'v':
        if (!memcmp(p, "void ", 5)) {
          score--;
        }
        break;
    }

    while ((p != q) && (*p++ != '\n')) {
    }
  }

  return score > 0;
}

static bool  //
looks_like_bash_python_etc(void) {
  const char* p = g_input_ptr;
  for (; (p < g_input_end) && (*p <= ' '); p++) {
  }
  if ((p >= g_input_end) || (*p++ != '#')) {
    return false;
  }
  if ((p >= g_input_end) || (*p++ != '!')) {
    return false;
  }
  if ((p >= g_input_end) || (*p++ == '[')) {
    return false;
  }
  return true;
}

static error_message_t  //
process_file_contents(const char* data_ptr, const size_t data_len) {
  g_input_ptr = data_ptr;
  g_input_end = data_ptr + data_len;

  if (looks_like_bash_python_etc()) {
    return NULL;
  }
  g_looks_rusty_prior_to_tokenization =
      guess_rustiness_prior_to_tokenization(data_ptr, data_len);
  TRY(tokenize());

  analyzer a = guess_language_family();
  if (a == NULL) {
    return NULL;
  }
  TRY((*a)());

  for (uint32_t i = i_emittable_definitions; i < n_emittable_definitions; i++) {
    emit_ignoring_definitions(g_emittable_definitions[i], "", 0u);
  }
  i_emittable_definitions = n_emittable_definitions;

  return NULL;
}

static error_message_t  //
process_fd(int fd) {
  static char data_array[67108864] = {0};  // 64MiB.

  do {
    struct stat z;
    if (fstat(fd, &z) || !S_ISREG(z.st_mode)) {
      break;
    } else if (z.st_size <= 0) {
      return NULL;
    } else if (sizeof(data_array) <= z.st_size) {
      return err_tok_itlbytes;
    }

    const size_t data_len = ((size_t)(z.st_size));
    char* const data_ptr = mmap(NULL, data_len, PROT_READ, MAP_SHARED, fd, 0);
    if (data_ptr == MAP_FAILED) {
      break;
    }
    error_message_t ret = process_file_contents(data_ptr, data_len);
    munmap(data_ptr, data_len);
    return ret;
  } while (false);

  for (size_t data_len = 0; data_len < sizeof(data_array);) {
    ssize_t n = read(fd, data_array + data_len, sizeof(data_array) - data_len);
    if (n > 0) {
      data_len += ((size_t)(n));
    } else if (n == 0) {
      return process_file_contents(data_array, data_len);
    } else if (errno == EINTR) {
      continue;
    } else {
      return strerror(errno);
    }
  }

  return err_tok_itlbytes;
}

static error_message_t  //
process_file(const char* filename) {
  size_t n = 0u;
  for (const char* p = filename; *p; p++) {
    if (*p < ' ') {
      g_filename_ptr = "<elided>";
      g_filename_len = 8u;
      return err_com_namelong;
    } else if ((*p == '"') || (*p == '\\')) {
      g_filename_ptr = filename;
      g_filename_len = n;
      return err_com_unsuname;
    }
    n++;
  }

  if (n >= 4096u) {
    g_filename_ptr = "<elided>";
    g_filename_len = 8u;
    return err_com_namelong;
  }
  g_filename_ptr = filename;
  g_filename_len = n;

  int fd = open(g_filename_ptr, O_RDONLY, 0);
  if (fd < 0) {
    return strerror(errno);
  }
  error_message_t ret = process_fd(fd);
  close(fd);
  return ret;
}

static error_message_t  //
process_flag(const char* arg) {
  g_filename_ptr = "<command_line>";
  g_filename_len = 14u;

  if (arg[0] == '-') {
    arg++;
    if (arg[0] == '-') {
      arg++;
    }
  }

  if (!strcmp(arg, "?") || !strcmp(arg, "help")) {
    fprintf(stderr, "Usage: %s -color=auto|always|never *.c foo/bar/*.java\n",
            g_progname_ptr);
    return NULL;

  } else if (!strncmp(arg, "color=", 6)) {
    arg += 6;
    bool on = false;

    char* no_color = getenv("NO_COLOR");
    if ((no_color != NULL) && (no_color[0] != '\x00')) {
      // No-op.
    } else if (!strcmp(arg, "auto")) {
      static const int stdout_fd = 1;
      const char* term = getenv("TERM");
      on = isatty(stdout_fd) && term && strcmp(term, "dumb");
    } else if (!strcmp(arg, "always")) {
      on = true;
    } else if (!strcmp(arg, "never")) {
      on = false;
    } else {
      return err_com_unsuflag;
    }

    g_color_code0 = on ? "\033[32m" : "";
    g_color_code1 = on ? "\033[0m" : "";
    return NULL;
  }

  return err_com_unsuflag;
}

static int  //
print_error_message(error_message_t err) {
  if (err == NULL) {
    return 0;
  }

  static char line_number_buffer[32];
  int lnb_len = snprintf(line_number_buffer, sizeof(line_number_buffer),
                         ":%" PRIu32 " ", g_line_number);
  if (lnb_len < 0) {
    return 1;
  }

  static const int stderr_fd = 2;
  write(stderr_fd, g_filename_ptr, g_filename_len);
  write(stderr_fd, line_number_buffer, ((size_t)(lnb_len)));
  write(stderr_fd, err, strlen(err));
  write(stderr_fd, "\n", 1);
  return 1;
}

int  //
main(int argc, char** argv) {
  g_progname_ptr = (argc > 0) ? argv[0] : "<program>";
  process_flag("-color=auto");

  if (argc <= 1) {
    g_filename_ptr = "<stdin>";
    g_filename_len = 7u;
    static const int stdin_fd = 0;
    int ret = print_error_message(process_fd(stdin_fd));
    if (ret != 0) {
      return ret;
    }
  }

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    error_message_t err =
        (arg[0] == '-') ? process_flag(arg) : process_file(arg);
    int ret = print_error_message(err);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}
