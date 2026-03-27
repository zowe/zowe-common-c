/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * test_utils.c - Unit tests for utils.h pure/near-pure functions
 *
 * Build on z/OS: use the Makefile in this directory
 *   cd tests/unit && make test_utils && ./test_utils
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"

#include "zowe_test.h"

/* ========================================================================
 *  parseInt / parseInitialInt
 * ======================================================================== */

void test_parseInt_simple(void) {
  ASSERT_INT_EQ(123, parseInt("123", 0, 3));
  ASSERT_INT_EQ(0,   parseInt("0", 0, 1));
  ASSERT_INT_EQ(42,  parseInt("xx42yy", 2, 4));
}

void test_parseInt_substring(void) {
  ASSERT_INT_EQ(20, parseInt("2025-03-27", 0, 2));   /* "20" */
  ASSERT_INT_EQ(25, parseInt("2025-03-27", 2, 4));   /* "25" */
}

void test_parseInitialInt_stops_at_nondigit(void) {
  ASSERT_INT_EQ(99,  parseInitialInt("99abc", 0, 5));
  ASSERT_INT_EQ(0,   parseInitialInt("abc", 0, 3));
  ASSERT_INT_EQ(500, parseInitialInt("500", 0, 3));
}

/* ========================================================================
 *  indexOf / lastIndexOf
 * ======================================================================== */

void test_indexOf_found(void) {
  ASSERT_INT_EQ(5, indexOf("Hello World", 11, 'W', 0));
  ASSERT_INT_EQ(0, indexOf("abc", 3, 'a', 0));
}

void test_indexOf_not_found(void) {
  ASSERT_INT_EQ(-1, indexOf("Hello", 5, 'z', 0));
}

void test_indexOf_with_start(void) {
  ASSERT_INT_EQ(7, indexOf("abcXdefXghi", 11, 'X', 4));
}

void test_lastIndexOf_found(void) {
  ASSERT_INT_EQ(7, lastIndexOf("abc.def.ghi", 11, '.'));
}

void test_lastIndexOf_not_found(void) {
  ASSERT_INT_EQ(-1, lastIndexOf("abcdef", 6, '.'));
}

/* ========================================================================
 *  indexOfString / lastIndexOfString
 * ======================================================================== */

void test_indexOfString_found(void) {
  ASSERT_INT_EQ(6, indexOfString("Hello World!", 12, "World", 0));
}

void test_indexOfString_not_found(void) {
  ASSERT_INT_EQ(-1, indexOfString("Hello World", 11, "xyz", 0));
}

void test_indexOfString_at_start(void) {
  ASSERT_INT_EQ(0, indexOfString("abcdef", 6, "abc", 0));
}

void test_indexOfString_at_end(void) {
  ASSERT_INT_EQ(3, indexOfString("abcdef", 6, "def", 0));
}

void test_indexOfString_with_start_pos(void) {
  ASSERT_INT_EQ(6, indexOfString("abcabcabc", 9, "abc", 1));
}

void test_lastIndexOfString_found(void) {
  ASSERT_INT_EQ(8, lastIndexOfString("foo.bar.foo.baz", 15, "foo"));
}

void test_lastIndexOfString_not_found(void) {
  ASSERT_INT_EQ(-1, lastIndexOfString("foo.bar", 7, "xyz"));
}

/* ========================================================================
 *  isZeros / isBlanks / hasText
 * ======================================================================== */

void test_isZeros_true(void) {
  char buf[10] = {0};
  ASSERT_TRUE(isZeros(buf, 0, 10));
}

void test_isZeros_false(void) {
  char buf[10] = {0};
  buf[5] = 1;
  ASSERT_FALSE(isZeros(buf, 0, 10));
}

void test_isZeros_offset(void) {
  char buf[10] = {0};
  buf[0] = 'X';
  ASSERT_TRUE(isZeros(buf, 1, 9));
}

void test_isBlanks_true(void) {
  char buf[] = "     ";
  ASSERT_TRUE(isBlanks(buf, 0, 5));
}

void test_isBlanks_false(void) {
  char buf[] = "  x  ";
  ASSERT_FALSE(isBlanks(buf, 0, 5));
}

/* ========================================================================
 *  strcopy_safe
 * ======================================================================== */

void test_strcopy_safe_normal(void) {
  char dest[20];
  strcopy_safe(dest, "hello", sizeof(dest));
  ASSERT_STR_EQ("hello", dest);
}

