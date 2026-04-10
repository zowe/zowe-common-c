

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __MVSCMD__
#define __MVSCMD__ 1

/*
  MVS command execution via __console2().
  No APF required — uses LE's EMCS console wrapper.
  The caller can issue system commands (D A, D U, etc.)
  and receive async responses.
*/

/*
  Response message — one per WTO line returned.
*/
typedef struct MvsCmdResponse_tag {
  char     text[256];       /* message text, null-terminated */
  int      routeCode;       /* route code */
  struct MvsCmdResponse_tag *next;
} MvsCmdResponse;

/*
  Opaque console handle.
*/
typedef struct MvsConsole_tag MvsConsole;

/*
  Open an EMCS console.
  consoleName should be 2-8 chars (e.g. "ZCMD01").
  If NULL, a name is generated from the PID.
  Returns 0 on success.
*/
int mvsCmdOpen(MvsConsole **console, const char *consoleName);

/*
  Issue a command and collect responses.
  Waits up to timeoutMs milliseconds for responses.
  Returns 0 on success. Responses are allocated with malloc;
  free with mvsCmdFreeResponses.
*/
int mvsCmdIssue(MvsConsole *console, const char *command,
                int timeoutMs,
                MvsCmdResponse **responses, int *responseCount);

/*
  Free a response chain.
*/
void mvsCmdFreeResponses(MvsCmdResponse *responses);

/*
  Close the console.
*/
void mvsCmdClose(MvsConsole *console);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
