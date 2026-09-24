/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/*
 * test_dynalloc.c -- clang-readiness smoke test for ddnameExists() +
 * getDSAB() in c/zos.c. Both call GETDSAB in inline asm. The DSECT-SUITE-
 * after fix moved ZOS_DSECT_SUITE from before to after the GETDSAB
 * expansion in each of those two functions; this test exercises both
 * paths through a fresh dynamic allocation (SVC 99 via dynalloc.c) so
 * the GETDSAB call sees a DD it didn't see at process start.
 *
 * Cycle:
 *   1. dynallocUSSDirectory(ddname, /tmp, err)  -- create the DD
 *   2. ddnameExists(ddname)                     -- expect rc=0 (GETDSAB found it)
 *   3. getDSAB(ddname)                           -- expect non-NULL
 *   4. hex-dump first 32 bytes of DSAB to confirm shape
 *   5. DeallocDDName(ddname)                    -- destroy it
 *   6. ddnameExists(ddname)                     -- expect non-zero (GETDSAB not-found)
 *
 * Note on semantics: despite the name, ddnameExists() returns the raw
 * GETDSAB return code: 0 means the DD WAS FOUND, non-zero means it
 * wasn't. This test checks both states accordingly.
 *
 * Each step prints. If any step fails, the test exits non-zero with the
 * failure point named.
 */

#include <stdio.h>
#include <string.h>

#include "zowetypes.h"
#include "zos.h"
#include "dynalloc.h"

#define TEST_DDNAME  "ZCLANGRD"   /* 8 chars; uppercase-ASCII */
#define TEST_USSPATH "/tmp"

int main(int argc, char **argv) {
  char errBuf[256];
  memset(errBuf, 0, sizeof(errBuf));

  printf("=== step 1: dynallocUSSDirectory('%s', '%s', errBuf) ===\n",
         TEST_DDNAME, TEST_USSPATH);
  fflush(stdout);
  int rc = dynallocUSSDirectory(TEST_DDNAME, TEST_USSPATH, errBuf);
  printf("  rc = %d\n", rc);
  if (errBuf[0] != '\0') {
    printf("  err: '%s'\n", errBuf);
  }
  fflush(stdout);
  if (rc != 0) {
    printf("FAIL: dynallocUSSDirectory rc=%d\n", rc);
    return 1;
  }

  printf("=== step 2: ddnameExists('%s') -- expect rc=0 (DD found) ===\n",
         TEST_DDNAME);
  fflush(stdout);
  int existsRC = ddnameExists(TEST_DDNAME);
  printf("  ddnameExists = %d\n", existsRC);
  fflush(stdout);
  if (existsRC != 0) {
    printf("FAIL: ddnameExists rc=%d for an allocated DD\n", existsRC);
    DeallocDDName(TEST_DDNAME);
    return 2;
  }

  printf("=== step 3: getDSAB('%s') -- expect non-NULL ===\n", TEST_DDNAME);
  fflush(stdout);
  DSAB *dsab = getDSAB(TEST_DDNAME);
  printf("  getDSAB = %p\n", (void *)dsab);
  fflush(stdout);
  if (dsab == NULL) {
    printf("FAIL: getDSAB returned NULL for an allocated DD\n");
    DeallocDDName(TEST_DDNAME);
    return 3;
  }

  printf("=== step 4: first 32 bytes of DSAB at %p ===\n", (void *)dsab);
  {
    const unsigned char *p = (const unsigned char *)dsab;
    for (int row = 0; row < 2; row++) {
      printf("  %04x:", row * 16);
      for (int col = 0; col < 16; col++) {
        printf(" %02x", p[row * 16 + col]);
      }
      printf("\n");
    }
  }
  fflush(stdout);

  printf("=== step 5: DeallocDDName('%s') ===\n", TEST_DDNAME);
  fflush(stdout);
  int drc = DeallocDDName(TEST_DDNAME);
  printf("  DeallocDDName rc = %d\n", drc);
  fflush(stdout);
  if (drc != 0) {
    printf("FAIL: DeallocDDName rc=%d\n", drc);
    return 4;
  }

  printf("=== step 6: ddnameExists('%s') after dealloc -- expect rc!=0 (DD gone) ===\n",
         TEST_DDNAME);
  fflush(stdout);
  int existsAfter = ddnameExists(TEST_DDNAME);
  printf("  ddnameExists = %d\n", existsAfter);
  fflush(stdout);
  if (existsAfter == 0) {
    printf("FAIL: ddnameExists rc=0 after dealloc (DD still seen)\n");
    return 5;
  }

  printf("PASS: dynalloc + ddnameExists + getDSAB cycle complete\n");
  return 0;
}