void test_strcopy_safe_truncation(void) {
  char dest[4];
  strcopy_safe(dest, "hello world", sizeof(dest));
  ASSERT_STR_EQ("hel", dest);
}

void test_strcopy_safe_zero_size(void) {
  char dest[4] = "old";
  strcopy_safe(dest, "new", 0);
  ASSERT_STR_EQ("old", dest);  /* unchanged */
}

/* ========================================================================
 *  hexToDec / decToHex
 * ======================================================================== */

void test_hexToDec(void) {
  /* 0x98 with 2 digits -> decimal 98 */
  ASSERT_UINT_EQ(98, hexToDec(0x98, 2));
  ASSERT_UINT_EQ(12, hexToDec(0x12, 2));
  ASSERT_UINT_EQ(0,  hexToDec(0x00, 2));
  ASSERT_UINT_EQ(123, hexToDec(0x123, 3));
}

void test_decToHex(void) {
  /* decimal 98 -> 0x98 with 2 digits */
  ASSERT_UINT_EQ(0x98,  decToHex(98, 2));
  ASSERT_UINT_EQ(0x12,  decToHex(12, 2));
  ASSERT_UINT_EQ(0x00,  decToHex(0, 2));
  ASSERT_UINT_EQ(0x123, decToHex(123, 3));
}

void test_hexToDec_decToHex_roundtrip(void) {
  /* hexToDec(decToHex(x)) == x */
  for (unsigned int i = 0; i < 100; i++) {
    ASSERT_UINT_EQ(i, hexToDec(decToHex(i, 2), 2));
  }
}

/* ========================================================================
 *  compareSequences
 * ======================================================================== */

void test_compareSequences_equal(void) {
  ASSERT_INT_EQ(SEQ_EQUAL, compareSequences("abc", "abc", 3));
}

void test_compareSequences_more(void) {
  ASSERT_INT_EQ(SEQ_MORE, compareSequences("bcd", "abc", 3));
}

void test_compareSequences_less(void) {
  ASSERT_INT_EQ(SEQ_LESS, compareSequences("abc", "bcd", 3));
}

void test_compareSequences_null(void) {
  ASSERT_INT_EQ(SEQ_ERROR, compareSequences(NULL, "abc", 3));
  ASSERT_INT_EQ(SEQ_ERROR, compareSequences("abc", NULL, 3));
}

/* ========================================================================
 *  matchWithWildcards
 * ======================================================================== */

void test_wildcard_exact_match(void) {
  ASSERT_TRUE(matchWithWildcards("hello", 5, "hello", 5, 0));
}

void test_wildcard_no_match(void) {
  ASSERT_FALSE(matchWithWildcards("hello", 5, "world", 5, 0));
}

void test_wildcard_star_end(void) {
  ASSERT_TRUE(matchWithWildcards("hel*", 4, "hello", 5, 0));
}

void test_wildcard_star_start(void) {
  ASSERT_TRUE(matchWithWildcards("*llo", 4, "hello", 5, 0));
}

void test_wildcard_star_middle(void) {
  ASSERT_TRUE(matchWithWildcards("h*o", 3, "hello", 5, 0));
}

void test_wildcard_star_only(void) {
  ASSERT_TRUE(matchWithWildcards("*", 1, "anything", 8, 0));
}

void test_wildcard_percent_single_char(void) {
  ASSERT_TRUE(matchWithWildcards("h%llo", 5, "hello", 5, 0));
  ASSERT_FALSE(matchWithWildcards("h%lo", 4, "hello", 5, 0));
}

void test_wildcard_length_mismatch(void) {
  ASSERT_FALSE(matchWithWildcards("abc", 3, "ab", 2, 0));
  ASSERT_FALSE(matchWithWildcards("ab", 2, "abc", 3, 0));
}

/* ========================================================================
 *  stringIsDigit
 * ======================================================================== */

void test_stringIsDigit_true(void) {
  ASSERT_TRUE(stringIsDigit("12345"));
  ASSERT_TRUE(stringIsDigit("0"));
}

void test_stringIsDigit_false(void) {
  ASSERT_FALSE(stringIsDigit("12a45"));
  ASSERT_FALSE(stringIsDigit("abc"));
}

void test_stringIsDigit_empty(void) {
  /* Note: empty string returns TRUE per implementation (loop never executes) */
  ASSERT_TRUE(stringIsDigit(""));
}

/* ========================================================================
 *  trimRight
 * ======================================================================== */

