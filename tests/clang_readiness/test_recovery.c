/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/*
 * test_recovery.c -- clang-readiness smoke test for the recovery router
 * and the DSECT SUITE inline-asm pattern used in c/recovery.c.
 *
 * Under xlclang, every inline `__asm` block in a translation unit shared
 * a single HLASM stream, so a one-time gen_dsects_*() emission of
 * CVT/PSA/SDWA/etc. DSECTs was visible to every other __asm block. Under
 * Open XL / ibm-clang64 each __asm is its own sub-program -- so we built
 * RCV_DSECT_SUITE / ZOS_DSECT_SUITE string-literal macros that re-emit
 * the DSECT dump at the head of every __asm that needs it (see
 * project_clang_readiness_helpers.md for the rationale).
 *
 * Recovery requires a CAA->rleTask before recoveryEstablishRouter will
 * succeed, even on the happy path -- existing tests/recoverytest.c uses
 * the INIT_ZOS_ENV_IF_NEEDED idiom. We replicate it inline here so this
 * test stays self-contained.
 */

#include <stdio.h>

#include "zowetypes.h"
#include "alloc.h"
#include "le.h"
#include "recovery.h"

#define TRACE(...) do { printf("[T:%4d] ", __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

int main(int argc, char **argv) {
  TRACE("entered main");

  TRACE("about to call getCAA()");
  CAA *caa = (CAA *)getCAA();
  TRACE("getCAA returned caa=%p", (void *)caa);

  if (caa == NULL) {
    TRACE("FAIL: caa is NULL");
    return 1;
  }

  TRACE("caa->rleTask before assignment = %p", (void *)caa->rleTask);

  TRACE("about to safeMalloc31 RLETask");
  void *task = safeMalloc31(sizeof(RLETask), "dummy RLE task");
  TRACE("safeMalloc31 returned task=%p", task);

  if (task == NULL) {
    TRACE("FAIL: safeMalloc31 returned NULL");
    return 1;
  }

  TRACE("about to assign caa->rleTask = task");
  caa->rleTask = (RLETask *)task;
  TRACE("assigned caa->rleTask = %p", (void *)caa->rleTask);

  TRACE("about to call recoveryEstablishRouter(0)");
  int rc = recoveryEstablishRouter(0);
  TRACE("recoveryEstablishRouter returned rc=%d", rc);

  if (rc != 0) {
    TRACE("FAIL: recoveryEstablishRouter rc=%d", rc);
    return 1;
  }

  TRACE("about to call recoveryPush");
  rc = recoveryPush("clang_readiness",
                    0,                          /* flags */
                    "clang readiness test",     /* dumpTitle */
                    NULL, NULL,                 /* analysis fn + data */
                    NULL, NULL);                /* cleanup  fn + data */
  TRACE("recoveryPush returned rc=%d", rc);

  if (rc != 0) {
    TRACE("FAIL: recoveryPush rc=%d", rc);
    return 1;
  }

  TRACE("about to do trivial arithmetic");
  volatile int x = 42;
  TRACE("x = %d", x);
  volatile int y = x * 2;
  TRACE("y = x * 2 = %d", y);
  if (y != 84) {
    TRACE("FAIL: arithmetic check (got %d, expected 84)", y);
    return 1;
  }
  TRACE("arithmetic OK");

  TRACE("about to call recoveryPop");
  recoveryPop();
  TRACE("recoveryPop returned");

  TRACE("PASS: recovery router established, pushed, popped (DSECT SUITE pattern OK)");
  printf("PASS: recovery router established, pushed, popped "
         "(DSECT SUITE pattern OK)\n");
  fflush(stdout);
  return 0;
}
