

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZOWE_TESTS_H
#define ZOWE_TESTS_H

#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdarg.h>

/** \file
 *  \brief zowetests.h is a simple, mocha-inspired unit test framework for C on z/OS.
 *
 *  This framework is designed to work with the xlclang compiler and other C99-compatible
 *  compilers. It provides a familiar describe/it/assert structure similar to the popular
 *  JavaScript test framework Mocha.
 *
 *  Assertion failures cause a non-local jump (via setjmp/longjmp) to the end of the
 *  enclosing IT block, so you do not need to check return values after each assertion.
 *
 *  Functional coverage is tracked via TEST_COVERS() calls, giving you a report of which
 *  API functions were exercised by the test suite.
 *
 *  Usage example:
 *  \code{.c}
 *  #include "zowetests.h"
 *  #include "json.h"
 *  #include "utils.h"
 *
 *  static ShortLivedHeap *slh = NULL;
 *
 *  static void setupHeap(void) {
 *    slh = makeShortLivedHeap(0x10000, 100);
 *  }
 *
 *  static void teardownHeap(void) {
 *    SLHFree(slh);
 *    slh = NULL;
 *  }
 *
 *  int main(void) {
 *    zoweTestInit();
 *
 *    DESCRIBE("JSON parsing") {
 *      SET_BEFORE_EACH(setupHeap);
 *      SET_AFTER_EACH(teardownHeap);
 *
 *      IT("parses a null value") {
 *        TEST_COVERS(jsonParseString);
 *        TEST_COVERS(jsonIsNull);
 *        char errorBuf[256] = {0};
 *        Json *json = jsonParseString(slh, "null", errorBuf, sizeof(errorBuf));
 *        ASSERT_NOT_NULL(json);
 *        ASSERT_TRUE(jsonIsNull(json));
 *      } IT_END
 *
 *    } DESCRIBE_END
 *
 *    return ZOWE_TEST_REPORT();
 *  }
 *  \endcode
 */

/* Maximum buffer sizes used throughout the framework */
#define ZOWE_TEST_SUITE_NAME_MAX 128
#define ZOWE_TEST_CASE_NAME_MAX 256
#define ZOWE_TEST_FAILURE_MSG_MAX 1024
#define ZOWE_TEST_FAILURE_FILE_MAX 256
#define ZOWE_TEST_MAX_COVERED_FUNCTIONS 512
#define ZOWE_TEST_COVERED_FUNCTION_NAME_MAX 128

/**
 * \brief Internal context for an active test run. Do not access members directly.
 *
 * All state for the currently-running describe/it block is held here as a single
 * global instance. This avoids any dynamic allocation in the framework itself.
 */
typedef struct ZoweTestContext_tag {
  /* setjmp landing point for ASSERT_* failures inside an IT block */
  jmp_buf assertJumpBuf;

  /* Names of the active suite and test */
  char suiteName[ZOWE_TEST_SUITE_NAME_MAX];
  char testName[ZOWE_TEST_CASE_NAME_MAX];

  /* Failure detail captured by _zoweTestAssertFailV() */
  char failureMessage[ZOWE_TEST_FAILURE_MSG_MAX];
  char failureFile[ZOWE_TEST_FAILURE_FILE_MAX];
  int failureLine;

  /* Per-suite counters (reset by each DESCRIBE) */
  int suitePassed;
  int suiteFailed;
  int suiteAssertionCount;

  /* Lifetime totals accumulated across all DESCRIBE blocks */
  int totalPassed;
  int totalFailed;

  /* Assertion count for the IT block currently executing */
  int assertionCount;

  /* Hooks registered via SET_BEFORE_EACH / SET_AFTER_EACH */
  void (*beforeEach)(void);
  void (*afterEach)(void);

  /* Internal state flags */
  bool inTest;
  bool testFailed;

  /* Functional coverage tracking */
  char coveredFunctions[ZOWE_TEST_MAX_COVERED_FUNCTIONS][ZOWE_TEST_COVERED_FUNCTION_NAME_MAX];
  int coveredFunctionCount;
} ZoweTestContext;

