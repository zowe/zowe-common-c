

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <setjmp.h>
#include <time.h>

/*
 * Prevent the leak-wrap macros from redefining safeMalloc/safeFree inside
 * this translation unit — we call the REAL versions here.
 */
#define ZOWE_TEST_NO_LEAK_WRAP

#include "zowetests.h"
#include "logging.h"

/* The single global test context instance */
ZoweTestContext zoweTestCtx;

void zoweTestInit(void) {
  memset(&zoweTestCtx, 0, sizeof(ZoweTestContext));

  /*
   * Check environment variable for JUnit output path.
   */
  const char *junitEnv = getenv(ZOWE_TEST_JUNIT_ENV);
  if (junitEnv != NULL && junitEnv[0] != '\0') {
    strncpy(zoweTestCtx.junitOutputPath, junitEnv, sizeof(zoweTestCtx.junitOutputPath) - 1);
    zoweTestCtx.junitOutputPath[sizeof(zoweTestCtx.junitOutputPath) - 1] = '\0';
    zoweTestCtx.junitEnabled = true;
  }

  /*
   * Initialize the logging subsystem so that any library code that calls
   * zowelog(NULL, ...) does not crash with a protection exception (0C4).
   * Without this, getLoggingContext() returns NULL and the first access
   * of context->zoweAnchor segfaults.
   *
   * No logging components are configured here, so all zowelog calls will
   * find their component's currentDetailLevel at 0 (ZOWE_LOG_NA) and
   * return immediately without producing any output.  Tests that want
   * log output can configure components after calling zoweTestInit().
   */
  LoggingContext *lctx = makeLoggingContext();
  logConfigureStandardDestinations(lctx);
}

void zoweTestEnableJUnit(const char *outputPath) {
  if (outputPath != NULL && outputPath[0] != '\0') {
    strncpy(zoweTestCtx.junitOutputPath, outputPath, sizeof(zoweTestCtx.junitOutputPath) - 1);
    zoweTestCtx.junitOutputPath[sizeof(zoweTestCtx.junitOutputPath) - 1] = '\0';
  }
  zoweTestCtx.junitEnabled = true;
}

void zoweTestEnableLeakDetection(void) {
  zoweTestCtx.leakDetector.enabled = true;
}

void _zoweTestDescribeBegin(const char *name) {
  strncpy(zoweTestCtx.suiteName, name, ZOWE_TEST_SUITE_NAME_MAX - 1);
  zoweTestCtx.suiteName[ZOWE_TEST_SUITE_NAME_MAX - 1] = '\0';

  zoweTestCtx.suitePassed = 0;
  zoweTestCtx.suiteFailed = 0;
  zoweTestCtx.suiteSkipped = 0;
  zoweTestCtx.suiteAssertionCount = 0;
  zoweTestCtx.beforeEach = NULL;
  zoweTestCtx.afterEach = NULL;

  printf("\n  %s\n", name);
}

void _zoweTestDescribeEnd(void) {
  printf("\n    %d passing, %d failing, %d skipped (%d assertion%s)\n",
      zoweTestCtx.suitePassed,
      zoweTestCtx.suiteFailed,
      zoweTestCtx.suiteSkipped,
      zoweTestCtx.suiteAssertionCount,
      zoweTestCtx.suiteAssertionCount == 1 ? "" : "s");

  zoweTestCtx.totalPassed  += zoweTestCtx.suitePassed;
  zoweTestCtx.totalFailed  += zoweTestCtx.suiteFailed;
  zoweTestCtx.totalSkipped += zoweTestCtx.suiteSkipped;
}

void _zoweTestItBegin(const char *name) {
  strncpy(zoweTestCtx.testName, name, ZOWE_TEST_CASE_NAME_MAX - 1);
  zoweTestCtx.testName[ZOWE_TEST_CASE_NAME_MAX - 1] = '\0';

  zoweTestCtx.testFailed = false;
  zoweTestCtx.assertionCount = 0;
  zoweTestCtx.failureMessage[0] = '\0';
  zoweTestCtx.failureFile[0] = '\0';
  zoweTestCtx.failureLine = 0;
  zoweTestCtx.inTest = true;
  zoweTestCtx.testStartTime = clock();

  /* Reset leak detector for this test */
  _zoweTestLeakReset();

  if (zoweTestCtx.beforeEach != NULL) {
    zoweTestCtx.beforeEach();
  }
}

