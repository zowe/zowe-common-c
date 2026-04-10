

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  zst — SDSF ST (Status) panel equivalent.

  Full-screen TUI displaying job queue status using SSI 80 (Extended Status).
  Uses ncurses for the alternate-screen, scrollable display.

  Usage:
    zst [options]
      -name <pattern>   Filter by job name
      -owner <userid>   Filter by owner
      -stc              Show started tasks only
      -tsu              Show TSO users only
      -job              Show batch jobs only
      -max <n>          Limit number of jobs
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zowetypes.h"
#include "alloc.h"
#include "jobservice.h"
#include "tui.h"

/* ----------------------------------------------------------------
   Data model: flat array of JobInfo pointers for random access
   ---------------------------------------------------------------- */

typedef struct STData_tag {
  JobService       *service;
  JobServiceFilter  filter;
  JobInfo          *jobList;     /* linked list from jobServiceGetJobs */
  JobInfo         **jobArray;    /* flat array for indexed access */
  int               jobCount;
} STData;

/* ----------------------------------------------------------------
   Phase-to-queue-name mapping
   ---------------------------------------------------------------- */

static const char *phaseToQueue(uint8_t phase) {
  if (phase >= JOB_PHASE_INPUT && phase <= JOB_PHASE_CONV) {
    return "INPUT";
  }
  if (phase >= JOB_PHASE_SETUP && phase <= JOB_PHASE_SPIN) {
    return "EXECUTION";
  }
  if (phase == JOB_PHASE_EXEC) {
    return "EXECUTION";
  }
  if (phase >= JOB_PHASE_WTBKDN && phase <= JOB_PHASE_PURG) {
    return "PRINT";
  }
  if (phase == JOB_PHASE_POSTEX) {
    return "PRINT";
  }
  /* detailed phases */
  if (phase >= JOB_PHASE_NOSUB && phase <= JOB_PHASE_DEMSEL) {
    /* sub-phases 1-24: mostly execution or setup related */
    if (phase <= JOB_PHASE_FETCH) return "INPUT";
    if (phase <= JOB_PHASE_ONMAIN) return "EXECUTION";
    if (phase >= JOB_PHASE_BRKDWN && phase <= JOB_PHASE_DONE) return "EXECUTION";
    if (phase >= JOB_PHASE_OUTPT && phase <= JOB_PHASE_CMPLT) return "PRINT";
    return "EXECUTION";
  }
  return "";
}

/* ----------------------------------------------------------------
   Completion status formatting (Max-RC column)
   ---------------------------------------------------------------- */

static void formatMaxRC(JobInfo *job, char *buf, int bufLen) {
  switch (job->compType) {
  case JOB_COMP_NORMAL:
    snprintf(buf, bufLen, "CC %04X", job->compCode);
    break;
  case JOB_COMP_ABEND:
    if (job->compCode & 0xFFF000) {
      snprintf(buf, bufLen, "ABEND S%03X", (job->compCode >> 12) & 0xFFF);
    } else {
      snprintf(buf, bufLen, "ABEND U%04d", job->compCode & 0xFFF);
    }
    break;
  case JOB_COMP_JCLERR:
    snprintf(buf, bufLen, "JCL ERROR");
    break;
  case JOB_COMP_CANCELED:
    snprintf(buf, bufLen, "CANCELED");
    break;
  case JOB_COMP_CC:
    snprintf(buf, bufLen, "CC %04X", job->compCode);
    break;
  case JOB_COMP_CNVABEND:
    snprintf(buf, bufLen, "CNV ABEND");
    break;
  case JOB_COMP_SECERR:
    snprintf(buf, bufLen, "SEC ERROR");
    break;
  case JOB_COMP_EOM:
    snprintf(buf, bufLen, "EOM FAIL");
    break;
  case JOB_COMP_CNVERR:
    snprintf(buf, bufLen, "CNV ERROR");
    break;
  case JOB_COMP_SYSFAIL:
    snprintf(buf, bufLen, "SYS FAIL");
    break;
  case JOB_COMP_FLUSHED:
    snprintf(buf, bufLen, "FLUSHED");
    break;
  default:
    buf[0] = '\0';
    break;
  }
}

