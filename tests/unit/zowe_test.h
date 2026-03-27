/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * zowe_test.h - Minimal C unit test framework for Zowe Common C
 *
 * Usage:
 *   #include "zowe_test.h"
 *
 *   void test_something(void) {
 *     ASSERT_INT_EQ(42, myFunction());
 *     ASSERT_STR_EQ("hello", myOther());
 *   }
 *
 *   int main(void) {
 *     TEST_SUITE_START("My Tests");
 *     RUN_TEST(test_something);
 *     TEST_SUITE_END();
 *     return TEST_SUITE_RC();
 *   }
 */

#ifndef ZOWE_TEST_H
#define ZOWE_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Counters */
static int zt_tests_run = 0;
static int zt_tests_passed = 0;
static int zt_tests_failed = 0;
static int zt_current_test_failed = 0;
static const char *zt_current_test_name = "";

#define TEST_SUITE_START(name)                                  \
  do {                                                          \
    printf("=== Test Suite: %s ===\n", (name));                 \
    zt_tests_run = 0;                                           \
    zt_tests_passed = 0;                                        \
    zt_tests_failed = 0;                                        \
  } while (0)

#define TEST_SUITE_END()                                        \
  do {                                                          \
    printf("\n--- Results ---\n");                               \
    printf("  Total:  %d\n", zt_tests_run);                     \
    printf("  Passed: %d\n", zt_tests_passed);                  \
    printf("  Failed: %d\n", zt_tests_failed);                  \
    if (zt_tests_failed > 0) {                                  \
      printf("FAILED\n");                                       \
    } else {                                                    \
      printf("ALL PASSED\n");                                   \
    }                                                           \
  } while (0)

#define TEST_SUITE_RC() (zt_tests_failed > 0 ? 1 : 0)

#define RUN_TEST(func)                                          \
  do {                                                          \
    zt_current_test_failed = 0;                                 \
    zt_current_test_name = #func;                               \
    zt_tests_run++;                                             \
    func();                                                     \
    if (zt_current_test_failed) {                               \
      zt_tests_failed++;                                        \
    } else {                                                    \
      zt_tests_passed++;                                        \
      printf("  PASS: %s\n", #func);                            \
    }                                                           \
  } while (0)

/* Assertion macros */

#define ASSERT_FAIL(msg)                                        \
  do {                                                          \
    printf("  FAIL: %s\n    %s:%d: %s\n",                       \
           zt_current_test_name, __FILE__, __LINE__, (msg));    \
    zt_current_test_failed = 1;                                 \
    return;                                                     \
  } while (0)

#define ASSERT_TRUE(cond)                                       \
  do {                                                          \
    if (!(cond)) {                                              \
      printf("  FAIL: %s\n    %s:%d: expected true: %s\n",      \
             zt_current_test_name, __FILE__, __LINE__, #cond);  \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#define ASSERT_FALSE(cond)                                      \
  do {                                                          \
    if ((cond)) {                                               \
      printf("  FAIL: %s\n    %s:%d: expected false: %s\n",     \
             zt_current_test_name, __FILE__, __LINE__, #cond);  \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#define ASSERT_INT_EQ(expected, actual)                          \
  do {                                                          \
    int _e = (expected);                                        \
    int _a = (actual);                                          \
    if (_e != _a) {                                             \
      printf("  FAIL: %s\n    %s:%d: expected %d, got %d\n",    \
             zt_current_test_name, __FILE__, __LINE__, _e, _a); \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#define ASSERT_UINT_EQ(expected, actual)                         \
  do {                                                          \
    unsigned int _e = (expected);                                \
    unsigned int _a = (actual);                                  \
    if (_e != _a) {                                             \
      printf("  FAIL: %s\n    %s:%d: expected %u, got %u\n",    \
             zt_current_test_name, __FILE__, __LINE__, _e, _a); \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#define ASSERT_PTR_NULL(ptr)                                    \
  do {                                                          \
    if ((ptr) != NULL) {                                        \
      printf("  FAIL: %s\n    %s:%d: expected NULL\n",           \
             zt_current_test_name, __FILE__, __LINE__);         \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#define ASSERT_PTR_NOT_NULL(ptr)                                \
  do {                                                          \
    if ((ptr) == NULL) {                                        \
      printf("  FAIL: %s\n    %s:%d: expected non-NULL\n",       \
             zt_current_test_name, __FILE__, __LINE__);         \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#define ASSERT_STR_EQ(expected, actual)                          \
  do {                                                          \
    const char *_e = (expected);                                \
    const char *_a = (actual);                                  \
    if (_e == NULL && _a == NULL) break;                        \
    if (_e == NULL || _a == NULL || strcmp(_e, _a) != 0) {      \
      printf("  FAIL: %s\n    %s:%d: expected \"%s\", got \"%s\"\n", \
             zt_current_test_name, __FILE__, __LINE__,          \
             _e ? _e : "(null)", _a ? _a : "(null)");           \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#define ASSERT_MEM_EQ(expected, actual, len)                     \
  do {                                                          \
    if (memcmp((expected), (actual), (len)) != 0) {             \
      printf("  FAIL: %s\n    %s:%d: memory not equal (%d bytes)\n", \
             zt_current_test_name, __FILE__, __LINE__, (int)(len)); \
      zt_current_test_failed = 1;                               \
      return;                                                   \
    }                                                           \
  } while (0)

#endif /* ZOWE_TEST_H */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