static void _zoweTestRecordResult(ZoweTestStatus status) {
  if (zoweTestCtx.resultCount >= ZOWE_TEST_MAX_RESULTS) {
    return;
  }
  ZoweTestResult *r = &zoweTestCtx.results[zoweTestCtx.resultCount];
  strncpy(r->suiteName, zoweTestCtx.suiteName, ZOWE_TEST_SUITE_NAME_MAX - 1);
  r->suiteName[ZOWE_TEST_SUITE_NAME_MAX - 1] = '\0';
  strncpy(r->testName, zoweTestCtx.testName, ZOWE_TEST_CASE_NAME_MAX - 1);
  r->testName[ZOWE_TEST_CASE_NAME_MAX - 1] = '\0';
  r->status = status;
  r->assertionCount = zoweTestCtx.assertionCount;
  r->failureLine = zoweTestCtx.failureLine;
  strncpy(r->failureMessage, zoweTestCtx.failureMessage, ZOWE_TEST_FAILURE_MSG_MAX - 1);
  r->failureMessage[ZOWE_TEST_FAILURE_MSG_MAX - 1] = '\0';
  strncpy(r->failureFile, zoweTestCtx.failureFile, ZOWE_TEST_FAILURE_FILE_MAX - 1);
  r->failureFile[ZOWE_TEST_FAILURE_FILE_MAX - 1] = '\0';

  clock_t endTime = clock();
  r->durationSec = (double)(endTime - zoweTestCtx.testStartTime) / CLOCKS_PER_SEC;

  zoweTestCtx.resultCount++;
}

void _zoweTestItPassed(void) {
  int leaks;

  zoweTestCtx.inTest = false;
  zoweTestCtx.suitePassed++;
  zoweTestCtx.suiteAssertionCount += zoweTestCtx.assertionCount;

  /* Check for memory leaks before reporting pass */
  leaks = _zoweTestLeakCheck();

  if (leaks > 0) {
    printf("    (/) %s (%d assertion%s) [WARNING: %d leak(s), %d bytes]\n",
        zoweTestCtx.testName,
        zoweTestCtx.assertionCount,
        zoweTestCtx.assertionCount == 1 ? "" : "s",
        zoweTestCtx.leakDetector.leaksDetected,
        zoweTestCtx.leakDetector.bytesLeaked);
  } else {
    printf("    (/) %s (%d assertion%s)\n",
        zoweTestCtx.testName,
        zoweTestCtx.assertionCount,
        zoweTestCtx.assertionCount == 1 ? "" : "s");
  }

  _zoweTestRecordResult(ZOWE_TEST_STATUS_PASSED);

  if (zoweTestCtx.afterEach != NULL) {
    zoweTestCtx.afterEach();
  }
}

void _zoweTestItFailed(void) {
  int leaks;

  zoweTestCtx.inTest = false;
  zoweTestCtx.suiteFailed++;
  zoweTestCtx.suiteAssertionCount += zoweTestCtx.assertionCount;

  printf("    (X) %s\n", zoweTestCtx.testName);
  printf("        %s:%d\n", zoweTestCtx.failureFile, zoweTestCtx.failureLine);
  printf("        AssertionError: %s\n", zoweTestCtx.failureMessage);

  /* Check for memory leaks */
  leaks = _zoweTestLeakCheck();
  if (leaks > 0) {
    printf("        [WARNING: %d leak(s), %d bytes]\n",
        zoweTestCtx.leakDetector.leaksDetected,
        zoweTestCtx.leakDetector.bytesLeaked);
  }

  _zoweTestRecordResult(ZOWE_TEST_STATUS_FAILED);

  if (zoweTestCtx.afterEach != NULL) {
    zoweTestCtx.afterEach();
  }
}

void _zoweTestPrepareSkip(const char *msg) {
  strncpy(zoweTestCtx.skipMessage, msg, ZOWE_TEST_FAILURE_MSG_MAX - 1);
  zoweTestCtx.skipMessage[ZOWE_TEST_FAILURE_MSG_MAX - 1] = '\0';
}

