/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * safe-strings-test.c -- strncpySafe / strncatSafe / strlenSafe.
 *
 * Every buffer here is allocated exactly as large as the size passed to the
 * function under test, so a single byte written past the end is a heap
 * overflow the sanitizer will catch. Run under -fsanitize=address.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "utils.h"

static int failures = 0;

static void check(int passed, const char *name, const char *detail) {
  printf("%-4s %s\n", passed ? "PASS" : "FAIL", name);
  if (!passed && detail != NULL) {
    printf("       %s\n", detail);
  }
  if (!passed) {
    failures++;
  }
}

/* Exact-sized heap buffer so ASan sees any off-by-one. */
static char *exactBuffer(int size) {
  char *b = (char *)malloc(size);
  memset(b, 'Z', size);
  return b;
}

static void testStrlenSafe(void) {
  char *b = exactBuffer(4);
  memcpy(b, "abc", 3);
  b[3] = '\0';
  check(strlenSafe(b, 4) == 3, "strlenSafe counts a terminated string", NULL);
  check(strlenSafe(b, 2) == 2, "strlenSafe stops at maxLength", NULL);
  free(b);

  b = exactBuffer(4);            /* all 'Z', no terminator anywhere */
  check(strlenSafe(b, 4) == 4, "strlenSafe returns maxLength when unterminated",
        "must not read past the buffer looking for a terminator");
  free(b);

  check(strlenSafe(NULL, 8) == 0, "strlenSafe tolerates NULL", NULL);
  check(strlenSafe("abc", 0) == 0, "strlenSafe tolerates maxLength 0", NULL);
}

static void testStrncpySafe(void) {
  char *b = exactBuffer(8);
  check(strncpySafe(b, 8, "abc", 8) == 3 && strcmp(b, "abc") == 0,
        "strncpySafe copies and terminates", NULL);
  free(b);

  b = exactBuffer(8);
  check(strncpySafe(b, 8, "abcdef", 3) == 3 && strcmp(b, "abc") == 0,
        "strncpySafe honours count when dest has room",
        "stopping at count is the caller's request, not a failure");
  free(b);

  b = exactBuffer(4);
  check(strncpySafe(b, 4, "abcdef", 8) == -1 && strcmp(b, "abc") == 0,
        "strncpySafe reports -1 when dest runs out first",
        "expected -1 with dest holding \"abc\"");
  free(b);

  b = exactBuffer(4);
  check(strncpySafe(b, 4, "abc", 8) == 3 && strcmp(b, "abc") == 0,
        "strncpySafe handles an exact fit", NULL);
  free(b);

  b = exactBuffer(4);
  check(strncpySafe(b, 4, NULL, 8) == 0 && b[0] == '\0',
        "strncpySafe with NULL source empties dest", NULL);
  free(b);

  check(strncpySafe(NULL, 4, "abc", 4) == -1, "strncpySafe tolerates NULL dest", NULL);
  b = exactBuffer(4);
  check(strncpySafe(b, 0, "abc", 4) == -1, "strncpySafe rejects destSize 0",
        "must not write when told the buffer has no room");
  free(b);
}

static void testStrcpySafe(void) {
  char *b = exactBuffer(8);
  check(strcpySafe(b, 8, "abc") == 3 && strcmp(b, "abc") == 0,
        "strcpySafe copies a whole string", NULL);
  free(b);

  b = exactBuffer(4);
  check(strcpySafe(b, 4, "abcdef") == -1 && strcmp(b, "abc") == 0,
        "strcpySafe reports -1 on truncation and still terminates", NULL);
  free(b);
}

static void testStrncatSafe(void) {
  char *b = exactBuffer(8);
  strcpySafe(b, 8, "ab");
  check(strncatSafe(b, 8, "cd", 8) == 4 && strcmp(b, "abcd") == 0,
        "strncatSafe appends and terminates", NULL);
  free(b);

  b = exactBuffer(8);
  strcpySafe(b, 8, "ab");
  check(strncatSafe(b, 8, "cdef", 2) == 4 && strcmp(b, "abcd") == 0,
        "strncatSafe honours count when dest has room",
        "stopping at count is the caller's request, not a failure");
  free(b);

  b = exactBuffer(5);
  strcpySafe(b, 5, "ab");
  check(strncatSafe(b, 5, "cdef", 8) == -1 && strcmp(b, "abcd") == 0,
        "strncatSafe reports -1 when dest runs out first",
        "expected -1 with dest holding \"abcd\"");
  free(b);

  b = exactBuffer(5);
  strcpySafe(b, 5, "abcd");
  check(strncatSafe(b, 5, "e", 8) == -1 && strcmp(b, "abcd") == 0,
        "strncatSafe on a full dest appends nothing", NULL);
  free(b);

  b = exactBuffer(4);            /* unterminated dest */
  check(strncatSafe(b, 4, "x", 8) == -1,
        "strncatSafe refuses an unterminated dest",
        "appending would have to read past the buffer");
  free(b);

  b = exactBuffer(8);
  strcpySafe(b, 8, "ab");
  check(strncatSafe(b, 8, NULL, 8) == 2 && strcmp(b, "ab") == 0,
        "strncatSafe with NULL source leaves dest alone", NULL);
  free(b);
}

static void testStrcatSafe(void) {
  char *b = exactBuffer(8);
  strcpySafe(b, 8, "ab");
  check(strcatSafe(b, 8, "cd") == 4 && strcmp(b, "abcd") == 0,
        "strcatSafe appends a whole string", NULL);
  free(b);

  b = exactBuffer(5);
  strcpySafe(b, 5, "ab");
  check(strcatSafe(b, 5, "cdef") == -1 && strcmp(b, "abcd") == 0,
        "strcatSafe reports -1 on truncation and still terminates", NULL);
  free(b);
}

int main(int argc, char **argv) {
  printf("\nbounded string helpers (utils.c)\n\n");
  testStrlenSafe();
  testStrncpySafe();
  testStrcpySafe();
  testStrncatSafe();
  testStrcatSafe();
  printf("\n%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
