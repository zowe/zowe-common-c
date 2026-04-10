

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZDISPLAY__
#define __ZDISPLAY__ 1

/*
  zdisplay — MVS DISPLAY command parser and executor.

  Parses "D xxx" command strings using standard MVS syntax,
  reads the underlying control blocks / services,
  and returns results as formatted text lines matching
  the approximate MVS console output format.

  This allows users and AI systems to leverage existing
  z/OS documentation and command knowledge.
*/

/* ----------------------------------------------------------------
   Command catalog entry — describes one D command variant
   ---------------------------------------------------------------- */

typedef struct ZDispCommand_tag {
  const char  *syntax;       /* e.g. "D IPLINFO" */
  const char  *description;  /* brief explanation */
  const char  *argHelp;      /* argument guidance, NULL if none */
  int          implemented;  /* 1 if we can execute it */
} ZDispCommand;

/* ----------------------------------------------------------------
   Result — formatted text lines
   ---------------------------------------------------------------- */

#define ZDISP_MAX_LINES 4096
#define ZDISP_MAX_LINE  256

typedef struct ZDispResult_tag {
  char   **lines;       /* array of null-terminated strings */
  int      lineCount;
  int      maxLines;
  int      rc;          /* 0=ok, non-zero=error */
  char     message[256];/* error or status message */
} ZDispResult;

/* ----------------------------------------------------------------
   API
   ---------------------------------------------------------------- */

/*
  Get the command catalog.
  Returns array of ZDispCommand entries; *count is set to the number.
  The returned pointer is to static data — do not free.
*/
const ZDispCommand *zdispGetCatalog(int *count);

/*
  Execute a display command.
  cmdText is the full command string, e.g. "D IPLINFO" or "D A,L".
  Case-insensitive.  Leading/trailing spaces are trimmed.
  Returns a ZDispResult with formatted text lines.
  Caller must free with zdispFreeResult().
*/
ZDispResult *zdispExecute(const char *cmdText);

/*
  Free a result.
*/
void zdispFreeResult(ZDispResult *result);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