/** The single global test context. Defined in zowetests.c. */
extern ZoweTestContext zoweTestCtx;

/**
 * \brief Initializes the test context. Must be called once before any DESCRIBE block.
 */
void zoweTestInit(void);

/* Internal functions used only by the macros below. Callers should not invoke these directly. */
void _zoweTestDescribeBegin(const char *name);
void _zoweTestDescribeEnd(void);
void _zoweTestItBegin(const char *name);
void _zoweTestItPassed(void);
void _zoweTestItFailed(void);
void _zoweTestAssertFailV(const char *file, int line, const char *format, ...);
void _zoweTestRecordCoverage(const char *functionName);
int _zoweTestFinalReport(void);

/* ============================================================
 *  Mocha-style structural macros
 * ============================================================ */

/**
 * \brief Begins a named test suite. Must be paired with DESCRIBE_END.
 *
 * Variables declared between DESCRIBE and DESCRIBE_END are visible to
 * all IT blocks within the same suite. Hook functions registered with
 * SET_BEFORE_EACH / SET_AFTER_EACH are cleared when the next DESCRIBE begins.
 */
#define DESCRIBE(name) \
  { \
    _zoweTestDescribeBegin(name);

/**
 * \brief Ends a test suite block opened by DESCRIBE.
 */
#define DESCRIBE_END \
    _zoweTestDescribeEnd(); \
  }

/**
 * \brief Registers a before-each hook for the current suite.
 *
 * fn must have the signature: void fn(void)
 * It is called before the body of each IT block in the enclosing DESCRIBE.
 */
#define SET_BEFORE_EACH(fn) \
  (zoweTestCtx.beforeEach = (fn))

/**
 * \brief Registers an after-each hook for the current suite.
 *
 * fn must have the signature: void fn(void)
 * It is called after each IT block, whether the test passed or failed.
 */
#define SET_AFTER_EACH(fn) \
  (zoweTestCtx.afterEach = (fn))

/**
 * \brief Begins a single named test case. Must be paired with IT_END.
 *
 * If an ASSERT_* macro fails inside this block, execution immediately jumps
 * to the corresponding IT_END and the test is recorded as failed. All code
 * after the failing assertion within the block is skipped.
 */
#define IT(name) \
  _zoweTestItBegin(name); \
  if (setjmp(zoweTestCtx.assertJumpBuf) == 0) {

/**
 * \brief Ends a test case block opened by IT.
 */
#define IT_END \
    _zoweTestItPassed(); \
  } else { \
    _zoweTestItFailed(); \
  }

/**
 * \brief Records that the current IT block exercises a specific API function.
 *
 * Call this macro with the bare function name (no quotes) once per tested
 * function. Duplicate names are suppressed. The final report lists all
 * functions that were exercised at least once across the entire test run.
 *
 * Example: TEST_COVERS(jsonParseString);
 */
#define TEST_COVERS(funcName) \
  _zoweTestRecordCoverage(#funcName)

/**
 * \brief Returns an exit code and prints the final results summary.
 *
 * Returns 0 if every test passed, or 1 if at least one test failed.
 * Use as the return value of main():  return ZOWE_TEST_REPORT();
 */
#define ZOWE_TEST_REPORT() \
  _zoweTestFinalReport()

/* ============================================================
 *  Assertion macros
 * ============================================================ */

/**
 * \brief Asserts that condition evaluates to true (non-zero).
 */
#define ASSERT_TRUE(condition) \
  do { \
    zoweTestCtx.assertionCount++; \
    if (!(condition)) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, "Expected true: %s", #condition); \
    } \
  } while (0)

/**
 * \brief Asserts that condition evaluates to false (zero).
 */
#define ASSERT_FALSE(condition) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((condition)) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, "Expected false: %s", #condition); \
    } \
  } while (0)

