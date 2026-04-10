

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  nctso — TSO Command Session.
           ncurses-based TUI using nctui framework.

  Collapsible command history:  each command is a row with +/- toggle,
  response lines shown underneath when expanded.

  Usage:
    nctso
      Type TSO commands in the command line and press Enter.
      Use +/- to expand/collapse output.
      NP actions: S=toggle, R=rerun, E=edit (load to command line).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nctui.h"
#include "tsopipe.h"

/* z/OS EBCDIC-to-ASCII conversion (program is compiled EBCDIC,
   but popen needs ASCII command strings in the hybrid ncurses env) */
#pragma runopts("FILETAG(AUTOCVT,AUTOTAG)")
int __etoa_l(char *buf, int len);
int __atoe_l(char *buf, int len);

/* ----------------------------------------------------------------
   Data model
   ---------------------------------------------------------------- */

#define MAX_ENTRIES 4096

typedef struct TsoEntry_tag {
  char   command[TSOPIPE_MAX_CMD];
  char **responseLines;       /* owned — freed when entry is cleared */
  int    responseCount;
  int    rc;
  int    collapsed;
} TsoEntry;

typedef struct TsoData_tag {
  TsoEntry    entries[MAX_ENTRIES];
  int         entryCount;
  NcTuiTable *tui;
} TsoData;

static TsoData gData;
static NcTuiTable *gTui = NULL;

/* ----------------------------------------------------------------
   Column definitions
   ---------------------------------------------------------------- */

enum {
  COL_NP = 0,
  COL_FOLD,
  COL_TEXT,
  COL_COUNT
};

static NcTuiColumn tsoColumns[] = {
  { "NP",  4,   NCTUI_ALIGN_LEFT },
  { " ",   3,   NCTUI_ALIGN_LEFT },
  { "Command / Response", 132, NCTUI_ALIGN_LEFT },
};

/* ----------------------------------------------------------------
   Row mapping: flat display row <-> (entry, line offset)
   lineIdx == -1 means the command row itself.
   ---------------------------------------------------------------- */

static void findEntryForRow(TsoData *d, int row, int *entryIdx, int *lineIdx) {
  int r = 0;
  for (int i = 0; i < d->entryCount; i++) {
    if (r == row) { *entryIdx = i; *lineIdx = -1; return; }
    r++;
    if (!d->entries[i].collapsed) {
      for (int j = 0; j < d->entries[i].responseCount; j++) {
        if (r == row) { *entryIdx = i; *lineIdx = j; return; }
        r++;
      }
    }
  }
  *entryIdx = -1;
  *lineIdx = -1;
}

static int computeRowCount(TsoData *d) {
  int count = 0;
  for (int i = 0; i < d->entryCount; i++) {
    count++;
    if (!d->entries[i].collapsed)
      count += d->entries[i].responseCount;
  }
  return count;
}

static int rowForEntry(TsoData *d, int entryIdx) {
  int r = 0;
  for (int i = 0; i < entryIdx; i++) {
    r++;
    if (!d->entries[i].collapsed)
      r += d->entries[i].responseCount;
  }
  return r;
}

/* ----------------------------------------------------------------
   Cell formatter
   ---------------------------------------------------------------- */

static void tsoCellFormatter(int row, int col, char *buf, int bufLen,
                             void *userData) {
  TsoData *d = (TsoData *)userData;
  int entryIdx, lineIdx;

  findEntryForRow(d, row, &entryIdx, &lineIdx);
  if (entryIdx < 0) { buf[0] = '\0'; return; }

  TsoEntry *entry = &d->entries[entryIdx];

  switch (col) {
  case COL_NP:
    buf[0] = '\0';
    break;

  case COL_FOLD:
    if (lineIdx == -1) {
      buf[0] = entry->collapsed ? '+' : '-';
      buf[1] = '\0';
    } else {
      buf[0] = '\0';
    }
    break;

  case COL_TEXT:
    if (lineIdx == -1) {
      /* Command row — show RC if nonzero */
      if (entry->rc != 0)
        snprintf(buf, bufLen, "%s  RC=%d", entry->command, entry->rc);
      else
        snprintf(buf, bufLen, "%s", entry->command);
    } else {
      /* Response line — indented */
      if (lineIdx < entry->responseCount && entry->responseLines[lineIdx])
        snprintf(buf, bufLen, "  %s", entry->responseLines[lineIdx]);
      else
        buf[0] = '\0';
    }
    break;

  default:
    buf[0] = '\0';
    break;
  }
}

/* ----------------------------------------------------------------
   Execute a TSO command and add to history
   ---------------------------------------------------------------- */

static void executeCommand(TsoData *d, const char *command) {
  if (d->entryCount >= MAX_ENTRIES) {
    if (d->tui)
      nctuiSetStatus(d->tui, "History full (%d entries)", MAX_ENTRIES);
    return;
  }

  TsoEntry *entry = &d->entries[d->entryCount];
  memset(entry, 0, sizeof(*entry));
  strncpy(entry->command, command, sizeof(entry->command) - 1);
  entry->collapsed = 0;

  if (d->tui)
    nctuiSetStatus(d->tui, "Running: %s ...", command);

  TsoPipeResult result;
  if (tsoPipeExec(command, &result) == 0) {
    entry->responseLines = result.lines;
    entry->responseCount = result.lineCount;
    entry->rc = result.rc;
    /* Ownership of lines transferred — don't call tsoPipeFreeResult */
    if (result.truncated && d->tui)
      nctuiSetStatus(d->tui, "Output truncated at %d lines", TSOPIPE_MAX_LINES);
  } else {
    entry->rc = -1;
  }

  d->entryCount++;
  d->tui->rowCount = computeRowCount(d);

  /* Scroll to show the new entry */
  int targetRow = rowForEntry(d, d->entryCount - 1);
  d->tui->selectedRow = targetRow;
  d->tui->cursorRow = targetRow;
  if (targetRow > d->tui->dataRows - 1)
    d->tui->scrollRow = targetRow;

  if (d->tui)
    nctuiSetStatus(d->tui, "");
}

