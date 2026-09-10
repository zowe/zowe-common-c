/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/* test_le_options.c -- getEDB / getOCB / getLEHeapOptions against what
 * Language Environment actually does.
 *
 * Usage:  test_le_options EXPECT_POOLS EXPECT_POOLS64 EXPECT_ZONES64
 *   EXPECT_POOLS    0|1   HEAPPOOLS expected ON
 *   EXPECT_POOLS64  0|1   HEAPPOOLS64 expected ON
 *   EXPECT_ZONES64  n     HEAPZONES size expected for the 64-bit heap
 *
 * The driver (run-zos.sh) runs this under several _CEE_RUNOPTS settings and
 * passes what each one should produce. Every reading from the OCB is also
 * cross-checked against live heap behaviour: on a 64-bit LE heap the word
 * just before a malloc'd block is the element length (0x20 for a 1-byte
 * request), under HEAPPOOLS64 it is a small pool index instead, and under
 * HEAPZONES it grows by the zone size. That cross-check keeps the test
 * honest if IBM ever moves a field: the two sources have to agree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "le.h"

static int failures = 0;

static void check(int ok, const char *what) {
  printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    failures++;
  }
}

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: %s EXPECT_POOLS EXPECT_POOLS64 EXPECT_ZONES64\n", argv[0]);
    return 2;
  }
  int expectPools   = atoi(argv[1]);
  int expectPools64 = atoi(argv[2]);
  unsigned int expectZones64 = (unsigned int)atoi(argv[3]);

  const char *runopts = getenv("_CEE_RUNOPTS");
  printf("_CEE_RUNOPTS=%s\n", runopts ? runopts : "(unset)");

  char *caa = getCAA();
  char *edb = getEDB();
  char *ocb = getOCB();
  printf("  CAA %p  EDB %p  OCB %p\n", (void *)caa, (void *)edb, (void *)ocb);
  check(edb != NULL, "getEDB found the enclave data block (eyecatcher CEEEDB)");
  check(ocb != NULL, "getOCB found the options control block (eyecatcher CEEOCB/CELQOCB)");

  LEHeapOptions options;
  int rc = getLEHeapOptions(&options);
  check(rc == 0, "getLEHeapOptions returned 0");
  printf("  HEAPPOOLS   %s  (where-set 0x%X)\n", options.heapPools ? "ON" : "off",
         options.heapPoolsWhereSet);
  printf("  HEAPPOOLS64 %s  (where-set 0x%X)\n", options.heapPools64 ? "ON" : "off",
         options.heapPools64WhereSet);
  printf("  HEAPZONES   size31 %u  size64 %u  (where-set 0x%X)\n",
         options.heapZonesSize31, options.heapZonesSize64, options.heapZonesWhereSet);

  check(options.heapPools   == (expectPools   != 0), "HEAPPOOLS matches the expectation");
  check(options.heapPools64 == (expectPools64 != 0), "HEAPPOOLS64 matches the expectation");
  check(options.heapZonesSize64 == expectZones64,    "HEAPZONES size64 matches the expectation");

#ifdef _LP64
  /* Cross-check against the heap itself. The 64-bit LE heap element header
     ends with the element length; a 1-byte request is a 0x20-byte element.
     Under HEAPPOOLS64 that word is the pool index (small). Under HEAPZONES
     the element grows by the zone. */
  char *block = malloc(1);
  unsigned int header = *(unsigned int *)(block - 4);
  printf("  malloc(1) element header word 0x%X\n", header);
  if (options.heapPools64) {
    check(header < 0x20, "heap agrees: pooled element carries a pool index, not a length");
  } else if (options.heapZonesSize64 > 0) {
    check(header > 0x20, "heap agrees: element is larger than 0x20, a zone is appended");
  } else {
    check(header == 0x20, "heap agrees: stock 64-bit element of 0x20 bytes");
  }
  free(block);
#else
  printf("  (31-bit build: heap-header cross-check applies to the 64-bit heap only)\n");
#endif

  printf("%d failure%s\n", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