void test_trimRight_trailing_spaces(void) {
  char buf[] = "hello   ";
  trimRight(buf, 8);
  ASSERT_STR_EQ("hello", buf);
}

void test_trimRight_no_trailing(void) {
  char buf[] = "hello";
  trimRight(buf, 5);
  ASSERT_STR_EQ("hello", buf);
}

void test_trimRight_all_spaces(void) {
  char buf[] = "     ";
  trimRight(buf, 5);
  ASSERT_STR_EQ("", buf);
}

/* ========================================================================
 *  Base64 encode / decode
 * ======================================================================== */

void test_base64_encode_decode_roundtrip(void) {
  const char *input = "Hello, World!";
  int inputLen = (int)strlen(input);
  char encoded[64];
  int encodedLen = 0;

  encodeBase64NoAlloc(input, inputLen, encoded, &encodedLen, 0);
  ASSERT_TRUE(encodedLen > 0);
  ASSERT_STR_EQ("SGVsbG8sIFdvcmxkIQ==", encoded);

  char decoded[64];
  int decodedLen = decodeBase64(encoded, decoded);
  ASSERT_INT_EQ(inputLen, decodedLen);
  ASSERT_MEM_EQ(input, decoded, inputLen);
}

void test_base64_empty(void) {
  char encoded[16];
  int encodedLen = 0;

  encodeBase64NoAlloc("", 0, encoded, &encodedLen, 0);
  ASSERT_INT_EQ(0, encodedLen);
}

void test_base64_padding_1(void) {
  /* input length 1 mod 3 -> 2 padding chars */
  const char *input = "A";
  char encoded[16];
  int encodedLen = 0;

  encodeBase64NoAlloc(input, 1, encoded, &encodedLen, 0);
  ASSERT_STR_EQ("QQ==", encoded);

  char decoded[16];
  int decodedLen = decodeBase64(encoded, decoded);
  ASSERT_INT_EQ(1, decodedLen);
  ASSERT_TRUE(decoded[0] == 'A');
}

void test_base64_padding_2(void) {
  /* input length 2 mod 3 -> 1 padding char */
  const char *input = "AB";
  char encoded[16];
  int encodedLen = 0;

  encodeBase64NoAlloc(input, 2, encoded, &encodedLen, 0);
  ASSERT_STR_EQ("QUI=", encoded);

  char decoded[16];
  int decodedLen = decodeBase64(encoded, decoded);
  ASSERT_INT_EQ(2, decodedLen);
  ASSERT_TRUE(decoded[0] == 'A');
  ASSERT_TRUE(decoded[1] == 'B');
}

void test_base64_no_padding(void) {
  /* input length 3 -> no padding */
  const char *input = "ABC";
  char encoded[16];
  int encodedLen = 0;

  encodeBase64NoAlloc(input, 3, encoded, &encodedLen, 0);
  ASSERT_STR_EQ("QUJD", encoded);

  char decoded[16];
  int decodedLen = decodeBase64(encoded, decoded);
  ASSERT_INT_EQ(3, decodedLen);
  ASSERT_MEM_EQ("ABC", decoded, 3);
}

void test_base64_binary_data(void) {
  /* Test with binary data (not printable) */
  char input[] = {0x00, 0x01, 0x02, (char)0xFF, (char)0xFE};
  char encoded[32];
  int encodedLen = 0;

  encodeBase64NoAlloc(input, 5, encoded, &encodedLen, 0);
  ASSERT_TRUE(encodedLen > 0);

  char decoded[32];
  int decodedLen = decodeBase64(encoded, decoded);
  ASSERT_INT_EQ(5, decodedLen);
  ASSERT_MEM_EQ(input, decoded, 5);
}

/* ========================================================================
 *  base64ToBase64url / base64urlToBase64
 * ======================================================================== */

void test_base64_to_base64url(void) {
  char s[] = "abc+def/ghi=";
  base64ToBase64url(s);
  ASSERT_STR_EQ("abc-def_ghi", s);
}

void test_base64url_to_base64(void) {
  char s[32] = "abc-def_ghi";
  base64urlToBase64(s, sizeof(s));
  /* Should have '+' and '/' restored, plus padding */
  ASSERT_TRUE(s[3] == '+');
  ASSERT_TRUE(s[7] == '/');
}

/* ========================================================================
 *  percentEncode
 * ======================================================================== */

void test_percentEncode_no_special(void) {
  char buf[64];
  int len = percentEncode("hello", buf, 5);
  buf[len] = '\0';
  ASSERT_STR_EQ("hello", buf);
}

