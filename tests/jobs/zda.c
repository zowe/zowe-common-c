

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  zda — SDSF DA (Display Active) panel equivalent.

  Full-screen TUI displaying active address spaces with performance data.
  Walks CVT -> ASVT -> ASCB chain to enumerate address spaces.
  Auto-refreshes to compute CPU%, I/O rate, etc.

  Usage:
    zda [options]
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
#include "tui.h"
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

static TuiColumn daColumns[] = {
  { "JobName",  8,  TUI_ALIGN_LEFT  },
  { "ASID",     5,  TUI_ALIGN_RIGHT },
  { "Owner",    8,  TUI_ALIGN_LEFT  },
  { "Type",     4,  TUI_ALIGN_LEFT  },
  { "DP",       4,  TUI_ALIGN_RIGHT },
  { "Real-K",   8,  TUI_ALIGN_RIGHT },
  { "CPU%",     6,  TUI_ALIGN_RIGHT },
  { "EXCP",     6,  TUI_ALIGN_RIGHT },
  { "IO/s",     7,  TUI_ALIGN_RIGHT },
  { "Status",   12, TUI_ALIGN_LEFT  },
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
   Build flat array from linked list, applying filters
   ---------------------------------------------------------------- */

static void buildASArray(DAData *data) {
  if (data->asArray) {
    free(data->asArray);
    data->asArray = NULL;
  }

  /* First pass: count matching entries */
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

  data->asCount = total;
  if (total == 0) return;

  data->asArray = (ASInfo **)malloc(total * sizeof(ASInfo *));
  if (!data->asArray) { data->asCount = 0; return; }

  int idx = 0;
  for (ASInfo *a = data->asList; a; a = a->next) {
    if (!data->showAll && a->isSystem && a->cpuPercent < 0.01 &&
        a->asid <= 3) {
      continue;
    }
    if (data->showSTC && !a->isSTC) continue;
    if (data->showTSO && !a->isTSO) continue;
    data->asArray[idx++] = a;
  }
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
      printf("Usage: zda [options]\n");
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

  /* Set up TUI */
  TuiTable tui;
  memset(&tui, 0, sizeof(tui));

  tuiSetColumns(&tui, daColumns, COL_COUNT, 2);  /* JobName, ASID fixed */
  tuiSetTitle(&tui, "ZDA DISPLAY ACTIVE");
  tui.rowCount = data.asCount;
  tui.cellFormatter = daCellFormatter;
  tui.commandHandler = daCommandHandler;
  tui.refreshHandler = daRefreshHandler;
  tui.userData = &data;

  tuiInit(&tui);
  tuiEventLoop(&tui);
  tuiTerm(&tui);

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