/* ----------------------------------------------------------------
   Column indices — must match the column definition array
   ---------------------------------------------------------------- */

enum {
  COL_NP = 0,
  COL_JOBNAME,
  COL_JOBID,
  COL_OWNER,
  COL_PRTY,
  COL_QUEUE,
  COL_CLASS,
  COL_POS,
  COL_SAFF,
  COL_ASYS,
  COL_STATUS,
  COL_PRTDEST,
  COL_SECLABEL,
  COL_TGNUM,
  COL_TGPCT,
  COL_ORIGNODE,
  COL_EXECNODE,
  COL_DEVICE,
  COL_MAXRC,
  COL_SRVCLASS,
  COL_WPOS,
  COL_SCHEDENV,
  COL_DLY,
  COL_MODE,
  COL_COUNT
};

static TuiColumn stColumns[] = {
  { "NP",       4,  TUI_ALIGN_LEFT  },
  { "JOBNAME",  8,  TUI_ALIGN_LEFT  },
  { "JobID",    8,  TUI_ALIGN_LEFT  },
  { "Owner",    8,  TUI_ALIGN_LEFT  },
  { "Prty",     4,  TUI_ALIGN_RIGHT },
  { "Queue",    10, TUI_ALIGN_LEFT  },
  { "C",        1,  TUI_ALIGN_LEFT  },
  { "Pos",      5,  TUI_ALIGN_RIGHT },
  { "SAff",     4,  TUI_ALIGN_LEFT  },
  { "ASys",     4,  TUI_ALIGN_LEFT  },
  { "Status",   16, TUI_ALIGN_LEFT  },
  { "PrtDest",  18, TUI_ALIGN_LEFT  },
  { "SecLabel", 8,  TUI_ALIGN_LEFT  },
  { "TGNum",    5,  TUI_ALIGN_RIGHT },
  { "TGPct",    5,  TUI_ALIGN_RIGHT },
  { "OrigNode", 8,  TUI_ALIGN_LEFT  },
  { "ExecNode", 8,  TUI_ALIGN_LEFT  },
  { "Device",   18, TUI_ALIGN_LEFT  },
  { "Max-RC",   10, TUI_ALIGN_LEFT  },
  { "SrvClass", 8,  TUI_ALIGN_LEFT  },
  { "WPos",     4,  TUI_ALIGN_RIGHT },
  { "Sched-Env",16, TUI_ALIGN_LEFT  },
  { "Dly",      3,  TUI_ALIGN_LEFT  },
  { "Mode",     3,  TUI_ALIGN_LEFT  },
};

/* ----------------------------------------------------------------
   Phase name for status display
   ---------------------------------------------------------------- */

static const char *phaseName(uint8_t phase) {
  switch (phase) {
  case JOB_PHASE_INPUT:    return "INPUT";
  case JOB_PHASE_WTCONV:   return "WAIT CONV";
  case JOB_PHASE_CONV:     return "CONVERTING";
  case JOB_PHASE_SETUP:    return "SETUP";
  case JOB_PHASE_EXEC:     return "EXECUTING";
  case JOB_PHASE_SPIN:     return "SPIN";
  case JOB_PHASE_WTBKDN:   return "WAIT BRKDN";
  case JOB_PHASE_POSTEX:   return "OUTPUT";
  case JOB_PHASE_WTPURG:   return "WAIT PURGE";
  case JOB_PHASE_PURG:     return "PURGING";
  case JOB_PHASE_RECV:     return "RECEIVING";
  case JOB_PHASE_WTXMIT:   return "WAIT XMIT";
  case JOB_PHASE_XMIT:     return "XMITTING";
  case JOB_PHASE_SELECT:   return "AWAITING SEL";
  case JOB_PHASE_ONMAIN:   return "ON MAIN";
  case JOB_PHASE_OUTPT:    return "OUTPUT";
  case JOB_PHASE_OUTQUE:   return "OUTPUT WTR";
  case JOB_PHASE_CMPLT:    return "COMPLETE";
  default:                 return "";
  }
}