void test_percentEncode_special_chars(void) {
  char buf[64];
  int len = percentEncode("a b", buf, 3);
  buf[len] = '\0';
  ASSERT_STR_EQ("a%20b", buf);
}

/* ========================================================================
 *  simpleHexFill / simpleHexPrint
 * ======================================================================== */

void test_simpleHexFill(void) {
  char buf[16];
  simpleHexFill(buf, 8, 0xFF);
  ASSERT_STR_EQ("000000FF", buf);
}

void test_simpleHexFill_zero(void) {
  char buf[16];
  simpleHexFill(buf, 4, 0);
  ASSERT_STR_EQ("0000", buf);
}

/* ========================================================================
 *  isCharAN - tests EBCDIC alphanumeric ranges (raw byte values)
 *
 *  This function is designed for EBCDIC data. We pass raw EBCDIC byte
 *  values directly so the tests are valid regardless of -qascii mode.
 * ======================================================================== */

void test_isCharAN_ebcdic_letters(void) {
  /* EBCDIC uppercase A=0xC1 .. I=0xC9, J=0xD1 .. R=0xD9, S=0xE2 .. Z=0xE9 */
  ASSERT_TRUE(isCharAN((char)0xC1));   /* EBCDIC 'A' */
  ASSERT_TRUE(isCharAN((char)0xC9));   /* EBCDIC 'I' */
  ASSERT_TRUE(isCharAN((char)0xD1));   /* EBCDIC 'J' */
  ASSERT_TRUE(isCharAN((char)0xD9));   /* EBCDIC 'R' */
  ASSERT_TRUE(isCharAN((char)0xE2));   /* EBCDIC 'S' */
  ASSERT_TRUE(isCharAN((char)0xE9));   /* EBCDIC 'Z' */
}

void test_isCharAN_ebcdic_lowercase(void) {
  /* EBCDIC lowercase a=0x81 .. i=0x89, j=0x91 .. r=0x99, s=0xA2 .. z=0xA9 */
  ASSERT_TRUE(isCharAN((char)0x81));   /* EBCDIC 'a' */
  ASSERT_TRUE(isCharAN((char)0x89));   /* EBCDIC 'i' */
  ASSERT_TRUE(isCharAN((char)0x91));   /* EBCDIC 'j' */
  ASSERT_TRUE(isCharAN((char)0x99));   /* EBCDIC 'r' */
  ASSERT_TRUE(isCharAN((char)0xA2));   /* EBCDIC 's' */
  ASSERT_TRUE(isCharAN((char)0xA9));   /* EBCDIC 'z' */
}

void test_isCharAN_ebcdic_digits(void) {
  /* EBCDIC 0=0xF0 .. 9=0xF9 */
  ASSERT_TRUE(isCharAN((char)0xF0));   /* EBCDIC '0' */
  ASSERT_TRUE(isCharAN((char)0xF5));   /* EBCDIC '5' */
  ASSERT_TRUE(isCharAN((char)0xF9));   /* EBCDIC '9' */
}

void test_isCharAN_ebcdic_non_alphanum(void) {
  ASSERT_FALSE(isCharAN((char)0x40));  /* EBCDIC space */
  ASSERT_FALSE(isCharAN((char)0x4B));  /* EBCDIC '.' */
  ASSERT_FALSE(isCharAN((char)0x00));  /* NUL */
  ASSERT_FALSE(isCharAN((char)0x7B));  /* EBCDIC '#' */
}

/* ========================================================================
 *  compareIgnoringCase - EBCDIC case-insensitive comparison
 *
 *  Uses EBCDIC byte values. The upchar() function converts EBCDIC
 *  lowercase to uppercase by OR-ing with 0x40.
 * ======================================================================== */

void test_compareIgnoringCase_ebcdic_equal(void) {
  /* "ABC" in EBCDIC */
  char upper[] = {(char)0xC1, (char)0xC2, (char)0xC3, 0};
  /* "abc" in EBCDIC */
  char lower[] = {(char)0x81, (char)0x82, (char)0x83, 0};
  ASSERT_INT_EQ(0, compareIgnoringCase(upper, lower, 3));
  ASSERT_INT_EQ(0, compareIgnoringCase(lower, upper, 3));
}

void test_compareIgnoringCase_ebcdic_different(void) {
  /* "ABD" vs "ABC" in EBCDIC */
  char s1[] = {(char)0xC1, (char)0xC2, (char)0xC4, 0};
  char s2[] = {(char)0xC1, (char)0xC2, (char)0xC3, 0};
  ASSERT_TRUE(compareIgnoringCase(s1, s2, 3) > 0);
  ASSERT_TRUE(compareIgnoringCase(s2, s1, 3) < 0);
}

