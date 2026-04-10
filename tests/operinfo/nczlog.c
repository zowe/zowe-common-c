

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  nczlog — SYSLOG/OPERLOG browser.
            ncurses-based version using nctui framework.

  Reads SYSLOG as a JES spool dataset via SAPI (SSI 79),
  then displays it in a scrollable text viewer.

  Usage:
    nczlog [options]
      -tail             Start at end of log (default: start)
      -max <n>          Max records to read (default 50000)
      -filter <text>    Only show lines containing <text>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "jobservice.h"
#include "nctui.h"

/* ----------------------------------------------------------------
   Record collector
   ---------------------------------------------------------------- */

#define MAX_LOG_LINES 100000

typedef struct LogCollector_tag {
  char **lines;
  int    lineCount;
  int    maxLines;
  char   filter[64];   /* optional text filter */
} LogCollector;

static int logRecordHandler(const char *record, int recordLen,
                            int lineNumber, void *userData) {
  LogCollector *coll = (LogCollector *)userData;
  if (coll->lineCount >= coll->maxLines) return 1;

  /* Apply text filter if set */
  if (coll->filter[0]) {
    /* Simple case-insensitive substring search */
    int found = 0;
    int filterLen = strlen(coll->filter);
    for (int i = 0; i <= recordLen - filterLen; i++) {
      int match = 1;
      for (int j = 0; j < filterLen; j++) {
        char rc = record[i + j];
        char fc = coll->filter[j];
        /* Uppercase both for case-insensitive */
        if (rc >= 'a' && rc <= 'z') rc -= 32;
        if (fc >= 'a' && fc <= 'z') fc -= 32;
        if (rc != fc) { match = 0; break; }
      }
      if (match) { found = 1; break; }
    }
    if (!found) return 0;  /* skip this line, continue reading */
  }

  char *line = (char *)malloc(recordLen + 1);
  if (!line) return 1;
  memcpy(line, record, recordLen);
  line[recordLen] = '\0';

  /* Trim trailing non-printable EBCDIC and spaces (all <= 0x40) */
  int end = recordLen - 1;
  while (end >= 0 && (unsigned char)line[end] <= 0x40) line[end--] = '\0';

  coll->lines[coll->lineCount++] = line;
  return 0;
}

static void freeLogLines(LogCollector *coll) {
  for (int i = 0; i < coll->lineCount; i++) {
    free(coll->lines[i]);
  }
  free(coll->lines);
  coll->lines = NULL;
  coll->lineCount = 0;
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  int startAtEnd = 0;
  int maxRecords = 50000;
  char filter[64] = "";

  /* Parse args */
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-tail")) {
      startAtEnd = 1;
    } else if (!strcmp(argv[i], "-max") && i + 1 < argc) {
      maxRecords = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "-filter") && i + 1 < argc) {
      strncpy(filter, argv[++i], 63);
      filter[63] = '\0';
    } else if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "-h")) {
      printf("Usage: nczlog [options]\n");
      printf("  -tail             Start at end of log\n");
      printf("  -max <n>          Max records (default 50000)\n");
      printf("  -filter <text>    Text filter\n");
      printf("  -help             Show this help\n");
      return 0;
    }
  }

  /* Initialize job service */
  JobService *svc = NULL;
  int rc = jobServiceInit(&svc);
  if (rc != 0) {
    fprintf(stderr, "jobServiceInit failed rc=%d\n", rc);
    return 1;
  }

  printf("Reading SYSLOG via SAPI...\n");
  fflush(stdout);

  /* Collect records using the SAPI-direct read method.
     SYSLOG is a system job with jobname=SYSLOG. */
  LogCollector coll;
  memset(&coll, 0, sizeof(coll));
  coll.maxLines = maxRecords;
  if (coll.maxLines > MAX_LOG_LINES) coll.maxLines = MAX_LOG_LINES;
  coll.lines = (char **)calloc(coll.maxLines, sizeof(char *));
  if (!coll.lines) {
    fprintf(stderr, "Out of memory\n");
    jobServiceTerm(svc);
    return 1;
  }
  if (filter[0]) {
    strncpy(coll.filter, filter, 63);
  }

  rc = jobServiceSAPIRead(svc, NULL, "SYSLOG", maxRecords,
                          logRecordHandler, &coll);

  if (coll.lineCount == 0) {
    fprintf(stderr, "No SYSLOG records read (rc=%d).\n", rc);
    fprintf(stderr, "SYSLOG access via SAPI may require appropriate authority.\n");
    freeLogLines(&coll);
    jobServiceTerm(svc);
    return 1;
  }

  printf("Read %d lines. Launching viewer...\n", coll.lineCount);
  fflush(stdout);

  /* Set up navigator and push text view page */
  NcTuiNav nav;
  nctuiNavInit(&nav);

  NcTuiPage page;
  nctuiPageInitTextView(&page, "SYSLOG", coll.lines, coll.lineCount);
  if (filter[0]) {
    snprintf(page.textView.title, sizeof(page.textView.title),
             "SYSLOG [filter: %s] (%d lines)", filter, coll.lineCount);
  } else {
    snprintf(page.textView.title, sizeof(page.textView.title),
             "SYSLOG (%d lines)", coll.lineCount);
  }
  if (startAtEnd) {
    page.textView.scrollRow = coll.lineCount;  /* will be clamped */
  }
  nctuiNavPush(&nav, &page);
  nctuiNavRun(&nav);
  nctuiNavTerm(&nav);

  /* Cleanup */
  freeLogLines(&coll);
  jobServiceTerm(svc);

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