void _zoweTestItSkipped(void) {
  zoweTestCtx.inTest = false;
  zoweTestCtx.suiteSkipped++;
  zoweTestCtx.suiteAssertionCount += zoweTestCtx.assertionCount;

  printf("    (-) %s (skipped: %s)\n",
      zoweTestCtx.testName, zoweTestCtx.skipMessage);

  _zoweTestRecordResult(ZOWE_TEST_STATUS_SKIPPED);

  if (zoweTestCtx.afterEach != NULL) {
    zoweTestCtx.afterEach();
  }
}

void _zoweTestAssertFailV(const char *file, int line, const char *format, ...) {
  va_list args;
  int filePathLen;
  const char *shortFile;

  /*
   * Extract just the basename from the file path so that failure messages
   * are readable even when the compiler provides a full absolute path.
   */
  filePathLen = (int)strlen(file);
  shortFile = file;
  for (int i = filePathLen - 1; i >= 0; i--) {
    if (file[i] == '/' || file[i] == '\\') {
      shortFile = file + i + 1;
      break;
    }
  }

  strncpy(zoweTestCtx.failureFile, shortFile, ZOWE_TEST_FAILURE_FILE_MAX - 1);
  zoweTestCtx.failureFile[ZOWE_TEST_FAILURE_FILE_MAX - 1] = '\0';
  zoweTestCtx.failureLine = line;
  zoweTestCtx.testFailed = true;

  va_start(args, format);
  vsnprintf(zoweTestCtx.failureMessage, ZOWE_TEST_FAILURE_MSG_MAX - 1, format, args);
  va_end(args);
  zoweTestCtx.failureMessage[ZOWE_TEST_FAILURE_MSG_MAX - 1] = '\0';

  /* Jump back to the IT macro's setjmp call site, signalling failure */
  longjmp(zoweTestCtx.assertJumpBuf, 1);
  /* unreachable */
}

void _zoweTestRecordCoverage(const char *functionName) {
  int i;

  if (zoweTestCtx.coveredFunctionCount >= ZOWE_TEST_MAX_COVERED_FUNCTIONS) {
    return;
  }

  /* Avoid recording the same function name more than once */
  for (i = 0; i < zoweTestCtx.coveredFunctionCount; i++) {
    if (strncmp(zoweTestCtx.coveredFunctions[i], functionName,
                ZOWE_TEST_COVERED_FUNCTION_NAME_MAX) == 0) {
      return;
    }
  }

  strncpy(zoweTestCtx.coveredFunctions[zoweTestCtx.coveredFunctionCount],
      functionName, ZOWE_TEST_COVERED_FUNCTION_NAME_MAX - 1);
  zoweTestCtx.coveredFunctions[zoweTestCtx.coveredFunctionCount][ZOWE_TEST_COVERED_FUNCTION_NAME_MAX - 1] = '\0';
  zoweTestCtx.coveredFunctionCount++;
}

int _zoweTestFinalReport(void) {
  int total = zoweTestCtx.totalPassed + zoweTestCtx.totalFailed + zoweTestCtx.totalSkipped;
  int i;

  printf("\n");
  printf("  ======================================\n");
  printf("  Test Results\n");
  printf("  ======================================\n");
  printf("  Total:   %d\n", total);
  printf("  Passing: %d\n", zoweTestCtx.totalPassed);
  printf("  Failing: %d\n", zoweTestCtx.totalFailed);
  printf("  Skipped: %d\n", zoweTestCtx.totalSkipped);

  if (zoweTestCtx.coveredFunctionCount > 0) {
    printf("\n  Functions exercised by these tests (%d):\n",
        zoweTestCtx.coveredFunctionCount);
    for (i = 0; i < zoweTestCtx.coveredFunctionCount; i++) {
      printf("    - %s\n", zoweTestCtx.coveredFunctions[i]);
    }
  }

  printf("  ======================================\n");

  /* Write JUnit XML if enabled */
  if (zoweTestCtx.junitEnabled) {
    _zoweTestWriteJUnitXML();
  }

  return (zoweTestCtx.totalFailed > 0) ? 1 : 0;
}

