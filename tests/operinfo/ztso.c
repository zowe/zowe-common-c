

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  ztso  Test program for the TSO service library.

  Establishes a background TSO/E environment and runs
  one or more TSO commands, printing the captured output.

  Usage:
    ztso                          Run default test commands
    ztso "LISTDS 'SYS1.PARMLIB'" Run a specific TSO command
    ztso "STATUS" "TIME"          Run multiple commands
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "tsoservice.h"

static void runCommand(TsoSession *session, const char *cmd) {
  TsoResult *result = NULL;

  printf("--- TSO: %s ---\n", cmd);
  int rc = tsoServiceExec(session, cmd, &result);

  if (result) {
    printf("  svcRC=%d funcRC=%d rsn=%d abend=%d\n",
           result->serviceRC, result->functionRC,
           result->reasonCode, result->abendCode);

    for (int i = 0; i < result->lineCount; i++) {
      printf("  %s\n", result->lines[i]);
    }

    if (result->lineCount == 0) {
      printf("  (no output captured)\n");
    }

    tsoServiceFreeResult(result);
  } else {
    printf("  rc=%d, no result\n", rc);
  }
  printf("\n");
}

int main(int argc, char *argv[]) {
  TsoSession *session = NULL;

  printf("ztso: initializing TSO/E background environment...\n");
  int rc = tsoServiceInit(&session);
  if (rc != RC_TSO_OK) {
    fprintf(stderr, "ztso: tsoServiceInit failed rc=%d\n", rc);
    return 1;
  }
  printf("ztso: TSO/E environment established.\n\n");

  if (argc > 1) {
    /* Run commands from argv */
    for (int i = 1; i < argc; i++) {
      runCommand(session, argv[i]);
    }
  } else {
    /* Default test sequence */
    runCommand(session, "TIME");
    runCommand(session, "STATUS");
    runCommand(session, "LISTDS 'SYS1.PARMLIB'");
    runCommand(session, "LISTDS 'SYS1.PROCLIB'");
  }

  printf("ztso: terminating TSO/E environment...\n");
  rc = tsoServiceTerm(session);
  if (rc != RC_TSO_OK) {
    fprintf(stderr, "ztso: tsoServiceTerm failed rc=%d\n", rc);
    return 1;
  }
  printf("ztso: done.\n");

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
