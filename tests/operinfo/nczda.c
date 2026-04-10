

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  nczda — SDSF DA (Display Active) panel equivalent.
           ncurses-based version using nctui framework.

  Full-screen TUI displaying active address spaces with performance data.
  Walks CVT -> ASVT -> ASCB chain to enumerate address spaces.
  Auto-refreshes to compute CPU%, I/O rate, etc.

  Usage:
    nczda [options]
      -interval <sec>   Refresh interval in seconds (default 5)
      -all              Show all address spaces (including idle system)
      -stc              Show only started tasks
      -tso              Show only TSO users
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "zowetypes.h"
#include "zos.h"
#include "nctui.h"
#include "asinfo.h"

/* ----------------------------------------------------------------
   Data model
   ---------------------------------------------------------------- */

typedef struct DAData_tag {
  ASInfo   *asList;        /* linked list from asInfoGetAll */
  ASInfo  **asArray;       /* flat array for indexed access */
  int       asCount;
  int       showAll;       /* include idle system addr spaces */
  int       showSTC;       /* filter: STCs only */
  int       showTSO;       /* filter: TSO only */
  int       refreshSec;    /* auto-refresh interval */
  struct timeval lastRefresh;
} DAData;

/* ----------------------------------------------------------------
   Column definitions
   ---------------------------------------------------------------- */

enum {
  COL_JOBNAME = 0,
  COL_ASID,
  COL_OWNER,
  COL_TYPE,
  COL_DP,
  COL_REAL,
  COL_CPU_PCT,
  COL_EXCP,
  COL_IO_RATE,
  COL_STATUS,
  COL_COUNT
};

static NcTuiColumn daColumns[] = {
  { "JobName",  8,  NCTUI_ALIGN_LEFT  },
  { "ASID",     5,  NCTUI_ALIGN_RIGHT },
  { "Owner",    8,  NCTUI_ALIGN_LEFT  },
  { "Type",     4,  NCTUI_ALIGN_LEFT  },
  { "DP",       4,  NCTUI_ALIGN_RIGHT },
  { "Real-K",   8,  NCTUI_ALIGN_RIGHT },
  { "CPU%",     6,  NCTUI_ALIGN_RIGHT },
  { "EXCP",     6,  NCTUI_ALIGN_RIGHT },
  { "IO/s",     7,  NCTUI_ALIGN_RIGHT },
  { "Status",   12, NCTUI_ALIGN_LEFT  },
};

/* ----------------------------------------------------------------
   Dispatch flag decoding
   ---------------------------------------------------------------- */

static const char *dspStatus(uint8_t dsp1) {
  /* ascbdsp1 bit flags */
  if (dsp1 & 0x80) return "DISPATCHED";
  if (dsp1 & 0x40) return "IN REAL";
  if (dsp1 & 0x20) return "SWAPPED";
  if (dsp1 & 0x02) return "LOGSWAP";
  if (dsp1 & 0x01) return "TERM";
  return "";
}

/* ----------------------------------------------------------------
   Type string
   ---------------------------------------------------------------- */

static const char *asType(ASInfo *info) {
  if (info->isTSO) return "TSO";
  if (info->isSTC) return "STC";
  if (info->isSystem) return "SYS";
  return "JOB";
}

/* ----------------------------------------------------------------
   Forward declarations
   ---------------------------------------------------------------- */

static void daCellFormatter(int row, int col, char *buf, int bufLen,
                            void *userData);

/* ----------------------------------------------------------------
   Build flat array from linked list, applying filters
   ---------------------------------------------------------------- */

static NcTuiTable *gTui = NULL;

