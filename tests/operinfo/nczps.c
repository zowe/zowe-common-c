

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  nczps — USS process viewer ("ps on steroids").
           ncurses-based version using nctui framework.

  Full-screen TUI displaying USS processes with z/OS enrichment (ASID).
  Auto-refreshes.  Filterable by user, command, etc.

  Usage:
    nczps [options]
      -interval <sec>   Refresh interval (default 5)
      -user <name>      Filter by username
      -all              Show all processes (including kernel)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "nctui.h"
#include "procinfo.h"

/* ----------------------------------------------------------------
   Data model
   ---------------------------------------------------------------- */

typedef struct PSData_tag {
  ProcInfo   *procList;
  ProcInfo  **procArray;
  int         procCount;
  int         showAll;
  char        filterUser[9];
  int         refreshSec;
  struct timeval lastRefresh;
} PSData;

/* ----------------------------------------------------------------
   Column definitions
   ---------------------------------------------------------------- */

enum {
  COL_PID = 0,
  COL_PPID,
  COL_USER,
  COL_CMD,
  COL_STATE,
  COL_CPU_PCT,
  COL_CPU_TIME,
  COL_MEM,
  COL_COUNT
};

static NcTuiColumn psColumns[] = {
  { "PID",     7,  NCTUI_ALIGN_RIGHT },
  { "PPID",    7,  NCTUI_ALIGN_RIGHT },
  { "USER",    8,  NCTUI_ALIGN_LEFT  },
  { "CMD",     16, NCTUI_ALIGN_LEFT  },
  { "State",   6,  NCTUI_ALIGN_LEFT  },
  { "CPU%",    6,  NCTUI_ALIGN_RIGHT },
  { "CPU-Time",10, NCTUI_ALIGN_RIGHT },
  { "Mem-K",   8,  NCTUI_ALIGN_RIGHT },
};

/* ----------------------------------------------------------------
   Forward declarations
   ---------------------------------------------------------------- */

static void psCellFormatter(int row, int col, char *buf, int bufLen,
                            void *userData);

/* ----------------------------------------------------------------
   Build flat array, applying filters
   ---------------------------------------------------------------- */

static NcTuiTable *gTui = NULL;

static void buildProcArray(PSData *data) {
  if (data->procArray) {
    free(data->procArray);
    data->procArray = NULL;
  }

  /* First pass: count matching entries (command-line filters) */
  int total = 0;
  for (ProcInfo *p = data->procList; p; p = p->next) {
    if (!data->showAll && p->pid <= 1) continue;
    if (data->filterUser[0] && strcmp(p->userName, data->filterUser) != 0) continue;
    total++;
  }

  if (total == 0) { data->procCount = 0; return; }

  /* Build unfiltered array */
  ProcInfo **tempArray = (ProcInfo **)malloc(total * sizeof(ProcInfo *));
  if (!tempArray) { data->procCount = 0; return; }

  int idx = 0;
  for (ProcInfo *p = data->procList; p; p = p->next) {
    if (!data->showAll && p->pid <= 1) continue;
    if (data->filterUser[0] && strcmp(p->userName, data->filterUser) != 0) continue;
    tempArray[idx++] = p;
  }

  /* Check for column filters */
  int anyFilter = 0;
  if (gTui) {
    for (int c = 0; c < gTui->numColumns; c++) {
      if (gTui->columns[c].filter[0]) { anyFilter = 1; break; }
    }
  }

  if (!anyFilter) {
    data->procArray = tempArray;
    data->procCount = total;
    return;
  }

  /* Apply column filters */
  data->procArray = tempArray;
  data->procCount = total;

  int matchCount = 0;
  char buf[NCTUI_MAX_COL_WIDTH + 1];
  for (int i = 0; i < total; i++) {
    int pass = 1;
    for (int c = 0; c < gTui->numColumns && pass; c++) {
      if (gTui->columns[c].filter[0] == '\0') continue;
      buf[0] = '\0';
      psCellFormatter(i, c, buf, sizeof(buf), data);
      if (!nctuiMatchFilter(gTui->columns[c].filter, buf)) pass = 0;
    }
    if (pass) matchCount++;
  }

  ProcInfo **filtered = (ProcInfo **)malloc(matchCount * sizeof(ProcInfo *));
  if (!filtered) return;

  idx = 0;
  for (int i = 0; i < total; i++) {
    int pass = 1;
    for (int c = 0; c < gTui->numColumns && pass; c++) {
      if (gTui->columns[c].filter[0] == '\0') continue;
      buf[0] = '\0';
      psCellFormatter(i, c, buf, sizeof(buf), data);
      if (!nctuiMatchFilter(gTui->columns[c].filter, buf)) pass = 0;
    }
    if (pass) filtered[idx++] = tempArray[i];
  }

  free(tempArray);
  data->procArray = filtered;
  data->procCount = matchCount;
}

