


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  zdisp — MVS DISPLAY command browser.

  Full-screen TUI that presents a catalog of MVS D commands.
  Select a command to pre-fill the command line, then submit
  to execute it via the zdisplay library.  Results are shown
  in a scrollable text viewer.

  Usage:
    zdisp              Interactive catalog browser
    zdisp D IPLINFO    Execute command directly and show result
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "zowetypes.h"
#include "zdisplay.h"
#include "tui.h"

/* ----------------------------------------------------------------
   Data model
   ---------------------------------------------------------------- */

typedef struct DispData_tag {
  const ZDispCommand *catalog;
  int                 catalogCount;
} DispData;

/* ----------------------------------------------------------------
   Catalog table columns
   ---------------------------------------------------------------- */

enum {
  COL_NUM = 0,
  COL_STATUS,
  COL_SYNTAX,
  COL_DESCRIPTION,
  COL_COUNT
};

static TuiColumn catalogColumns[] = {
  { "#",      3,  TUI_ALIGN_RIGHT },
  { "Impl",   4,  TUI_ALIGN_LEFT  },
  { "Command", 18, TUI_ALIGN_LEFT },
  { "Description", 52, TUI_ALIGN_LEFT },
};

/* ----------------------------------------------------------------
   Cell formatter for catalog table
   ---------------------------------------------------------------- */

static void catalogCellFormatter(int row, int col, char *buf, int bufLen,
                                 void *userData) {
  DispData *data = (DispData *)userData;
  if (row < 0 || row >= data->catalogCount) {
    buf[0] = '\0';
    return;
  }
  const ZDispCommand *cmd = &data->catalog[row];

  switch (col) {
  case COL_NUM:
    snprintf(buf, bufLen, "%d", row + 1);
    break;
  case COL_STATUS:
    snprintf(buf, bufLen, "%s", cmd->implemented ? "Yes" : " - ");
    break;
  case COL_SYNTAX:
    snprintf(buf, bufLen, "%s", cmd->syntax);
    break;
  case COL_DESCRIPTION:
    snprintf(buf, bufLen, "%s", cmd->description);
    break;
  default:
    buf[0] = '\0';
    break;
  }
}

/* ----------------------------------------------------------------
   Show command result in text viewer
   ---------------------------------------------------------------- */

static void showResult(const char *cmdText, ZDispResult *result) {
  if (!result || result->lineCount == 0) return;

  TuiTextView tv;
  memset(&tv, 0, sizeof(tv));
  snprintf(tv.title, sizeof(tv.title), "%.127s", cmdText);
  tv.lines = result->lines;
  tv.lineCount = result->lineCount;
  tuiTextViewShow(&tv);
}

/* ----------------------------------------------------------------
   Execute a command and display results
   ---------------------------------------------------------------- */

static void executeAndShow(TuiTable *tui, const char *cmdText) {
  ZDispResult *result = zdispExecute(cmdText);
  if (!result) {
    tuiSetStatus(tui, "ERROR: zdispExecute returned NULL");
    return;
  }

  if (result->lineCount > 0) {
    showResult(cmdText, result);
  } else {
    tuiSetStatus(tui, "No output for: %s", cmdText);
  }

  zdispFreeResult(result);
}

/* ----------------------------------------------------------------
   Show HELP — list all commands in the text viewer
   ---------------------------------------------------------------- */

static void showHelp(DispData *data) {
  /* Build help text as an array of lines */
  int maxLines = data->catalogCount + 10;
  char **lines = (char **)calloc(maxLines, sizeof(char *));
  if (!lines) return;
  int n = 0;

  char buf[256];

  lines[n++] = strdup(" ZDISPLAY COMMAND REFERENCE");
  lines[n++] = strdup("");
  snprintf(buf, sizeof(buf), " %-18s %-4s  %s", "COMMAND", "IMPL", "DESCRIPTION");
  lines[n++] = strdup(buf);
  snprintf(buf, sizeof(buf), " %-18s %-4s  %s", "-------", "----", "-----------");
  lines[n++] = strdup(buf);

  for (int i = 0; i < data->catalogCount && n < maxLines; i++) {
    const ZDispCommand *cmd = &data->catalog[i];
    snprintf(buf, sizeof(buf), " %-18s %-4s  %s",
             cmd->syntax,
             cmd->implemented ? "Yes" : " - ",
             cmd->description);
    lines[n++] = strdup(buf);
  }

  lines[n++] = strdup("");
  lines[n++] = strdup(" Select a command and press Enter, or type a command directly.");
  lines[n++] = strdup(" F3 = Back/Quit    F5 = Refresh");

  TuiTextView tv;
  memset(&tv, 0, sizeof(tv));
  snprintf(tv.title, sizeof(tv.title), "ZDISPLAY HELP");
  tv.lines = lines;
  tv.lineCount = n;
  tuiTextViewShow(&tv);

  for (int i = 0; i < n; i++) free(lines[i]);
  free(lines);
}

