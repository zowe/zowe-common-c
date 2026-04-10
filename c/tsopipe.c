

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  tsopipe — execute TSO commands via popen("tsocmd ...").

  Each call forks a tsocmd subprocess.  Not fast, but authorized
  and unrestricted — every command in the book works.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "tsopipe.h"

#define MAX_LINE_LEN    1024
#define INITIAL_CAPACITY  64

int tsoPipeExec(const char *command, TsoPipeResult *result) {
  char popenCmd[TSOPIPE_MAX_CMD + 32];
  char lineBuf[MAX_LINE_LEN];

  memset(result, 0, sizeof(*result));

  snprintf(popenCmd, sizeof(popenCmd), "tsocmd \"%s\" 2>&1", command);

  FILE *p = popen(popenCmd, "r");
  if (!p) {
    result->rc = -1;
    return -1;
  }

  int capacity = INITIAL_CAPACITY;
  result->lines = (char **)malloc(capacity * sizeof(char *));
  if (!result->lines) {
    pclose(p);
    result->rc = -1;
    return -1;
  }

  while (fgets(lineBuf, sizeof(lineBuf), p)) {
    int len = strlen(lineBuf);
    while (len > 0 && (lineBuf[len - 1] == '\n' || lineBuf[len - 1] == '\r'))
      lineBuf[--len] = '\0';

    if (result->lineCount >= capacity) {
      capacity *= 2;
      char **newLines = (char **)realloc(result->lines, capacity * sizeof(char *));
      if (!newLines) {
        pclose(p);
        return -1;
      }
      result->lines = newLines;
    }

    result->lines[result->lineCount] = strdup(lineBuf);
    result->lineCount++;

    if (result->lineCount >= TSOPIPE_MAX_LINES) {
      result->truncated = 1;
      break;
    }
  }

  int status = pclose(p);
  result->rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  return 0;
}

void tsoPipeFreeResult(TsoPipeResult *result) {
  if (result->lines) {
    for (int i = 0; i < result->lineCount; i++) {
      if (result->lines[i]) free(result->lines[i]);
    }
    free(result->lines);
    result->lines = NULL;
  }
  result->lineCount = 0;
  result->rc = 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