/* ----------------------------------------------------------------
   Format CPU time as HH:MM:SS
   ---------------------------------------------------------------- */

static void formatCpuTime(uint64_t micros, char *buf, int bufLen) {
  uint64_t secs = micros / 1000000;
  int hours = (int)(secs / 3600);
  int mins  = (int)((secs % 3600) / 60);
  int sec   = (int)(secs % 60);
  if (hours > 0) {
    snprintf(buf, bufLen, "%d:%02d:%02d", hours, mins, sec);
  } else {
    snprintf(buf, bufLen, "%d:%02d", mins, sec);
  }
}

/* ----------------------------------------------------------------
   Cell formatter
   ---------------------------------------------------------------- */

static void psCellFormatter(int row, int col, char *buf, int bufLen,
                            void *userData) {
  PSData *data = (PSData *)userData;
  if (row < 0 || row >= data->procCount) { buf[0] = '\0'; return; }
  ProcInfo *proc = data->procArray[row];

  switch (col) {
  case COL_PID:
    snprintf(buf, bufLen, "%d", (int)proc->pid);
    break;
  case COL_PPID:
    snprintf(buf, bufLen, "%d", (int)proc->ppid);
    break;
  case COL_USER:
    snprintf(buf, bufLen, "%s", proc->userName);
    break;
  case COL_CMD:
    snprintf(buf, bufLen, "%s", proc->command);
    break;
  case COL_STATE:
    snprintf(buf, bufLen, "%s", procInfoStateName(proc->status));
    break;
  case COL_CPU_PCT:
    if (proc->cpuPercent > 0.005) {
      snprintf(buf, bufLen, "%.1f", proc->cpuPercent);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_CPU_TIME:
    formatCpuTime(proc->cpuTime, buf, bufLen);
    break;
  case COL_MEM:
    if (proc->memSize > 0) {
      snprintf(buf, bufLen, "%u", proc->memSize);
    } else {
      buf[0] = '\0';
    }
    break;
  default:
    buf[0] = '\0';
    break;
  }
}

/* ----------------------------------------------------------------
   Refresh handler
   ---------------------------------------------------------------- */

static int psRefreshHandler(void *userData) {
  PSData *data = (PSData *)userData;

  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t elapsedMicros = 0;
  if (data->lastRefresh.tv_sec > 0) {
    elapsedMicros = (uint64_t)(now.tv_sec - data->lastRefresh.tv_sec) * 1000000ULL
                  + (uint64_t)(now.tv_usec - data->lastRefresh.tv_usec);
  }
  data->lastRefresh = now;

  int total = procInfoRefresh(&data->procList, elapsedMicros);
  if (total < 0) total = 0;

  buildProcArray(data);
  return data->procCount;
}

/* ----------------------------------------------------------------
   Command handler
   ---------------------------------------------------------------- */

static int psCommandHandler(const char *command, void *userData) {
  PSData *data = (PSData *)userData;

  if (command[0] == 'q' || command[0] == 'Q') {
    return 1;
  }
  if (strcmp(command, "ALL") == 0 || strcmp(command, "all") == 0) {
    data->showAll = !data->showAll;
    buildProcArray(data);
    return 0;
  }
  /* USER <name> -- set user filter */
  if (strncmp(command, "USER ", 5) == 0 || strncmp(command, "user ", 5) == 0) {
    strncpy(data->filterUser, command + 5, 8);
    data->filterUser[8] = '\0';
    /* Trim trailing spaces */
    for (int i = 7; i >= 0 && data->filterUser[i] == ' '; i--) {
      data->filterUser[i] = '\0';
    }
    buildProcArray(data);
    return 0;
  }
  /* Clear filter */
  if (strcmp(command, "RESET") == 0 || strcmp(command, "reset") == 0) {
    data->filterUser[0] = '\0';
    buildProcArray(data);
    return 0;
  }
  return 0;
}

/* ----------------------------------------------------------------
   Filter handler: rebuild array when filters change
   ---------------------------------------------------------------- */

static void psFilterHandler(void *userData) {
  PSData *data = (PSData *)userData;
  buildProcArray(data);
  if (gTui) {
    gTui->rowCount = data->procCount;
    gTui->scrollRow = 0;
    gTui->selectedRow = -1;
  }
}

/* ----------------------------------------------------------------
   Sort handler: qsort the process array by selected column
   ---------------------------------------------------------------- */

static int psSortCompare(const void *a, const void *b) {
  if (!gTui) return 0;
  ProcInfo **pa = (ProcInfo **)a;
  ProcInfo **pb = (ProcInfo **)b;

  char bufA[NCTUI_MAX_COL_WIDTH + 1] = "";
  char bufB[NCTUI_MAX_COL_WIDTH + 1] = "";

  int col = gTui->sortColumn;
  switch (col) {
  case COL_PID:
    snprintf(bufA, sizeof(bufA), "%08d", (int)(*pa)->pid);
    snprintf(bufB, sizeof(bufB), "%08d", (int)(*pb)->pid);
    break;
  case COL_PPID:
    snprintf(bufA, sizeof(bufA), "%08d", (int)(*pa)->ppid);
    snprintf(bufB, sizeof(bufB), "%08d", (int)(*pb)->ppid);
    break;
  case COL_USER:
    strncpy(bufA, (*pa)->userName, sizeof(bufA)-1);
    strncpy(bufB, (*pb)->userName, sizeof(bufB)-1);
    break;
  case COL_CMD:
    strncpy(bufA, (*pa)->command, sizeof(bufA)-1);
    strncpy(bufB, (*pb)->command, sizeof(bufB)-1);
    break;
  case COL_STATE:
    strncpy(bufA, procInfoStateName((*pa)->status), sizeof(bufA)-1);
    strncpy(bufB, procInfoStateName((*pb)->status), sizeof(bufB)-1);
    break;
  case COL_CPU_PCT:
    snprintf(bufA, sizeof(bufA), "%012.4f", (*pa)->cpuPercent);
    snprintf(bufB, sizeof(bufB), "%012.4f", (*pb)->cpuPercent);
    break;
  case COL_CPU_TIME:
    snprintf(bufA, sizeof(bufA), "%016llu", (unsigned long long)(*pa)->cpuTime);
    snprintf(bufB, sizeof(bufB), "%016llu", (unsigned long long)(*pb)->cpuTime);
    break;
  case COL_MEM:
    snprintf(bufA, sizeof(bufA), "%08u", (*pa)->memSize);
    snprintf(bufB, sizeof(bufB), "%08u", (*pb)->memSize);
    break;
  default:
    snprintf(bufA, sizeof(bufA), "%08d", (int)(*pa)->pid);
    snprintf(bufB, sizeof(bufB), "%08d", (int)(*pb)->pid);
    break;
  }

  int cmp = strcmp(bufA, bufB);
  return gTui->sortAscending ? cmp : -cmp;
}

static void psSortHandler(int col, int ascending, void *userData) {
  PSData *data = (PSData *)userData;
  if (data->procCount > 1 && data->procArray) {
    qsort(data->procArray, data->procCount, sizeof(ProcInfo *), psSortCompare);
  }
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  PSData data;
  memset(&data, 0, sizeof(data));
  data.refreshSec = 5;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-interval") && i + 1 < argc) {
      data.refreshSec = atoi(argv[++i]);
      if (data.refreshSec < 1) data.refreshSec = 1;
    } else if (!strcmp(argv[i], "-user") && i + 1 < argc) {
      strncpy(data.filterUser, argv[++i], 8);
      data.filterUser[8] = '\0';
    } else if (!strcmp(argv[i], "-all")) {
      data.showAll = 1;
    } else if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "-h")) {
      printf("Usage: nczps [options]\n");
      printf("  -interval <sec>   Refresh interval (default 5)\n");
      printf("  -user <name>      Filter by username\n");
      printf("  -all              Show all processes\n");
      printf("  -help             Show this help\n");
      return 0;
    }
  }

  /* Initial scan */
  gettimeofday(&data.lastRefresh, NULL);
  int total = procInfoGetAll(&data.procList);
  if (total < 0) {
    fprintf(stderr, "procInfoGetAll failed\n");
    return 1;
  }
  buildProcArray(&data);

  /* Set up navigator */
  NcTuiNav nav;
  nctuiNavInit(&nav);

  NcTuiPage psPage;
  nctuiPageInitTable(&psPage, "PS");
  nctuiSetColumns(&psPage.table, psColumns, COL_COUNT, 3);
  nctuiSetTitle(&psPage.table, "ZPS PROCESS VIEWER");
  psPage.table.rowCount = data.procCount;
  psPage.table.cellFormatter = psCellFormatter;
  psPage.table.commandHandler = psCommandHandler;
  psPage.table.refreshHandler = psRefreshHandler;
  psPage.table.filterHandler = psFilterHandler;
  psPage.table.sortHandler = psSortHandler;
  psPage.table.userData = &data;

  nctuiNavPush(&nav, &psPage);
  gTui = &nav.stack[0].table;

  nctuiNavRun(&nav);
  nctuiNavTerm(&nav);

  /* Cleanup */
  procInfoFree(data.procList);
  if (data.procArray) free(data.procArray);

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
