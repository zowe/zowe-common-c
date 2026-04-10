


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  procinfo.c — USS process enumeration using w_getpsent().

  w_getpsent() is the z/OS USS equivalent of /proc traversal.
  It's declared in <sys/ps.h> and requires no special authority.
  Returns per-process: PID, PPID, UID, state, CPU time, command, etc.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/ps.h>
#include <pwd.h>

#include "procinfo.h"

/*
  Process state codes from w_getpsent().
  These are single characters defined as PS_* in <sys/ps.h>.
*/
const char *procInfoStateName(int status) {
  /* The state character is in the low byte of ps_state */
  int stateChar = status & 0xFF;
  switch (stateChar) {
  case 'R': return "RUN";
  case 'S': return "SLEEP";
  case 'W': return "WAIT";
  case 'X': return "FORK";
  case 'Z': return "ZOMBIE";
  case 'C': return "KWAIT";
  case 'F': return "FWAIT";
  case 'K': return "KWAIT";
  default:  return "?";
  }
}

/*
  Resolve UID to username.  Uses getpwuid; returns numeric string on failure.
*/
static void resolveUser(uid_t uid, char *buf, int bufLen) {
  struct passwd *pw = getpwuid(uid);
  if (pw && pw->pw_name) {
    strncpy(buf, pw->pw_name, bufLen - 1);
    buf[bufLen - 1] = '\0';
  } else {
    snprintf(buf, bufLen, "%d", (int)uid);
  }
}

int procInfoGetAll(ProcInfo **result) {
  *result = NULL;

  W_PSPROC buf;
  int token = 0;
  int count = 0;
  ProcInfo *head = NULL;
  ProcInfo *tail = NULL;

  int inputLength = sizeof(W_PSPROC);

  while (1) {
    memset(&buf, 0, sizeof(buf));

    int rc = w_getpsent(token, &buf, inputLength);
    if (rc < 0) break;
    token = rc;
    if (token == 0) break;

    ProcInfo *info = (ProcInfo *)malloc(sizeof(ProcInfo));
    if (!info) break;
    memset(info, 0, sizeof(ProcInfo));

    info->pid = buf.ps_pid;
    info->ppid = buf.ps_ppid;
    info->uid = buf.ps_ruid;
    info->status = buf.ps_state;

    /* Command name — use path if available, else cmdptr */
    if (buf.ps_pathlen > 0 && buf.ps_pathptr &&
        buf.ps_pathlen < (int)sizeof(info->command)) {
      memcpy(info->command, buf.ps_pathptr, buf.ps_pathlen);
      info->command[buf.ps_pathlen] = '\0';
    } else if (buf.ps_cmdlen > 0 && buf.ps_cmdptr &&
               buf.ps_cmdlen < (int)sizeof(info->command)) {
      memcpy(info->command, buf.ps_cmdptr, buf.ps_cmdlen);
      info->command[buf.ps_cmdlen] = '\0';
    }
    /* Extract just the basename for display */
    char *slash = strrchr(info->command, '/');
    if (slash) {
      memmove(info->command, slash + 1, strlen(slash + 1) + 1);
    }

    /* CPU time: ps_usertime + ps_systime (in clock ticks) */
    info->cpuTime = (uint64_t)buf.ps_usertime + (uint64_t)buf.ps_systime;

    /* Virtual memory size in pages -> KB (4K pages) */
    info->memSize = (uint32_t)(buf.ps_size * 4);

    /* Resolve username */
    resolveUser(info->uid, info->userName, sizeof(info->userName));

    if (!head) {
      head = tail = info;
    } else {
      tail->next = info;
      tail = info;
    }
    count++;
  }

  *result = head;
  return count;
}

int procInfoRefresh(ProcInfo **list, uint64_t elapsedMicros) {
  ProcInfo *oldList = *list;
  ProcInfo *newList = NULL;
  int count = procInfoGetAll(&newList);
  if (count < 0) return -1;

  /* Match by PID to compute CPU deltas */
  if (elapsedMicros > 0 && oldList) {
    for (ProcInfo *cur = newList; cur; cur = cur->next) {
      for (ProcInfo *old = oldList; old; old = old->next) {
        if (old->pid == cur->pid) {
          cur->cpuTimePrev = old->cpuTime;
          if (cur->cpuTime >= old->cpuTime) {
            uint64_t delta = cur->cpuTime - old->cpuTime;
            /* clock_t ticks — assume 100 ticks/sec (z/OS default) */
            double deltaMicros = (double)delta * 10000.0;
            cur->cpuPercent = (deltaMicros / (double)elapsedMicros) * 100.0;
          }
          break;
        }
      }
    }
  }

  procInfoFree(oldList);
  *list = newList;
  return count;
}

void procInfoFree(ProcInfo *list) {
  while (list) {
    ProcInfo *next = list->next;
    free(list);
    list = next;
  }
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