/**
 * \brief Asserts that ptr is not NULL.
 */
#define ASSERT_NOT_NULL(ptr) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((ptr) == NULL) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, "Expected non-NULL pointer: %s", #ptr); \
    } \
  } while (0)

/**
 * \brief Asserts that ptr is NULL.
 */
#define ASSERT_NULL(ptr) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((ptr) != NULL) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, "Expected NULL pointer: %s", #ptr); \
    } \
  } while (0)

/**
 * \brief Asserts that two integer expressions are equal.
 *
 * Both sides are cast to int for comparison and display.
 */
#define ASSERT_EQUAL_INT(actual, expected) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((int)(actual) != (int)(expected)) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "Expected %s == %s, but got %d != %d", \
          #actual, #expected, (int)(actual), (int)(expected)); \
    } \
  } while (0)

/**
 * \brief Asserts that two integer expressions are not equal.
 */
#define ASSERT_NOT_EQUAL_INT(actual, unexpected) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((int)(actual) == (int)(unexpected)) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "Expected %s != %s, but both were %d", \
          #actual, #unexpected, (int)(actual)); \
    } \
  } while (0)

/**
 * \brief Asserts that actual > threshold (integer comparison).
 */
#define ASSERT_GT_INT(actual, threshold) \
  do { \
    zoweTestCtx.assertionCount++; \
    if (!((int)(actual) > (int)(threshold))) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "Expected %s > %s, but got %d <= %d", \
          #actual, #threshold, (int)(actual), (int)(threshold)); \
    } \
  } while (0)

/**
 * \brief Asserts that actual < threshold (integer comparison).
 */
#define ASSERT_LT_INT(actual, threshold) \
  do { \
    zoweTestCtx.assertionCount++; \
    if (!((int)(actual) < (int)(threshold))) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "Expected %s < %s, but got %d >= %d", \
          #actual, #threshold, (int)(actual), (int)(threshold)); \
    } \
  } while (0)

/**
 * \brief Asserts that two null-terminated strings are equal via strcmp.
 *
 * Both pointers must be non-NULL; a NULL pointer triggers a failure.
 */
#define ASSERT_EQUAL_STR(actual, expected) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((actual) == NULL || (expected) == NULL) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "ASSERT_EQUAL_STR: NULL pointer - actual=%s expected=%s", \
          (actual) ? (actual) : "<NULL>", (expected) ? (expected) : "<NULL>"); \
    } else if (strcmp((actual), (expected)) != 0) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "Expected \"%s\" but got \"%s\"", (expected), (actual)); \
    } \
  } while (0)

/**
 * \brief Asserts that haystack contains the substring needle.
 *
 * haystack must be non-NULL; needle must be non-NULL.
 */
#define ASSERT_STR_CONTAINS(haystack, needle) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((haystack) == NULL) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "ASSERT_STR_CONTAINS: haystack is NULL (needle was \"%s\")", (needle)); \
    } else if (strstr((haystack), (needle)) == NULL) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "Expected string to contain \"%s\" but got: \"%s\"", (needle), (haystack)); \
    } \
  } while (0)

/**
 * \brief Asserts that haystack does not contain the substring needle.
 *
 * A NULL haystack is treated as not containing anything (assertion passes).
 */
#define ASSERT_STR_NOT_CONTAINS(haystack, needle) \
  do { \
    zoweTestCtx.assertionCount++; \
    if ((haystack) != NULL && strstr((haystack), (needle)) != NULL) { \
      _zoweTestAssertFailV(__FILE__, __LINE__, \
          "Expected string NOT to contain \"%s\" but it did: \"%s\"", \
          (needle), (haystack)); \
    } \
  } while (0)

/**
 * \brief Immediately fails the current test with a custom message string.
 */
#define FAIL(message) \
  do { \
    zoweTestCtx.assertionCount++; \
    _zoweTestAssertFailV(__FILE__, __LINE__, "%s", (message)); \
  } while (0)

#endif /* ZOWE_TESTS_H */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