static void buildASArray(DAData *data) {
  if (data->asArray) {
    free(data->asArray);
    data->asArray = NULL;
  }

  /* First pass: count matching entries (command-line filters) */
  int total = 0;
  for (ASInfo *a = data->asList; a; a = a->next) {
    if (!data->showAll && a->isSystem && a->cpuPercent < 0.01 &&
        a->asid <= 3) {
      continue;
    }
    if (data->showSTC && !a->isSTC) continue;
    if (data->showTSO && !a->isTSO) continue;
    total++;
  }

  if (total == 0) { data->asCount = 0; return; }

  /* Build unfiltered array (after command-line filters) */
  ASInfo **tempArray = (ASInfo **)malloc(total * sizeof(ASInfo *));
  if (!tempArray) { data->asCount = 0; return; }

  int idx = 0;
  for (ASInfo *a = data->asList; a; a = a->next) {
    if (!data->showAll && a->isSystem && a->cpuPercent < 0.01 &&
        a->asid <= 3) {
      continue;
    }
    if (data->showSTC && !a->isSTC) continue;
    if (data->showTSO && !a->isTSO) continue;
    tempArray[idx++] = a;
  }

  /* Check for column filters */
  int anyFilter = 0;
  if (gTui) {
    for (int c = 0; c < gTui->numColumns; c++) {
      if (gTui->columns[c].filter[0]) { anyFilter = 1; break; }
    }
  }

  if (!anyFilter) {
    data->asArray = tempArray;
    data->asCount = total;
    return;
  }

  /* Apply column filters */
  data->asArray = tempArray;
  data->asCount = total;

  int matchCount = 0;
  char buf[NCTUI_MAX_COL_WIDTH + 1];
  for (int i = 0; i < total; i++) {
    int pass = 1;
    for (int c = 0; c < gTui->numColumns && pass; c++) {
      if (gTui->columns[c].filter[0] == '\0') continue;
      buf[0] = '\0';
      daCellFormatter(i, c, buf, sizeof(buf), data);
      if (!nctuiMatchFilter(gTui->columns[c].filter, buf)) pass = 0;
    }
    if (pass) matchCount++;
  }

  ASInfo **filtered = (ASInfo **)malloc(matchCount * sizeof(ASInfo *));
  if (!filtered) return;

  idx = 0;
  for (int i = 0; i < total; i++) {
    int pass = 1;
    for (int c = 0; c < gTui->numColumns && pass; c++) {
      if (gTui->columns[c].filter[0] == '\0') continue;
      buf[0] = '\0';
      daCellFormatter(i, c, buf, sizeof(buf), data);
      if (!nctuiMatchFilter(gTui->columns[c].filter, buf)) pass = 0;
    }
    if (pass) filtered[idx++] = tempArray[i];
  }

  free(tempArray);
  data->asArray = filtered;
  data->asCount = matchCount;
}

/* ----------------------------------------------------------------
   Cell formatter
   ---------------------------------------------------------------- */