/* ============================================================
 *  JUnit XML Output
 *
 *  Writes a standard JUnit XML report consumable by Jenkins, GitHub Actions,
 *  Azure DevOps, and other CI systems. The format follows the de-facto
 *  standard from Apache Ant / Maven Surefire.
 * ============================================================ */

/* Helper to XML-escape a string into a fixed buffer */
static void _xmlEscape(const char *src, char *dest, int destSize) {
  int di = 0;
  if (src == NULL) {
    dest[0] = '\0';
    return;
  }
  for (int si = 0; src[si] != '\0' && di < destSize - 6; si++) {
    switch (src[si]) {
      case '&':  memcpy(dest + di, "&amp;", 5);  di += 5; break;
      case '<':  memcpy(dest + di, "&lt;", 4);   di += 4; break;
      case '>':  memcpy(dest + di, "&gt;", 4);   di += 4; break;
      case '"':  memcpy(dest + di, "&quot;", 6); di += 6; break;
      case '\'': memcpy(dest + di, "&apos;", 6); di += 6; break;
      default:   dest[di++] = src[si]; break;
    }
  }
  dest[di] = '\0';
}

void _zoweTestWriteJUnitXML(void) {
  FILE *fp;
  char escapedSuite[ZOWE_TEST_SUITE_NAME_MAX * 6];
  char escapedTest[ZOWE_TEST_CASE_NAME_MAX * 6];
  char escapedMsg[ZOWE_TEST_FAILURE_MSG_MAX * 6];
  int i;
  int totalTests = zoweTestCtx.totalPassed + zoweTestCtx.totalFailed + zoweTestCtx.totalSkipped;
  double totalTime = 0.0;

  if (zoweTestCtx.junitOutputPath[0] == '\0') {
    /* Default filename if not specified */
    strncpy(zoweTestCtx.junitOutputPath, "test-results.xml",
            sizeof(zoweTestCtx.junitOutputPath) - 1);
  }

  fp = fopen(zoweTestCtx.junitOutputPath, "w");
  if (fp == NULL) {
    printf("  [WARN] Could not open JUnit output file: %s\n", zoweTestCtx.junitOutputPath);
    return;
  }

  /* Calculate total time */
  for (i = 0; i < zoweTestCtx.resultCount; i++) {
    totalTime += zoweTestCtx.results[i].durationSec;
  }

  fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(fp, "<testsuites tests=\"%d\" failures=\"%d\" skipped=\"%d\" time=\"%.3f\">\n",
      totalTests, zoweTestCtx.totalFailed, zoweTestCtx.totalSkipped, totalTime);

  /*
   * Group results by suite name. We iterate the results array and detect
   * suite boundaries by name changes.
   */
  i = 0;
  while (i < zoweTestCtx.resultCount) {
    const char *currentSuite = zoweTestCtx.results[i].suiteName;
    int suiteStart = i;
    int suitePassed = 0, suiteFailed = 0, suiteSkipped = 0;
    double suiteTime = 0.0;

    /* Count tests in this suite */
    while (i < zoweTestCtx.resultCount &&
           strcmp(zoweTestCtx.results[i].suiteName, currentSuite) == 0) {
      switch (zoweTestCtx.results[i].status) {
        case ZOWE_TEST_STATUS_PASSED:  suitePassed++;  break;
        case ZOWE_TEST_STATUS_FAILED:  suiteFailed++;  break;
        case ZOWE_TEST_STATUS_SKIPPED: suiteSkipped++; break;
      }
      suiteTime += zoweTestCtx.results[i].durationSec;
      i++;
    }

    int suiteTests = suitePassed + suiteFailed + suiteSkipped;
    _xmlEscape(currentSuite, escapedSuite, sizeof(escapedSuite));

    fprintf(fp, "  <testsuite name=\"%s\" tests=\"%d\" failures=\"%d\" skipped=\"%d\" time=\"%.3f\">\n",
        escapedSuite, suiteTests, suiteFailed, suiteSkipped, suiteTime);

    /* Write individual test cases */
    for (int j = suiteStart; j < suiteStart + suiteTests; j++) {
      ZoweTestResult *r = &zoweTestCtx.results[j];
      _xmlEscape(r->testName, escapedTest, sizeof(escapedTest));

      fprintf(fp, "    <testcase name=\"%s\" classname=\"%s\" time=\"%.3f\" assertions=\"%d\"",
          escapedTest, escapedSuite, r->durationSec, r->assertionCount);

      if (r->status == ZOWE_TEST_STATUS_PASSED) {
        fprintf(fp, " />\n");
      } else if (r->status == ZOWE_TEST_STATUS_SKIPPED) {
        fprintf(fp, ">\n");
        fprintf(fp, "      <skipped />\n");
        fprintf(fp, "    </testcase>\n");
      } else {
        _xmlEscape(r->failureMessage, escapedMsg, sizeof(escapedMsg));
        fprintf(fp, ">\n");
        fprintf(fp, "      <failure message=\"%s\" type=\"AssertionError\">%s:%d: %s</failure>\n",
            escapedMsg, r->failureFile, r->failureLine, escapedMsg);
        fprintf(fp, "    </testcase>\n");
      }
    }

    fprintf(fp, "  </testsuite>\n");
  }

  fprintf(fp, "</testsuites>\n");
  fclose(fp);

  printf("\n  JUnit XML report written to: %s\n", zoweTestCtx.junitOutputPath);
}