/* ----------------------------------------------------------------
   Command handler
   ---------------------------------------------------------------- */

static int catalogCommandHandler(const char *command, void *userData) {
  DispData *data = (DispData *)userData;

  if (command[0] == 'Q' || command[0] == 'q') {
    return 1;  /* quit */
  }

  if (strcasecmp(command, "HELP") == 0 || strcmp(command, "?") == 0) {
    showHelp(data);
    return 0;
  }

  /* Anything starting with "D " or "DISPLAY " is a command to execute.
     We return a special code to signal the event loop to handle it. */
  if ((command[0] == 'D' || command[0] == 'd') &&
      (command[1] == ' ' || command[1] == '\0')) {
    /* Will be handled after event loop returns — store in TUI's cmdBuf
       which is already there. Use return code 2 as a signal. */
    return 2;
  }

  /* Check for "DISPLAY " prefix too */
  if (strncasecmp(command, "DISPLAY ", 8) == 0) {
    return 2;
  }

  return 0;
}

/* ----------------------------------------------------------------
   Select handler: pre-fill command line with selected command
   ---------------------------------------------------------------- */

static TuiTable *gTui = NULL;  /* for select handler to access tui */

static int catalogSelectHandler(int row, void *userData) {
  DispData *data = (DispData *)userData;
  if (row < 0 || row >= data->catalogCount) return 0;

  const ZDispCommand *cmd = &data->catalog[row];

  /* Pre-fill the command line with the command syntax */
  if (gTui) {
    strncpy(gTui->cmdBuf, cmd->syntax, sizeof(gTui->cmdBuf) - 1);
    gTui->cmdPos = strlen(gTui->cmdBuf);
    gTui->cmdActive = 1;

    /* Show argument help in status bar if available */
    if (cmd->argHelp) {
      tuiSetStatus(gTui, "Args: %s", cmd->argHelp);
    } else if (!cmd->implemented) {
      tuiSetStatus(gTui, "Not yet implemented");
    } else {
      tuiSetStatus(gTui, "Press Enter to execute");
    }
  }

  return 0;  /* stay in event loop */
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  DispData data;
  memset(&data, 0, sizeof(data));

  data.catalog = zdispGetCatalog(&data.catalogCount);

  /* If command-line args given, execute directly (non-interactive) */
  if (argc > 1) {
    /* Reconstruct command string from argv */
    char cmdText[256];
    memset(cmdText, 0, sizeof(cmdText));
    for (int i = 1; i < argc; i++) {
      if (i > 1) strcat(cmdText, " ");
      strncat(cmdText, argv[i], sizeof(cmdText) - strlen(cmdText) - 2);
    }

    /* If it doesn't start with D or DISPLAY, prepend D */
    if (cmdText[0] != 'D' && cmdText[0] != 'd') {
      char tmp[256];
      snprintf(tmp, sizeof(tmp), "D %s", cmdText);
      strcpy(cmdText, tmp);
    }

    ZDispResult *result = zdispExecute(cmdText);
    if (result) {
      for (int i = 0; i < result->lineCount; i++) {
        printf("%s\n", result->lines[i]);
      }
      int rc = result->rc;
      zdispFreeResult(result);
      return rc;
    }
    fprintf(stderr, "zdispExecute failed\n");
    return 1;
  }

  /* Interactive mode — set up TUI */
  TuiTable tui;
  memset(&tui, 0, sizeof(tui));
  gTui = &tui;

  tuiSetColumns(&tui, catalogColumns, COL_COUNT, 2);  /* #, Impl are fixed */
  tuiSetTitle(&tui, "ZDISP MVS DISPLAY COMMANDS   (HELP for reference, Q to quit)");
  tui.rowCount = data.catalogCount;
  tui.cellFormatter = catalogCellFormatter;
  tui.commandHandler = catalogCommandHandler;
  tui.selectHandler = catalogSelectHandler;
  tui.userData = &data;

  tuiInit(&tui);

  tuiSetStatus(&tui, "Select a command or type D <command>.  HELP=reference  Q=quit");

  /* Main loop: tuiEventLoop returns 0 on F3, or whatever the command/select
     handler returned.  rc==2 means "execute the command in cmdBuf".
     Anything else (0=F3, 1=Q) exits. */
  while (1) {
    int rc = tuiEventLoop(&tui);

    if (rc != 2) {
      break;  /* F3 or Q */
    }

    /* Execute the command currently in cmdBuf */
    char cmdText[256];
    strncpy(cmdText, tui.cmdBuf, sizeof(cmdText) - 1);
    cmdText[sizeof(cmdText) - 1] = '\0';

    if (cmdText[0]) {
      executeAndShow(&tui, cmdText);
      /* After text viewer returns, clear command and re-show catalog */
      tui.cmdBuf[0] = '\0';
      tui.cmdPos = 0;
    }
    tuiSetStatus(&tui, "Select a command or type D <command>.  HELP=reference  Q=quit");
  }

  tuiTerm(&tui);
  gTui = NULL;

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