static void daCellFormatter(int row, int col, char *buf, int bufLen,
                            void *userData) {
  DAData *data = (DAData *)userData;
  if (row < 0 || row >= data->asCount) { buf[0] = '\0'; return; }
  ASInfo *info = data->asArray[row];

  switch (col) {
  case COL_JOBNAME:
    snprintf(buf, bufLen, "%s", info->jobName);
    break;
  case COL_ASID:
    snprintf(buf, bufLen, "%04X", info->asid);
    break;
  case COL_OWNER:
    snprintf(buf, bufLen, "%s", info->userid);
    break;
  case COL_TYPE:
    snprintf(buf, bufLen, "%s", asType(info));
    break;
  case COL_DP:
    snprintf(buf, bufLen, "%d", info->dispPriority);
    break;
  case COL_REAL:
    /* frames * 4 = KB */
    if (info->realFrames > 0) {
      snprintf(buf, bufLen, "%u", info->realFrames * 4);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_CPU_PCT:
    if (info->cpuPercent > 0.005) {
      snprintf(buf, bufLen, "%.1f", info->cpuPercent);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_EXCP:
    if (info->excpCount > 0) {
      snprintf(buf, bufLen, "%u", info->excpCount);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_IO_RATE:
    if (info->ioRate > 0.5) {
      snprintf(buf, bufLen, "%.1f", info->ioRate);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_STATUS:
    snprintf(buf, bufLen, "%s", dspStatus(info->dsp1));
    break;
  default:
    buf[0] = '\0';
    break;
  }
}

/* ----------------------------------------------------------------
   Refresh handler
   ---------------------------------------------------------------- */

static int daRefreshHandler(void *userData) {
  DAData *data = (DAData *)userData;

  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t elapsedMicros = 0;
  if (data->lastRefresh.tv_sec > 0) {
    elapsedMicros = (uint64_t)(now.tv_sec - data->lastRefresh.tv_sec) * 1000000ULL
                  + (uint64_t)(now.tv_usec - data->lastRefresh.tv_usec);
  }
  data->lastRefresh = now;

  int totalAS = asInfoRefresh(&data->asList, elapsedMicros);
  if (totalAS < 0) totalAS = 0;

  buildASArray(data);
  return data->asCount;
}

/* ----------------------------------------------------------------
   Command handler
   ---------------------------------------------------------------- */

static int daCommandHandler(const char *command, void *userData) {
  DAData *data = (DAData *)userData;

  if (command[0] == 'q' || command[0] == 'Q') {
    return 1;  /* quit */
  }
  if (strcmp(command, "ALL") == 0 || strcmp(command, "all") == 0) {
    data->showAll = !data->showAll;
    buildASArray(data);
    return 0;
  }
  if (strcmp(command, "STC") == 0 || strcmp(command, "stc") == 0) {
    data->showSTC = !data->showSTC;
    data->showTSO = 0;
    buildASArray(data);
    return 0;
  }
  if (strcmp(command, "TSO") == 0 || strcmp(command, "tso") == 0) {
    data->showTSO = !data->showTSO;
    data->showSTC = 0;
    buildASArray(data);
    return 0;
  }
  return 0;
}

/* ----------------------------------------------------------------
   Filter handler: rebuild array when filters change
   ---------------------------------------------------------------- */

static void daFilterHandler(void *userData) {
  DAData *data = (DAData *)userData;
  buildASArray(data);
  if (gTui) {
    gTui->rowCount = data->asCount;
    gTui->scrollRow = 0;
    gTui->selectedRow = -1;
  }
}

/* ----------------------------------------------------------------
   Sort handler: qsort the address space array by selected column
   ---------------------------------------------------------------- */

static int daSortCompare(const void *a, const void *b) {
  if (!gTui) return 0;
  ASInfo **aa = (ASInfo **)a;
  ASInfo **ab = (ASInfo **)b;

  char bufA[NCTUI_MAX_COL_WIDTH + 1] = "";
  char bufB[NCTUI_MAX_COL_WIDTH + 1] = "";

  int col = gTui->sortColumn;
  switch (col) {
  case COL_JOBNAME:
    strncpy(bufA, (*aa)->jobName, sizeof(bufA)-1);
    strncpy(bufB, (*ab)->jobName, sizeof(bufB)-1);
    break;
  case COL_ASID:
    snprintf(bufA, sizeof(bufA), "%04X", (*aa)->asid);
    snprintf(bufB, sizeof(bufB), "%04X", (*ab)->asid);
    break;
  case COL_OWNER:
    strncpy(bufA, (*aa)->userid, sizeof(bufA)-1);
    strncpy(bufB, (*ab)->userid, sizeof(bufB)-1);
    break;
  case COL_TYPE:
    strncpy(bufA, asType(*aa), sizeof(bufA)-1);
    strncpy(bufB, asType(*ab), sizeof(bufB)-1);
    break;
  case COL_DP:
    snprintf(bufA, sizeof(bufA), "%08d", (*aa)->dispPriority);
    snprintf(bufB, sizeof(bufB), "%08d", (*ab)->dispPriority);
    break;
  case COL_REAL:
    snprintf(bufA, sizeof(bufA), "%08u", (*aa)->realFrames);
    snprintf(bufB, sizeof(bufB), "%08u", (*ab)->realFrames);
    break;
  case COL_CPU_PCT:
    snprintf(bufA, sizeof(bufA), "%012.4f", (*aa)->cpuPercent);
    snprintf(bufB, sizeof(bufB), "%012.4f", (*ab)->cpuPercent);
    break;
  case COL_EXCP:
    snprintf(bufA, sizeof(bufA), "%08u", (*aa)->excpCount);
    snprintf(bufB, sizeof(bufB), "%08u", (*ab)->excpCount);
    break;
  case COL_IO_RATE:
    snprintf(bufA, sizeof(bufA), "%012.4f", (*aa)->ioRate);
    snprintf(bufB, sizeof(bufB), "%012.4f", (*ab)->ioRate);
    break;
  case COL_STATUS:
    strncpy(bufA, dspStatus((*aa)->dsp1), sizeof(bufA)-1);
    strncpy(bufB, dspStatus((*ab)->dsp1), sizeof(bufB)-1);
    break;
  default:
    strncpy(bufA, (*aa)->jobName, sizeof(bufA)-1);
    strncpy(bufB, (*ab)->jobName, sizeof(bufB)-1);
    break;
  }

  int cmp = strcmp(bufA, bufB);
  return gTui->sortAscending ? cmp : -cmp;
}

static void daSortHandler(int col, int ascending, void *userData) {
  DAData *data = (DAData *)userData;
  if (data->asCount > 1 && data->asArray) {
    qsort(data->asArray, data->asCount, sizeof(ASInfo *), daSortCompare);
  }
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  DAData data;
  memset(&data, 0, sizeof(data));
  data.refreshSec = 5;

  /* Parse args */
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-interval") && i + 1 < argc) {
      data.refreshSec = atoi(argv[++i]);
      if (data.refreshSec < 1) data.refreshSec = 1;
    } else if (!strcmp(argv[i], "-all")) {
      data.showAll = 1;
    } else if (!strcmp(argv[i], "-stc")) {
      data.showSTC = 1;
    } else if (!strcmp(argv[i], "-tso")) {
      data.showTSO = 1;
    } else if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "-h")) {
      printf("Usage: nczda [options]\n");
      printf("  -interval <sec>   Refresh interval (default 5)\n");
      printf("  -all              Show all address spaces\n");
      printf("  -stc              Show started tasks only\n");
      printf("  -tso              Show TSO users only\n");
      printf("  -help             Show this help\n");
      return 0;
    }
  }

  /* Initial scan */
  gettimeofday(&data.lastRefresh, NULL);
  int totalAS = asInfoGetAll(&data.asList);
  if (totalAS < 0) {
    fprintf(stderr, "asInfoGetAll failed\n");
    return 1;
  }
  buildASArray(&data);

  /* Set up navigator */
  NcTuiNav nav;
  nctuiNavInit(&nav);

  NcTuiPage daPage;
  nctuiPageInitTable(&daPage, "DA");
  nctuiSetColumns(&daPage.table, daColumns, COL_COUNT, 2);
  nctuiSetTitle(&daPage.table, "ZDA DISPLAY ACTIVE");
  daPage.table.rowCount = data.asCount;
  daPage.table.cellFormatter = daCellFormatter;
  daPage.table.commandHandler = daCommandHandler;
  daPage.table.refreshHandler = daRefreshHandler;
  daPage.table.filterHandler = daFilterHandler;
  daPage.table.sortHandler = daSortHandler;
  daPage.table.userData = &data;

  nctuiNavPush(&nav, &daPage);
  gTui = &nav.stack[0].table;

  nctuiNavRun(&nav);
  nctuiNavTerm(&nav);

  /* Cleanup */
  asInfoFree(data.asList);
  if (data.asArray) free(data.asArray);

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