/* ----------------------------------------------------------------
   Cell formatter callback
   ---------------------------------------------------------------- */

static void stCellFormatter(int row, int col, char *buf, int bufLen,
                            void *userData) {
  STData *data = (STData *)userData;
  if (row < 0 || row >= data->jobCount) {
    buf[0] = '\0';
    return;
  }
  JobInfo *job = data->jobArray[row];

  switch (col) {
  case COL_NP:
    buf[0] = '\0';
    break;
  case COL_JOBNAME:
    snprintf(buf, bufLen, "%s", job->jobName);
    break;
  case COL_JOBID:
    snprintf(buf, bufLen, "%s", job->jobId);
    break;
  case COL_OWNER:
    snprintf(buf, bufLen, "%s", job->owner);
    break;
  case COL_PRTY:
    snprintf(buf, bufLen, "%d", job->priority);
    break;
  case COL_QUEUE:
    snprintf(buf, bufLen, "%s", phaseToQueue(job->phase));
    break;
  case COL_CLASS:
    snprintf(buf, bufLen, "%c", job->jobClass[0] ? job->jobClass[0] : ' ');
    break;
  case COL_POS:
    if (job->queuePos > 0) {
      snprintf(buf, bufLen, "%u", job->queuePos);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_SAFF:
    /* system affinity — show first 4 chars of active system */
    snprintf(buf, bufLen, "%.4s", job->activeSys);
    break;
  case COL_ASYS:
    snprintf(buf, bufLen, "%.4s", job->activeSys);
    break;
  case COL_STATUS: {
    const char *ph = phaseName(job->phase);
    if (ph[0]) {
      snprintf(buf, bufLen, "%s", ph);
    } else {
      buf[0] = '\0';
    }
    break;
  }
  case COL_PRTDEST:
    snprintf(buf, bufLen, "%s", job->printDest);
    break;
  case COL_SECLABEL:
    snprintf(buf, bufLen, "%s", job->secLabel);
    break;
  case COL_TGNUM:
    if (job->spoolTrackGroups >= 0) {
      snprintf(buf, bufLen, "%d", job->spoolTrackGroups);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_TGPCT:
    if (job->spoolTrackGroups >= 0) {
      /* approximate: show as fraction — real % would need total spool size */
      snprintf(buf, bufLen, "0.%02d", job->spoolTrackGroups % 100);
    } else {
      buf[0] = '\0';
    }
    break;
  case COL_ORIGNODE:
    snprintf(buf, bufLen, "%s", job->originNode);
    break;
  case COL_EXECNODE:
    snprintf(buf, bufLen, "%s", job->execNode);
    break;
  case COL_DEVICE:
    snprintf(buf, bufLen, "%s", job->device);
    break;
  case COL_MAXRC:
    formatMaxRC(job, buf, bufLen);
    break;
  case COL_SRVCLASS:
    snprintf(buf, bufLen, "%s", job->serviceClass);
    break;
  case COL_WPOS:
    if (job->wlmQueuePos > 0) {
      snprintf(buf, bufLen, "%d", job->wlmQueuePos);
    } else {
      snprintf(buf, bufLen, "0");
    }
    break;
  case COL_SCHEDENV:
    snprintf(buf, bufLen, "%s", job->schedEnv);
    break;
  case COL_DLY:
    snprintf(buf, bufLen, "%s", job->delayFlags ? "YES" : "");
    break;
  case COL_MODE:
    if (job->wlmMode) {
      snprintf(buf, bufLen, "WLM");
    } else if (job->phase == JOB_PHASE_EXEC || job->phase == JOB_PHASE_ONMAIN) {
      snprintf(buf, bufLen, "JES");
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
   Build flat array from linked list, applying column filters
   ---------------------------------------------------------------- */

static TuiTable *gTui = NULL;  /* for filter/sort access */

static int jobPassesFilters(STData *data, JobInfo *job, int rawIndex) {
  if (!gTui) return 1;
  char buf[TUI_MAX_COL_WIDTH + 1];
  for (int c = 0; c < gTui->numColumns; c++) {
    if (gTui->columns[c].filter[0] == '\0') continue;
    buf[0] = '\0';
    stCellFormatter(rawIndex, c, buf, sizeof(buf), data);
    if (!tuiMatchFilter(gTui->columns[c].filter, buf)) {
      return 0;
    }
  }
  return 1;
}

static void buildJobArray(STData *data) {
  if (data->jobArray) {
    free(data->jobArray);
    data->jobArray = NULL;
  }

  /* Count total jobs in list */
  int total = 0;
  for (JobInfo *j = data->jobList; j; j = j->next) total++;
  if (total <= 0) { data->jobCount = 0; return; }

  /* Build unfiltered temp array first (needed for cellFormatter indexing) */
  JobInfo **tempArray = (JobInfo **)malloc(total * sizeof(JobInfo *));
  if (!tempArray) { data->jobCount = 0; return; }
  {
    int i = 0;
    for (JobInfo *j = data->jobList; j; j = j->next) {
      tempArray[i++] = j;
    }
  }

  /* If no filters active, use temp directly */
  int anyFilter = 0;
  if (gTui) {
    for (int c = 0; c < gTui->numColumns; c++) {
      if (gTui->columns[c].filter[0]) { anyFilter = 1; break; }
    }
  }

  if (!anyFilter) {
    data->jobArray = tempArray;
    data->jobCount = total;
    return;
  }

  /* Filter pass: temporarily set jobArray to unfiltered for cellFormatter */
  data->jobArray = tempArray;
  data->jobCount = total;

  /* Count matches */
  int matchCount = 0;
  for (int i = 0; i < total; i++) {
    if (jobPassesFilters(data, tempArray[i], i)) matchCount++;
  }

  /* Build filtered array */
  JobInfo **filtered = (JobInfo **)malloc(matchCount * sizeof(JobInfo *));
  if (!filtered) {
    data->jobCount = total;
    return;
  }
  int idx = 0;
  for (int i = 0; i < total; i++) {
    if (jobPassesFilters(data, tempArray[i], i)) {
      filtered[idx++] = tempArray[i];
    }
  }

  free(tempArray);
  data->jobArray = filtered;
  data->jobCount = matchCount;
}

/* ----------------------------------------------------------------
   Refresh handler: re-fetch jobs from SSI 80
   ---------------------------------------------------------------- */

static int stRefreshHandler(void *userData) {
  STData *data = (STData *)userData;

  /* Free old data */
  if (data->jobList) {
    jobServiceFreeJobs(data->service, data->jobList);
    data->jobList = NULL;
    data->jobCount = 0;
  }

  /* Re-init service (new storage anchor) */
  jobServiceTerm(data->service);
  jobServiceInit(&data->service);

  /* Fetch jobs */
  int rc = jobServiceGetJobs(data->service, &data->filter,
                             &data->jobList, &data->jobCount);
  if (rc != 0) {
    data->jobCount = 0;
  }
  buildJobArray(data);
  return data->jobCount;
}

/* ----------------------------------------------------------------
   Sysout record collector for text viewer
   ---------------------------------------------------------------- */

#define MAX_SYSOUT_LINES 50000

typedef struct SysoutCollector_tag {
  char **lines;
  int    lineCount;
  int    maxLines;
} SysoutCollector;

static int sysoutRecordHandler(const char *record, int recordLen,
                               int lineNumber, void *userData) {
  SysoutCollector *coll = (SysoutCollector *)userData;
  if (coll->lineCount >= coll->maxLines) return 1;  /* stop */

  char *line = (char *)malloc(recordLen + 1);
  if (!line) return 1;
  memcpy(line, record, recordLen);
  line[recordLen] = '\0';

  /* trim trailing spaces */
  int end = recordLen - 1;
  while (end >= 0 && line[end] == ' ') line[end--] = '\0';

  coll->lines[coll->lineCount++] = line;
  return 0;
}

static void freeSysoutLines(SysoutCollector *coll) {
  for (int i = 0; i < coll->lineCount; i++) {
    free(coll->lines[i]);
  }
  free(coll->lines);
  coll->lines = NULL;
  coll->lineCount = 0;
}

/* ----------------------------------------------------------------
   DD list cell formatter — for the sysout drill-down sub-table
   ---------------------------------------------------------------- */

typedef struct DDListData_tag {
  JobSysoutDataset **dsArray;
  int                dsCount;
} DDListData;

enum {
  DDCOL_NUM = 0,
  DDCOL_STEP,
  DDCOL_PROC,
  DDCOL_DD,
  DDCOL_RECFM,
  DDCOL_LRECL,
  DDCOL_LINES,
  DDCOL_PAGES,
  DDCOL_COUNT
};

static TuiColumn ddColumns[] = {
  { "#",      4,  TUI_ALIGN_RIGHT },
  { "Step",   8,  TUI_ALIGN_LEFT  },
  { "Proc",   8,  TUI_ALIGN_LEFT  },
  { "DDName", 8,  TUI_ALIGN_LEFT  },
  { "Recfm",  5,  TUI_ALIGN_LEFT  },
  { "Lrecl",  5,  TUI_ALIGN_RIGHT },
  { "Lines",  8,  TUI_ALIGN_RIGHT },
  { "Pages",  6,  TUI_ALIGN_RIGHT },
};

static void ddCellFormatter(int row, int col, char *buf, int bufLen,
                            void *userData) {
  DDListData *data = (DDListData *)userData;
  if (row < 0 || row >= data->dsCount) { buf[0] = '\0'; return; }
  JobSysoutDataset *ds = data->dsArray[row];

  switch (col) {
  case DDCOL_NUM:
    snprintf(buf, bufLen, "%d", row + 1);
    break;
  case DDCOL_STEP:
    snprintf(buf, bufLen, "%s", ds->stepName);
    break;
  case DDCOL_PROC:
    snprintf(buf, bufLen, "%s", ds->procName);
    break;
  case DDCOL_DD:
    snprintf(buf, bufLen, "%s", ds->ddName);
    break;
  case DDCOL_RECFM: {
    char r[8] = "";
    if (ds->recfm & 0x80) strcat(r, "F");
    else if (ds->recfm & 0x40) strcat(r, "V");
    else if (ds->recfm & 0xC0) strcat(r, "U");
    if (ds->recfm & 0x10) strcat(r, "B");
    if (ds->recfm & 0x08) strcat(r, "S");
    if (ds->recfm & 0x04) strcat(r, "A");
    if (ds->recfm & 0x02) strcat(r, "M");
    if (r[0] == '\0') strcpy(r, "?");
    snprintf(buf, bufLen, "%s", r);
    break;
  }
  case DDCOL_LRECL:
    snprintf(buf, bufLen, "%u", ds->lrecl);
    break;
  case DDCOL_LINES:
    if (ds->countsValid && ds->lineCount > 0)
      snprintf(buf, bufLen, "%u", ds->lineCount);
    else
      buf[0] = '\0';
    break;
  case DDCOL_PAGES:
    if (ds->countsValid && ds->pageCount > 0)
      snprintf(buf, bufLen, "%u", ds->pageCount);
    else
      buf[0] = '\0';
    break;
  default:
    buf[0] = '\0';
    break;
  }
}

/* ----------------------------------------------------------------
   Show sysout content for a single DD using text viewer
   ---------------------------------------------------------------- */

static void showSysoutContent(JobService *svc, JobInfo *job,
                              JobSysoutDataset *ds) {
  if (!ds->tokenValid) return;

  SysoutCollector coll;
  memset(&coll, 0, sizeof(coll));
  coll.maxLines = MAX_SYSOUT_LINES;
  coll.lines = (char **)malloc(coll.maxLines * sizeof(char *));
  if (!coll.lines) return;

  int recordsRead = 0;
  int rc = jobServiceReadSysout(svc, ds->clientToken,
                                coll.maxLines, sysoutRecordHandler,
                                &coll, &recordsRead);

  if (coll.lineCount > 0) {
    TuiTextView tv;
    memset(&tv, 0, sizeof(tv));
    snprintf(tv.title, sizeof(tv.title), "%s %s %s.%s.%s",
             job->jobName, job->jobId,
             ds->procName[0] ? ds->procName : "-",
             ds->stepName[0] ? ds->stepName : "-",
             ds->ddName);
    tv.lines = coll.lines;
    tv.lineCount = coll.lineCount;
    tuiTextViewShow(&tv);
  }

  freeSysoutLines(&coll);
}

/* ----------------------------------------------------------------
   DD list select handler — view content of selected DD
   ---------------------------------------------------------------- */

static int selectedDDRow = -1;

static int ddSelectHandler(int row, void *userData) {
  selectedDDRow = row;
  return 1;  /* exit dd event loop */
}

/* ----------------------------------------------------------------
   Show sysout DD list for a job (drill-down from ST)
   ---------------------------------------------------------------- */

static void showJobSysout(STData *data, int jobRow) {
  if (jobRow < 0 || jobRow >= data->jobCount) return;
  JobInfo *origJob = data->jobArray[jobRow];

  /* Re-fetch this single job with verbose detail to get sysout tokens */
  JobServiceFilter verboseFilter;
  memset(&verboseFilter, 0, sizeof(verboseFilter));
  verboseFilter.flags = JOB_SELECT_BY_JOBID;
  strncpy(verboseFilter.jobIdLow, origJob->jobId, 8);
  strncpy(verboseFilter.jobIdHigh, origJob->jobId, 8);
  verboseFilter.detailLevel = JOB_DETAIL_VERBOSE;
  verboseFilter.maxJobs = 1;

  JobService *svc2 = NULL;
  jobServiceInit(&svc2);

  JobInfo *verboseJob = NULL;
  int verboseCount = 0;
  int rc = jobServiceGetJobs(svc2, &verboseFilter,
                             &verboseJob, &verboseCount);
  if (rc != 0 || !verboseJob) {
    if (svc2) jobServiceTerm(svc2);
    return;
  }

  /* Flatten sysout datasets into an array */
  int totalDS = 0;
  for (JobSysout *so = verboseJob->sysouts; so; so = so->next) {
    for (JobSysoutDataset *ds = so->datasets; ds; ds = ds->next) {
      totalDS++;
    }
  }

  if (totalDS == 0) {
    jobServiceFreeJobs(svc2, verboseJob);
    jobServiceTerm(svc2);
    return;
  }

  JobSysoutDataset **dsArray = (JobSysoutDataset **)malloc(
      totalDS * sizeof(JobSysoutDataset *));
  if (!dsArray) {
    jobServiceFreeJobs(svc2, verboseJob);
    jobServiceTerm(svc2);
    return;
  }

  int idx = 0;
  for (JobSysout *so = verboseJob->sysouts; so; so = so->next) {
    for (JobSysoutDataset *ds = so->datasets; ds; ds = ds->next) {
      dsArray[idx++] = ds;
    }
  }

  /* Show DD list as a sub-table */
  DDListData ddData;
  ddData.dsArray = dsArray;
  ddData.dsCount = totalDS;

  TuiTable ddTui;
  memset(&ddTui, 0, sizeof(ddTui));

  char ddTitle[128];
  snprintf(ddTitle, sizeof(ddTitle), "SYSOUT DATASETS: %s %s",
           verboseJob->jobName, verboseJob->jobId);

  tuiSetColumns(&ddTui, ddColumns, DDCOL_COUNT, 1);
  tuiSetTitle(&ddTui, ddTitle);
  ddTui.rowCount = totalDS;
  ddTui.cellFormatter = ddCellFormatter;
  ddTui.selectHandler = ddSelectHandler;
  ddTui.userData = &ddData;
  ddTui.selectedRow = -1;

  /* Sub-table reuses existing raw mode + alt screen from parent.
     tuiEventLoop -> tuiRender will query screen size automatically. */
  ddTui.scrollRow = 0;
  ddTui.scrollCol = ddTui.fixedColumns;
  ddTui.cmdBuf[0] = '\0';
  ddTui.cmdPos = 0;
  ddTui.cmdActive = 1;
  ddTui.useColor = 1;
  ddTui.origTermValid = 0;

  while (1) {
    selectedDDRow = -1;
    tuiEventLoop(&ddTui);

    if (selectedDDRow >= 0 && selectedDDRow < totalDS) {
      JobSysoutDataset *ds = dsArray[selectedDDRow];
      if (ds->tokenValid) {
        showSysoutContent(svc2, verboseJob, ds);
        /* After text viewer returns, re-show DD list */
        continue;
      }
    }
    break;  /* F3 or quit from DD list */
  }

  free(dsArray);
  jobServiceFreeJobs(svc2, verboseJob);
  jobServiceTerm(svc2);
}

/* ----------------------------------------------------------------
   Filter handler: rebuild array when filters change
   ---------------------------------------------------------------- */

static void stFilterHandler(void *userData) {
  STData *data = (STData *)userData;
  buildJobArray(data);
  if (gTui) {
    gTui->rowCount = data->jobCount;
    gTui->scrollRow = 0;
    gTui->selectedRow = -1;
  }
}

/* ----------------------------------------------------------------
   Sort handler: qsort the job array by selected column
   ---------------------------------------------------------------- */

static int stSortCompare(const void *a, const void *b) {
  if (!gTui) return 0;
  JobInfo **ja = (JobInfo **)a;
  JobInfo **jb = (JobInfo **)b;

  /* Find indices for cellFormatter — since the array is being sorted
     we format directly from the JobInfo pointers using temp indices */
  char bufA[TUI_MAX_COL_WIDTH + 1] = "";
  char bufB[TUI_MAX_COL_WIDTH + 1] = "";

  /* We need to call the formatter with the right row data.
     Trick: temporarily poke the pointer into index 0 of a temp STData */
  STData *data = (STData *)gTui->userData;
  JobInfo *origA = data->jobArray[0];

  /* Just format from the JobInfo directly by column */
  int col = gTui->sortColumn;
  /* Use a local helper to avoid the cellFormatter indirection */
  switch (col) {
  case COL_JOBNAME:
    strncpy(bufA, (*ja)->jobName, sizeof(bufA)-1);
    strncpy(bufB, (*jb)->jobName, sizeof(bufB)-1);
    break;
  case COL_JOBID:
    strncpy(bufA, (*ja)->jobId, sizeof(bufA)-1);
    strncpy(bufB, (*jb)->jobId, sizeof(bufB)-1);
    break;
  case COL_OWNER:
    strncpy(bufA, (*ja)->owner, sizeof(bufA)-1);
    strncpy(bufB, (*jb)->owner, sizeof(bufB)-1);
    break;
  case COL_PRTY:
    snprintf(bufA, sizeof(bufA), "%08d", (*ja)->priority);
    snprintf(bufB, sizeof(bufB), "%08d", (*jb)->priority);
    break;
  case COL_QUEUE:
    strncpy(bufA, phaseToQueue((*ja)->phase), sizeof(bufA)-1);
    strncpy(bufB, phaseToQueue((*jb)->phase), sizeof(bufB)-1);
    break;
  case COL_STATUS:
    strncpy(bufA, phaseName((*ja)->phase), sizeof(bufA)-1);
    strncpy(bufB, phaseName((*jb)->phase), sizeof(bufB)-1);
    break;
  case COL_MAXRC:
    formatMaxRC(*ja, bufA, sizeof(bufA));
    formatMaxRC(*jb, bufB, sizeof(bufB));
    break;
  default:
    /* For other columns, use string compare on formatted value.
       Safe fallback: we can't easily call cellFormatter from qsort. */
    strncpy(bufA, (*ja)->jobName, sizeof(bufA)-1);
    strncpy(bufB, (*jb)->jobName, sizeof(bufB)-1);
    break;
  }

  int cmp = strcmp(bufA, bufB);
  return gTui->sortAscending ? cmp : -cmp;
}

static void stSortHandler(int col, int ascending, void *userData) {
  STData *data = (STData *)userData;
  if (data->jobCount > 1 && data->jobArray) {
    qsort(data->jobArray, data->jobCount, sizeof(JobInfo *), stSortCompare);
  }
}

/* ----------------------------------------------------------------
   ST select handler: drill down into sysout
   ---------------------------------------------------------------- */

static int stSelectHandler(int row, void *userData) {
  STData *data = (STData *)userData;
  showJobSysout(data, row);
  return 0;  /* continue ST event loop after drill-down returns */
}

/* ----------------------------------------------------------------
   Command handler
   ---------------------------------------------------------------- */

static int stCommandHandler(const char *command, void *userData) {
  if (command[0] == 'q' || command[0] == 'Q') {
    return 1;  /* quit */
  }
  /* Could add: SORT, FILTER, PREFIX, OWNER, etc. */
  return 0;
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  STData data;
  memset(&data, 0, sizeof(data));

  /* Parse command-line args */
  data.filter.detailLevel = JOB_DETAIL_TERSE;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-name") && i + 1 < argc) {
      data.filter.flags |= JOB_SELECT_BY_NAME;
      strncpy(data.filter.jobName, argv[++i], 8);
    } else if (!strcmp(argv[i], "-owner") && i + 1 < argc) {
      data.filter.flags |= JOB_SELECT_BY_OWNER;
      strncpy(data.filter.owner, argv[++i], 8);
    } else if (!strcmp(argv[i], "-stc")) {
      data.filter.flags |= JOB_SELECT_STC;
    } else if (!strcmp(argv[i], "-tsu")) {
      data.filter.flags |= JOB_SELECT_TSU;
    } else if (!strcmp(argv[i], "-job")) {
      data.filter.flags |= JOB_SELECT_JOB;
    } else if (!strcmp(argv[i], "-max") && i + 1 < argc) {
      data.filter.maxJobs = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "-h")) {
      printf("Usage: zst [options]\n");
      printf("  -name <pattern>   Filter by job name\n");
      printf("  -owner <userid>   Filter by owner\n");
      printf("  -stc              Show started tasks only\n");
      printf("  -tsu              Show TSO users only\n");
      printf("  -job              Show batch jobs only\n");
      printf("  -max <n>          Limit number of jobs\n");
      printf("  -help             Show this help\n");
      return 0;
    }
  }

  /* Initialize job service */
  int rc = jobServiceInit(&data.service);
  if (rc != 0) {
    fprintf(stderr, "jobServiceInit failed rc=%d\n", rc);
    return 1;
  }

  /* Initial job fetch */
  rc = jobServiceGetJobs(data.service, &data.filter,
                         &data.jobList, &data.jobCount);
  if (rc != 0) {
    fprintf(stderr, "jobServiceGetJobs failed rc=%d\n", rc);
    jobServiceTerm(data.service);
    return 1;
  }
  buildJobArray(&data);

  /* Set up TUI */
  TuiTable tui;
  memset(&tui, 0, sizeof(tui));

  tuiSetColumns(&tui, stColumns, COL_COUNT, 4);  /* NP,JOBNAME,JobID,Owner are fixed */
  tuiSetTitle(&tui, "ZST STATUS DISPLAY ALL CLASSES");
  tui.rowCount = data.jobCount;
  tui.cellFormatter = stCellFormatter;
  tui.commandHandler = stCommandHandler;
  tui.refreshHandler = stRefreshHandler;
  tui.selectHandler = stSelectHandler;
  tui.filterHandler = stFilterHandler;
  tui.sortHandler = stSortHandler;
  tui.userData = &data;
  gTui = &tui;

  /* Enter TUI */
  tuiInit(&tui);
  tuiEventLoop(&tui);
  tuiTerm(&tui);

  /* Cleanup */
  if (data.jobList) {
    jobServiceFreeJobs(data.service, data.jobList);
  }
  jobServiceTerm(data.service);
  if (data.jobArray) {
    free(data.jobArray);
  }

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
