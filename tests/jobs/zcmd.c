

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  zcmd — MVS command console.

  Split-screen TUI: scrollable command response history on top,
  command input at bottom.  Issues commands via __console2() EMCS console.

  Usage:
    zcmd [initial-command]
      e.g. zcmd "D A,L"
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "tui.h"
#include "mvscmd.h"

/* ----------------------------------------------------------------
   Response history — scrollable text buffer
   ---------------------------------------------------------------- */

#define MAX_HISTORY_LINES 10000

typedef struct CmdData_tag {
  MvsConsole  *console;
  char       **histLines;     /* ring buffer of history lines */
  int          histCount;
  int          histMax;
  int          scrollRow;     /* top of visible window */
  int          screenRows;
  int          screenCols;
  char         cmdBuf[256];
  int          cmdPos;
} CmdData;

static void addHistoryLine(CmdData *data, const char *line) {
  if (data->histCount >= data->histMax) {
    /* Shift down (discard oldest) */
    free(data->histLines[0]);
    memmove(&data->histLines[0], &data->histLines[1],
            (data->histMax - 1) * sizeof(char *));
    data->histCount = data->histMax - 1;
  }
  data->histLines[data->histCount] = strdup(line);
  data->histCount++;
}

static void addSeparator(CmdData *data) {
  addHistoryLine(data, "------------------------------------------------------------");
}

static void issueAndCollect(CmdData *data, const char *command) {
  /* Show the command in history */
  char cmdLine[300];
  snprintf(cmdLine, sizeof(cmdLine), "==> %s", command);
  addHistoryLine(data, cmdLine);

  MvsCmdResponse *responses = NULL;
  int responseCount = 0;
  int rc = mvsCmdIssue(data->console, command, 3000,
                       &responses, &responseCount);

  if (rc != 0) {
    char errLine[80];
    snprintf(errLine, sizeof(errLine), "*** Command failed rc=%d", rc);
    addHistoryLine(data, errLine);
  } else if (responseCount == 0) {
    addHistoryLine(data, "(no response received)");
  } else {
    for (MvsCmdResponse *r = responses; r; r = r->next) {
      addHistoryLine(data, r->text);
    }
    mvsCmdFreeResponses(responses);
  }
  addSeparator(data);

  /* Auto-scroll to bottom */
  int dataRows = data->screenRows - 4;  /* title + cmd + status + header */
  if (data->histCount > dataRows) {
    data->scrollRow = data->histCount - dataRows;
  }
}

/* ----------------------------------------------------------------
   Rendering
   ---------------------------------------------------------------- */

#define ESC "\x27"  /* EBCDIC ESC = 0x27 */

static void zcmd_write(const char *s) {
  write(STDOUT_FILENO, s, strlen(s));
}

static void zcmd_printf(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n > 0) write(STDOUT_FILENO, buf, n);
}

static void zcmd_moveTo(int row, int col) {
  zcmd_printf(ESC "[%d;%dH", row, col);
}

static void zcmd_clearLine(void) {
  zcmd_write(ESC "[2K");
}

#define SGR_RESET   ESC "[0m"
#define SGR_BOLD    ESC "[1m"
#define SGR_REVERSE ESC "[7m"
#define FG_CYAN     ESC "[36m"
#define FG_GREEN    ESC "[32m"
#define FG_YELLOW   ESC "[33m"
#define FG_WHITE    ESC "[37m"