/* ============================================================
 *  Memory Leak Detection
 *
 *  Tracks allocations made during each IT block. After the test's afterEach
 *  hook runs, _zoweTestLeakCheck() scans for unfreed allocations.
 *  
 *  This is intentionally simple: it uses a fixed-size array and linear
 *  search. It is designed for test code, not production hot paths.
 * ============================================================ */

void _zoweTestLeakReset(void) {
  ZoweTestLeakDetector *ld = &zoweTestCtx.leakDetector;
  ld->entryCount = 0;
  ld->leaksDetected = 0;
  ld->bytesLeaked = 0;
}

void _zoweTestLeakTrackAlloc(void *ptr, int size, const char *site) {
  ZoweTestLeakDetector *ld = &zoweTestCtx.leakDetector;

  if (!ld->enabled || !zoweTestCtx.inTest) {
    return;
  }
  if (ld->entryCount >= ZOWE_TEST_MAX_ALLOC_ENTRIES) {
    return; /* Silently stop tracking if we run out of slots */
  }

  ZoweTestAllocEntry *e = &ld->entries[ld->entryCount];
  e->ptr = ptr;
  e->size = size;
  e->site = site;
  e->freed = false;
  ld->entryCount++;
}

void _zoweTestLeakTrackFree(void *ptr, int size) {
  ZoweTestLeakDetector *ld = &zoweTestCtx.leakDetector;

  if (!ld->enabled || !zoweTestCtx.inTest) {
    return;
  }

  /* Find the allocation entry and mark it freed (reverse search for LIFO pattern) */
  for (int i = ld->entryCount - 1; i >= 0; i--) {
    if (ld->entries[i].ptr == ptr && !ld->entries[i].freed) {
      ld->entries[i].freed = true;
      return;
    }
  }
  /* ptr not found — it was allocated before this IT block or outside tracking */
}

int _zoweTestLeakCheck(void) {
  ZoweTestLeakDetector *ld = &zoweTestCtx.leakDetector;

  if (!ld->enabled) {
    return 0;
  }

  ld->leaksDetected = 0;
  ld->bytesLeaked = 0;

  for (int i = 0; i < ld->entryCount; i++) {
    if (!ld->entries[i].freed) {
      ld->leaksDetected++;
      ld->bytesLeaked += ld->entries[i].size;
    }
  }

  /* Print details of first few leaks for debugging */
  if (ld->leaksDetected > 0) {
    int shown = 0;
    for (int i = 0; i < ld->entryCount && shown < 5; i++) {
      if (!ld->entries[i].freed) {
        printf("        LEAK: %d bytes at %p (site: %s)\n",
            ld->entries[i].size, ld->entries[i].ptr,
            ld->entries[i].site ? ld->entries[i].site : "<unknown>");
        shown++;
      }
    }
    if (ld->leaksDetected > 5) {
      printf("        ... and %d more leak(s)\n", ld->leaksDetected - 5);
    }
  }

  return ld->leaksDetected;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
