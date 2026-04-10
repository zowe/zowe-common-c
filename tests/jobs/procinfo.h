

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __PROCINFO__
#define __PROCINFO__ 1

#include <stdint.h>
#include <sys/types.h>

/*
  USS process info — one per process.
  Populated by w_getpsent() from <sys/ps.h>.
*/
typedef struct ProcInfo_tag {
  pid_t      pid;
  pid_t      ppid;
  uid_t      uid;
  char       userName[9];     /* resolved username, null-terminated */
  char       command[256];    /* command/program name */
  char       args[256];       /* command arguments (if available) */
  int        status;          /* process state (ps_state flags + char) */
  uint64_t   cpuTime;         /* CPU time in clock ticks */
  uint64_t   cpuTimePrev;     /* previous for delta calc */
  double     cpuPercent;      /* CPU% since last sample */
  uint32_t   memSize;         /* virtual memory size in KB */
  struct ProcInfo_tag *next;
} ProcInfo;

/*
  Enumerate all USS processes.
  Returns a linked list of ProcInfo.
  Caller must free with procInfoFree().
  Returns count, or -1 on error.
*/
int procInfoGetAll(ProcInfo **result);

/*
  Refresh: re-enumerate and compute CPU%.
  Frees old list and returns new one.
  elapsedMicros is time since last call.
  Returns count, or -1 on error.
*/
int procInfoRefresh(ProcInfo **list, uint64_t elapsedMicros);

/*
  Free a chain of ProcInfo.
*/
void procInfoFree(ProcInfo *list);

/*
  Process state names.
*/
const char *procInfoStateName(int status);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
