


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  nczdisp — MVS DISPLAY command browser.
             ncurses-based version using nctui framework.

  Full-screen TUI that presents a catalog of MVS D commands.
  Select a command to pre-fill the command line, then submit
  to execute it via the zdisplay library.  Results are shown
  in a scrollable text viewer.

  Usage:
    nczdisp              Interactive catalog browser
    nczdisp D IPLINFO    Execute command directly and show result
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "zowetypes.h"
#include "zdisplay.h"
#include "nctui.h"

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

static NcTuiColumn catalogColumns[] = {
  { "#",      3,  NCTUI_ALIGN_RIGHT },
  { "Impl",   4,  NCTUI_ALIGN_LEFT  },
  { "Command", 18, NCTUI_ALIGN_LEFT },
  { "Description", 52, NCTUI_ALIGN_LEFT },
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
   Globals for navigator-based text view (lines must survive while
   the text view page is on the stack)
   ---------------------------------------------------------------- */

static NcTuiTable *gTui = NULL;
static ZDispResult *gLastResult = NULL;   /* kept alive until next execute */
static char       **gHelpLines = NULL;    /* kept alive until cleanup */
static int          gHelpLineCount = 0;

static void freeLastResult(void) {
  if (gLastResult) {
    zdispFreeResult(gLastResult);
    gLastResult = NULL;
  }
}

static void freeHelpLines(void) {
  for (int i = 0; i < gHelpLineCount; i++) free(gHelpLines[i]);
  free(gHelpLines);
  gHelpLines = NULL;
  gHelpLineCount = 0;
}

/* ----------------------------------------------------------------
   Execute a command and push result as text view page
   ---------------------------------------------------------------- */

static void executeAndShow(NcTuiTable *tui, const char *cmdText) {
  freeLastResult();  /* free previous result */

  ZDispResult *result = zdispExecute(cmdText);
  if (!result) {
    nctuiSetStatus(tui, "ERROR: zdispExecute returned NULL");
    return;
  }

  if (result->lineCount > 0 && gTui && gTui->nav) {
    gLastResult = result;  /* keep alive while text view is showing */
    NcTuiPage tvPage;
    nctuiPageInitTextView(&tvPage, "Result",
                          result->lines, result->lineCount);
    snprintf(tvPage.textView.title, sizeof(tvPage.textView.title),
             "%.127s", cmdText);
    nctuiNavPush(gTui->nav, &tvPage);
  } else {
    nctuiSetStatus(tui, "No output for: %s", cmdText);
    zdispFreeResult(result);
  }
}

/* ----------------------------------------------------------------
   Show HELP -- list all commands in the text viewer
   ---------------------------------------------------------------- */

static void showHelp(DispData *data) {
  if (!gTui || !gTui->nav) return;

  /* Build help text once and keep alive */
  freeHelpLines();

  int maxLines = data->catalogCount + 10;
  gHelpLines = (char **)calloc(maxLines, sizeof(char *));
  if (!gHelpLines) return;
  int n = 0;

  char buf[256];

  gHelpLines[n++] = strdup(" ZDISPLAY COMMAND REFERENCE");
  gHelpLines[n++] = strdup("");
  snprintf(buf, sizeof(buf), " %-18s %-4s  %s", "COMMAND", "IMPL", "DESCRIPTION");
  gHelpLines[n++] = strdup(buf);
  snprintf(buf, sizeof(buf), " %-18s %-4s  %s", "-------", "----", "-----------");
  gHelpLines[n++] = strdup(buf);

  for (int i = 0; i < data->catalogCount && n < maxLines; i++) {
    const ZDispCommand *cmd = &data->catalog[i];
    snprintf(buf, sizeof(buf), " %-18s %-4s  %s",
             cmd->syntax,
             cmd->implemented ? "Yes" : " - ",
             cmd->description);
    gHelpLines[n++] = strdup(buf);
  }

  gHelpLines[n++] = strdup("");
  gHelpLines[n++] = strdup(" Select a command and press Enter, or type a command directly.");
  gHelpLines[n++] = strdup(" F3 = Back/Quit    F5 = Refresh");
  gHelpLineCount = n;

  NcTuiPage tvPage;
  nctuiPageInitTextView(&tvPage, "Help", gHelpLines, n);
  snprintf(tvPage.textView.title, sizeof(tvPage.textView.title),
           "ZDISPLAY HELP");
  nctuiNavPush(gTui->nav, &tvPage);
}

/* ----------------------------------------------------------------
   Filter handler
   ---------------------------------------------------------------- */

static void catalogFilterHandler(void *userData) {
  /* Catalog is static -- nothing to rebuild.
     The TUI framework handles filter matching via cellFormatter. */
}

/* ----------------------------------------------------------------
   Sort handler: qsort the catalog -- not applicable for static catalog
   ---------------------------------------------------------------- */

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

  /* Anything starting with "D " or "DISPLAY " is a command to execute */
  if (((command[0] == 'D' || command[0] == 'd') &&
       (command[1] == ' ' || command[1] == '\0')) ||
      strncasecmp(command, "DISPLAY ", 8) == 0) {
    if (gTui) {
      executeAndShow(gTui, command);
    }
    return 0;
  }

  return 0;
}

/* ----------------------------------------------------------------
   Select handler: pre-fill command line with selected command
   ---------------------------------------------------------------- */

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
      nctuiSetStatus(gTui, "Args: %s", cmd->argHelp);
    } else if (!cmd->implemented) {
      nctuiSetStatus(gTui, "Not yet implemented");
    } else {
      nctuiSetStatus(gTui, "Press Enter to execute");
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

  /* Interactive mode -- set up navigator */
  NcTuiNav nav;
  nctuiNavInit(&nav);

  NcTuiPage catPage;
  nctuiPageInitTable(&catPage, "Catalog");
  nctuiSetColumns(&catPage.table, catalogColumns, COL_COUNT, 2);
  nctuiSetTitle(&catPage.table,
                "ZDISP MVS DISPLAY COMMANDS   (HELP for reference, Q to quit)");
  catPage.table.rowCount = data.catalogCount;
  catPage.table.cellFormatter = catalogCellFormatter;
  catPage.table.commandHandler = catalogCommandHandler;
  catPage.table.selectHandler = catalogSelectHandler;
  catPage.table.filterHandler = catalogFilterHandler;
  catPage.table.userData = &data;

  nctuiNavPush(&nav, &catPage);
  gTui = &nav.stack[0].table;

  nctuiSetStatus(gTui, "Select a command or type D <command>.  HELP=reference  Q=quit");

  nctuiNavRun(&nav);
  nctuiNavTerm(&nav);

  /* Cleanup */
  freeLastResult();
  freeHelpLines();
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
