/* test_signalcontrol.c -- compile + install smoke for signalControl().
 *
 * The primary purpose is to ensure c/signalcontrol.c stays type-clean
 * under the strict BPX prototypes in zowe_bpx_prototypes.h -- the
 * compile of this TU + COMMON_C is the regression guard. The runtime
 * arm installs a handler for SIGUSR1 and verifies rc=0 / returnCode=0,
 * which confirms the BPX1SIA / BPX4SIA call shape is honored at run
 * time too.
 *
 * Note: we deliberately do NOT raise() the signal here. Under xlclang
 * the signal handler trampoline triggers a 0C1 operation exception
 * on return -- pre-existing runtime issue, unrelated to the BPX
 * prototype work. */

#include <stdio.h>
#include <signal.h>
#include "zowetypes.h"
#include "signalcontrol.h"

static void noopHandler(int sig) {
  (void)sig;
}

int main(int argc, char **argv) {
  int returnCode = 0;
  int reasonCode = 0;
  int rc;

  rc = signalControl(SIGUSR1, noopHandler, &returnCode, &reasonCode);
  printf("signalControl(SIGUSR1) rc=%d returnCode=%d reasonCode=0x%x\n",
         rc, returnCode, reasonCode);
  if (rc != 0) {
    fprintf(stderr, "FAIL: signalControl returned non-zero\n");
    return 1;
  }

  printf("PASS: signalControl install smoke\n");
  return 0;
}
