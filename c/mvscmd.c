

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  mvscmd.c — Issue MVS system commands from USS and capture response.

  Uses:  tsocmd "CONSOLE SYSCMD(cmd) SOLDISPLAY(200)"

  The CONSOLE command sends the system command and SOLDISPLAY controls
  how many lines of solicited response to wait for and display.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mvscmd.h"

int mvsCmdIssue(const char *cmd, MvsCmdResult *result) {
  memset(result, 0, sizeof(MvsCmdResult));

  /* Build the tsocmd string.
     Single quotes inside the CONSOLE command need care — we use
     double-quote wrapping for the tsocmd argument. */
  char shellCmd[1024];
  int len = snprintf(shellCmd, sizeof(shellCmd),
                     "tsocmd \"CONSOLE SYSCMD(%s) SOLDISPLAY(200)\" 2>/dev/null",
                     cmd);
  if (len < 0 || len >= (int)sizeof(shellCmd)) {
    result->rc = -1;
    return -1;
  }

  FILE *fp = popen(shellCmd, "r");
  if (!fp) {
    result->rc = -1;
    return -1;
  }

  char buf[MVSCMD_MAX_LINE];
  while (fgets(buf, sizeof(buf), fp)) {
    /* Strip trailing newline */
    int slen = strlen(buf);
    while (slen > 0 && (buf[slen-1] == '\n' || buf[slen-1] == '\r')) {
      buf[--slen] = '\0';
    }

    /* Skip blank lines and the tsocmd echo line */
    if (slen == 0) continue;

    if (result->lineCount < MVSCMD_MAX_LINES) {
      result->lines[result->lineCount] = strdup(buf);
      result->lineCount++;
    }
  }

  int status = pclose(fp);
  result->rc = (status == -1) ? -1 : (status >> 8) & 0xFF;

  return result->rc;
}

void mvsCmdFree(MvsCmdResult *result) {
  for (int i = 0; i < result->lineCount; i++) {
    free(result->lines[i]);
    result->lines[i] = NULL;
  }
  result->lineCount = 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
