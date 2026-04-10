

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __TSOPIPE_H__
#define __TSOPIPE_H__

/*
  tsopipe — execute TSO commands via tsocmd and capture output.

  Simple popen-based approach: each call to tsoPipeExec() runs
  tsocmd as a subprocess, collects stdout/stderr, and returns
  the output lines and exit code.
*/

#define TSOPIPE_MAX_CMD   1024
#define TSOPIPE_MAX_LINES 5000

typedef struct TsoPipeResult_tag {
  char **lines;       /* array of output lines (caller owns after exec) */
  int    lineCount;
  int    rc;          /* tsocmd exit code */
  int    truncated;   /* 1 if output was capped at TSOPIPE_MAX_LINES */
} TsoPipeResult;

/* Execute a TSO command.  Returns 0 on success (command ran, check result->rc
   for the TSO return code), -1 on popen/system failure. */
int  tsoPipeExec(const char *command, TsoPipeResult *result);

/* Free the lines array inside a result.  Safe to call multiple times. */
void tsoPipeFreeResult(TsoPipeResult *result);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