static void renderScreen(CmdData *data) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    data->screenRows = ws.ws_row;
    data->screenCols = ws.ws_col;
  }

  int rows = data->screenRows;
  int cols = data->screenCols;

  /* Line 1: title bar */
  zcmd_moveTo(1, 1);
  zcmd_write(SGR_REVERSE);
  zcmd_printf("%-*s", cols, " ZCMD - MVS COMMAND CONSOLE");
  zcmd_write(SGR_RESET);

  /* Lines 2 to rows-2: history */
  int histAreaStart = 2;
  int histAreaEnd = rows - 2;
  int histAreaRows = histAreaEnd - histAreaStart + 1;

  for (int i = 0; i < histAreaRows; i++) {
    int lineIdx = data->scrollRow + i;
    zcmd_moveTo(histAreaStart + i, 1);
    zcmd_clearLine();
    if (lineIdx >= 0 && lineIdx < data->histCount) {
      char *line = data->histLines[lineIdx];
      /* Color command lines differently */
      if (line[0] == '=' && line[1] == '=' && line[2] == '>') {
        zcmd_write(FG_GREEN SGR_BOLD);
      } else if (line[0] == '*') {
        zcmd_write(FG_YELLOW);
      } else if (line[0] == '-' && line[1] == '-') {
        zcmd_write(FG_CYAN);
      }
      int len = strlen(line);
      if (len > cols) len = cols;
      write(STDOUT_FILENO, line, len);
      zcmd_write(SGR_RESET);
    }
  }

  /* Line rows-1: command input */
  zcmd_moveTo(rows - 1, 1);
  zcmd_clearLine();
  zcmd_write(SGR_BOLD FG_WHITE);
  zcmd_printf("COMMAND ===> ");
  zcmd_write(SGR_RESET);
  zcmd_printf("%s", data->cmdBuf);

  /* Line rows: status */
  zcmd_moveTo(rows, 1);
  zcmd_write(SGR_REVERSE);
  zcmd_printf("%-*s", cols, "");
  zcmd_moveTo(rows, 1);
  zcmd_printf(" Lines: %d  F3=Exit  F7=Up  F8=Down  PgUp/PgDn  Enter=Submit",
              data->histCount);
  zcmd_write(SGR_RESET);

  /* Position cursor at command input */
  zcmd_moveTo(rows - 1, 14 + data->cmdPos);
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  CmdData data;
  memset(&data, 0, sizeof(data));
  data.histMax = MAX_HISTORY_LINES;
  data.histLines = (char **)calloc(data.histMax, sizeof(char *));
  if (!data.histLines) {
    fprintf(stderr, "Out of memory\n");
    return 1;
  }

  /* Open console */
  int rc = mvsCmdOpen(&data.console, NULL);
  if (rc != 0) {
    fprintf(stderr, "mvsCmdOpen failed rc=%d\n", rc);
    free(data.histLines);
    return 1;
  }

  addHistoryLine(&data, "ZCMD MVS Command Console");
  addHistoryLine(&data, "Type a command and press Enter. F3 to exit.");
  addSeparator(&data);

  /* If initial command given on command line */
  if (argc > 1) {
    char fullCmd[256] = "";
    for (int i = 1; i < argc; i++) {
      if (i > 1) strcat(fullCmd, " ");
      strncat(fullCmd, argv[i], sizeof(fullCmd) - strlen(fullCmd) - 1);
    }
    issueAndCollect(&data, fullCmd);
  }

  /* Enter raw mode + alt screen (reuse TUI init) */
  TuiTable dummyTui;
  memset(&dummyTui, 0, sizeof(dummyTui));
  tuiInit(&dummyTui);

  /* Get screen size */
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    data.screenRows = ws.ws_row;
    data.screenCols = ws.ws_col;
  } else {
    data.screenRows = 24;
    data.screenCols = 80;
  }

  renderScreen(&data);

  /* Event loop using tuiReadKey */
  int running = 1;
  while (running) {
    /* We use the TUI's tuiReadKey indirectly by reading stdin directly.
       Since tuiInit set raw mode, we can read key-by-key.
       For simplicity, we reuse the same escape parsing approach. */
    int ch = tuiReadKey();

    if (ch == TUI_KEY_F3 || ch == TUI_KEY_ESCAPE) {
      running = 0;
      break;
    }

    int histAreaRows = data.screenRows - 4;

    if (ch == TUI_KEY_PGUP || ch == TUI_KEY_F7) {
      data.scrollRow -= histAreaRows;
      if (data.scrollRow < 0) data.scrollRow = 0;
      renderScreen(&data);
      continue;
    }
    if (ch == TUI_KEY_PGDN || ch == TUI_KEY_F8) {
      data.scrollRow += histAreaRows;
      int maxScroll = data.histCount - histAreaRows;
      if (maxScroll < 0) maxScroll = 0;
      if (data.scrollRow > maxScroll) data.scrollRow = maxScroll;
      renderScreen(&data);
      continue;
    }
    if (ch == TUI_KEY_UP) {
      if (data.scrollRow > 0) data.scrollRow--;
      renderScreen(&data);
      continue;
    }
    if (ch == TUI_KEY_DOWN) {
      int maxScroll = data.histCount - histAreaRows;
      if (maxScroll < 0) maxScroll = 0;
      if (data.scrollRow < maxScroll) data.scrollRow++;
      renderScreen(&data);
      continue;
    }

    if (ch == TUI_KEY_ENTER) {
      if (data.cmdBuf[0]) {
        /* Check for quit */
        if (data.cmdBuf[0] == 'q' || data.cmdBuf[0] == 'Q') {
          if (data.cmdPos == 1 || !strcmp(data.cmdBuf, "quit") ||
              !strcmp(data.cmdBuf, "QUIT")) {
            running = 0;
            break;
          }
        }
        issueAndCollect(&data, data.cmdBuf);
        data.cmdBuf[0] = '\0';
        data.cmdPos = 0;
      }
      renderScreen(&data);
      continue;
    }

    if (ch == TUI_KEY_BACKSPACE) {
      if (data.cmdPos > 0) {
        data.cmdPos--;
        data.cmdBuf[data.cmdPos] = '\0';
        renderScreen(&data);
      }
      continue;
    }

    /* Printable character */
    if (ch >= 0x20 && ch < 0x100 && ch != 0x7F) {
      if (data.cmdPos < 254) {
        data.cmdBuf[data.cmdPos++] = (char)ch;
        data.cmdBuf[data.cmdPos] = '\0';
        renderScreen(&data);
      }
      continue;
    }

    if (ch == TUI_KEY_RESIZE) {
      renderScreen(&data);
      continue;
    }
  }

  tuiTerm(&dummyTui);

  /* Cleanup */
  mvsCmdClose(data.console);
  for (int i = 0; i < data.histCount; i++) {
    free(data.histLines[i]);
  }
  free(data.histLines);

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