/* ========================================================================
 *  main
 * ======================================================================== */

int main(void) {
  TEST_SUITE_START("utils");

  /* parseInt / parseInitialInt */
  RUN_TEST(test_parseInt_simple);
  RUN_TEST(test_parseInt_substring);
  RUN_TEST(test_parseInitialInt_stops_at_nondigit);

  /* indexOf / lastIndexOf */
  RUN_TEST(test_indexOf_found);
  RUN_TEST(test_indexOf_not_found);
  RUN_TEST(test_indexOf_with_start);
  RUN_TEST(test_lastIndexOf_found);
  RUN_TEST(test_lastIndexOf_not_found);

  /* indexOfString / lastIndexOfString */
  RUN_TEST(test_indexOfString_found);
  RUN_TEST(test_indexOfString_not_found);
  RUN_TEST(test_indexOfString_at_start);
  RUN_TEST(test_indexOfString_at_end);
  RUN_TEST(test_indexOfString_with_start_pos);
  RUN_TEST(test_lastIndexOfString_found);
  RUN_TEST(test_lastIndexOfString_not_found);

  /* isZeros / isBlanks */
  RUN_TEST(test_isZeros_true);
  RUN_TEST(test_isZeros_false);
  RUN_TEST(test_isZeros_offset);
  RUN_TEST(test_isBlanks_true);
  RUN_TEST(test_isBlanks_false);

  /* strcopy_safe */
  RUN_TEST(test_strcopy_safe_normal);
  RUN_TEST(test_strcopy_safe_truncation);
  RUN_TEST(test_strcopy_safe_zero_size);

  /* hexToDec / decToHex */
  RUN_TEST(test_hexToDec);
  RUN_TEST(test_decToHex);
  RUN_TEST(test_hexToDec_decToHex_roundtrip);

  /* compareSequences */
  RUN_TEST(test_compareSequences_equal);
  RUN_TEST(test_compareSequences_more);
  RUN_TEST(test_compareSequences_less);
  RUN_TEST(test_compareSequences_null);

  /* matchWithWildcards */
  RUN_TEST(test_wildcard_exact_match);
  RUN_TEST(test_wildcard_no_match);
  RUN_TEST(test_wildcard_star_end);
  RUN_TEST(test_wildcard_star_start);
  RUN_TEST(test_wildcard_star_middle);
  RUN_TEST(test_wildcard_star_only);
  RUN_TEST(test_wildcard_percent_single_char);
  RUN_TEST(test_wildcard_length_mismatch);

  /* stringIsDigit */
  RUN_TEST(test_stringIsDigit_true);
  RUN_TEST(test_stringIsDigit_false);
  RUN_TEST(test_stringIsDigit_empty);

  /* trimRight */
  RUN_TEST(test_trimRight_trailing_spaces);
  RUN_TEST(test_trimRight_no_trailing);
  RUN_TEST(test_trimRight_all_spaces);

  /* Base64 */
  RUN_TEST(test_base64_encode_decode_roundtrip);
  RUN_TEST(test_base64_empty);
  RUN_TEST(test_base64_padding_1);
  RUN_TEST(test_base64_padding_2);
  RUN_TEST(test_base64_no_padding);
  RUN_TEST(test_base64_binary_data);
  RUN_TEST(test_base64_to_base64url);
  RUN_TEST(test_base64url_to_base64);

  /* percentEncode */
  RUN_TEST(test_percentEncode_no_special);
  RUN_TEST(test_percentEncode_special_chars);

  /* simpleHexFill */
  RUN_TEST(test_simpleHexFill);
  RUN_TEST(test_simpleHexFill_zero);

  /* isCharAN - EBCDIC alphanumeric */
  RUN_TEST(test_isCharAN_ebcdic_letters);
  RUN_TEST(test_isCharAN_ebcdic_lowercase);
  RUN_TEST(test_isCharAN_ebcdic_digits);
  RUN_TEST(test_isCharAN_ebcdic_non_alphanum);

  /* compareIgnoringCase - EBCDIC */
  RUN_TEST(test_compareIgnoringCase_ebcdic_equal);
  RUN_TEST(test_compareIgnoringCase_ebcdic_different);

  TEST_SUITE_END();
  return TEST_SUITE_RC();
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
