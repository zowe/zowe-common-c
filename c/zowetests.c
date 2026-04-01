

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

#include "zowetests.h"

/* The single global test context instance */
ZoweTestContext zoweTestCtx;

void zoweTestInit(void) {
  memset(&zoweTestCtx, 0, sizeof(ZoweTestContext));
}

void _zoweTestDescribeBegin(const char *name) {
  strncpy(zoweTestCtx.suiteName, name, ZOWE_TEST_SUITE_NAME_MAX - 1);
  zoweTestCtx.suiteName[ZOWE_TEST_SUITE_NAME_MAX - 1] = '\0';

  zoweTestCtx.suitePassed = 0;
  zoweTestCtx.suiteFailed = 0;
  zoweTestCtx.suiteAssertionCount = 0;
  zoweTestCtx.beforeEach = NULL;
  zoweTestCtx.afterEach = NULL;

  printf("\n  %s\n", name);
}

void _zoweTestDescribeEnd(void) {
  printf("\n    %d passing, %d failing (%d assertion%s)\n",
      zoweTestCtx.suitePassed,
      zoweTestCtx.suiteFailed,
      zoweTestCtx.suiteAssertionCount,
      zoweTestCtx.suiteAssertionCount == 1 ? "" : "s");

  zoweTestCtx.totalPassed += zoweTestCtx.suitePassed;
  zoweTestCtx.totalFailed += zoweTestCtx.suiteFailed;
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

  if (zoweTestCtx.beforeEach != NULL) {
    zoweTestCtx.beforeEach();
  }
}

void _zoweTestItPassed(void) {
  zoweTestCtx.inTest = false;
  zoweTestCtx.suitePassed++;
  zoweTestCtx.suiteAssertionCount += zoweTestCtx.assertionCount;

  printf("    (/) %s (%d assertion%s)\n",
      zoweTestCtx.testName,
      zoweTestCtx.assertionCount,
      zoweTestCtx.assertionCount == 1 ? "" : "s");

  if (zoweTestCtx.afterEach != NULL) {
    zoweTestCtx.afterEach();
  }
}

void _zoweTestItFailed(void) {
  zoweTestCtx.inTest = false;
  zoweTestCtx.suiteFailed++;
  zoweTestCtx.suiteAssertionCount += zoweTestCtx.assertionCount;

  printf("    (X) %s\n", zoweTestCtx.testName);
  printf("        %s:%d\n", zoweTestCtx.failureFile, zoweTestCtx.failureLine);
  printf("        AssertionError: %s\n", zoweTestCtx.failureMessage);

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
  int total = zoweTestCtx.totalPassed + zoweTestCtx.totalFailed;
  int i;

  printf("\n");
  printf("  ======================================\n");
  printf("  Test Results\n");
  printf("  ======================================\n");
  printf("  Total:   %d\n", total);
  printf("  Passing: %d\n", zoweTestCtx.totalPassed);
  printf("  Failing: %d\n", zoweTestCtx.totalFailed);

  if (zoweTestCtx.coveredFunctionCount > 0) {
    printf("\n  Functions exercised by these tests (%d):\n",
        zoweTestCtx.coveredFunctionCount);
    for (i = 0; i < zoweTestCtx.coveredFunctionCount; i++) {
      printf("    - %s\n", zoweTestCtx.coveredFunctions[i]);
    }
  }

  printf("  ======================================\n");

  return (zoweTestCtx.totalFailed > 0) ? 1 : 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