/* ----------------------------------------------------------------
   NP action: toggle expand/collapse
   ---------------------------------------------------------------- */

static int actionToggle(int row, void *userData, NcTuiNav *nav) {
  TsoData *d = (TsoData *)userData;
  int entryIdx, lineIdx;

  findEntryForRow(d, row, &entryIdx, &lineIdx);
  if (entryIdx < 0) return 0;

  d->entries[entryIdx].collapsed = !d->entries[entryIdx].collapsed;
  d->tui->rowCount = computeRowCount(d);
  return 0;
}

/* ----------------------------------------------------------------
   NP action: rerun command
   ---------------------------------------------------------------- */

static int actionRerun(int row, void *userData, NcTuiNav *nav) {
  TsoData *d = (TsoData *)userData;
  int entryIdx, lineIdx;

  findEntryForRow(d, row, &entryIdx, &lineIdx);
  if (entryIdx < 0) return 0;

  executeCommand(d, d->entries[entryIdx].command);
  return 0;
}

/* ----------------------------------------------------------------
   NP action: edit (load command to command line)
   ---------------------------------------------------------------- */

static int actionEdit(int row, void *userData, NcTuiNav *nav) {
  TsoData *d = (TsoData *)userData;
  int entryIdx, lineIdx;

  findEntryForRow(d, row, &entryIdx, &lineIdx);
  if (entryIdx < 0) return 0;

  strncpy(d->tui->cmdBuf, d->entries[entryIdx].command,
          sizeof(d->tui->cmdBuf) - 1);
  d->tui->cmdPos = strlen(d->tui->cmdBuf);
  d->tui->cmdActive = 1;

  return 0;
}

/* ----------------------------------------------------------------
   Command handler — execute TSO commands typed at the command line
   ---------------------------------------------------------------- */

static int tsoCommandHandler(const char *command, void *userData) {
  TsoData *d = (TsoData *)userData;

  if (command[0] == '\0') return 0;

  if (command[0] == 'Q' || command[0] == 'q') {
    if (strlen(command) == 1 ||
        strcmp(command, "QUIT") == 0 || strcmp(command, "quit") == 0)
      return 1;
  }

  if (strcmp(command, "COLLAPSE") == 0 || strcmp(command, "collapse") == 0) {
    for (int i = 0; i < d->entryCount; i++)
      d->entries[i].collapsed = 1;
    d->tui->rowCount = computeRowCount(d);
    return 0;
  }

  if (strcmp(command, "EXPAND") == 0 || strcmp(command, "expand") == 0) {
    for (int i = 0; i < d->entryCount; i++)
      d->entries[i].collapsed = 0;
    d->tui->rowCount = computeRowCount(d);
    return 0;
  }

  if (strcmp(command, "CLEAR") == 0 || strcmp(command, "clear") == 0) {
    for (int i = 0; i < d->entryCount; i++) {
      TsoEntry *e = &d->entries[i];
      if (e->responseLines) {
        for (int j = 0; j < e->responseCount; j++)
          free(e->responseLines[j]);
        free(e->responseLines);
      }
    }
    d->entryCount = 0;
    d->tui->rowCount = 0;
    d->tui->scrollRow = 0;
    d->tui->selectedRow = -1;
    return 0;
  }

  /* Everything else is a TSO command */
  executeCommand(d, command);
  return 0;
}

/* ----------------------------------------------------------------
   Refresh handler (F5) — just recompute row count
   ---------------------------------------------------------------- */

static int tsoRefreshHandler(void *userData) {
  TsoData *d = (TsoData *)userData;
  return computeRowCount(d);
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  memset(&gData, 0, sizeof(gData));

  /* Set up navigator */
  NcTuiNav nav;
  nctuiNavInit(&nav);

  NcTuiPage tsoPage;
  nctuiPageInitTable(&tsoPage, "TSO");
  nctuiSetColumns(&tsoPage.table, tsoColumns, COL_COUNT, 3);
  nctuiSetTitle(&tsoPage.table, "ZTSO - TSO COMMAND SESSION");
  tsoPage.table.rowCount = 0;
  tsoPage.table.cellFormatter = tsoCellFormatter;
  tsoPage.table.commandHandler = tsoCommandHandler;
  tsoPage.table.refreshHandler = tsoRefreshHandler;
  tsoPage.table.userData = &gData;

  /* NP actions */
  nctuiAddAction(&tsoPage.table, 'S', "Toggle +/-",
                 NCTUI_ACTION_DEFAULT, actionToggle);
  nctuiAddAction(&tsoPage.table, 'R', "Rerun", 0, actionRerun);
  nctuiAddAction(&tsoPage.table, 'E', "Edit", 0, actionEdit);

  nctuiNavPush(&nav, &tsoPage);
  gTui = &nav.stack[0].table;
  gData.tui = gTui;

  nctuiNavRun(&nav);
  nctuiNavTerm(&nav);

  /* Cleanup */
  for (int i = 0; i < gData.entryCount; i++) {
    TsoEntry *e = &gData.entries[i];
    if (e->responseLines) {
      for (int j = 0; j < e->responseCount; j++)
        free(e->responseLines[j]);
      free(e->responseLines);
    }
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
