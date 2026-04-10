

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __MVSCMD_H__
#define __MVSCMD_H__

/*
  mvscmd — Issue MVS system commands from USS via tsocmd CONSOLE
           and capture the response lines.

  Usage:
    MvsCmdResult result;
    int rc = mvsCmdIssue("D SLIP,ID=ALL", &result);
    if (rc == 0) {
      for (int i = 0; i < result.lineCount; i++) {
        printf("%s\n", result.lines[i]);
      }
    }
    mvsCmdFree(&result);

  Notes:
    - Requires CONSOLE command authority (OPERPARM or CONSOLE profile)
    - Uses tsocmd "CONSOLE SYSCMD(...) SOLDISPLAY(200)"
    - Response lines are heap-allocated; caller must call mvsCmdFree()
*/

#define MVSCMD_MAX_LINES  500
#define MVSCMD_MAX_LINE   256

typedef struct MvsCmdResult_tag {
  int   rc;                              /* 0=ok, -1=popen fail, >0=tsocmd rc */
  int   lineCount;
  char *lines[MVSCMD_MAX_LINES];
} MvsCmdResult;

/*
  Issue an MVS system command (e.g. "D SLIP,ID=ALL") and capture the
  console response into result.  Returns 0 on success.
*/
int mvsCmdIssue(const char *cmd, MvsCmdResult *result);

/*
  Free all heap storage in a result.
*/
void mvsCmdFree(MvsCmdResult *result);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
