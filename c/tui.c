

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  TUI framework using VT100/ANSI escape sequences and termios.
  No ncurses dependency — works directly with any VT100-compatible terminal.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "tui.h"

/* Debug key logging */
#include <fcntl.h>
static int tui_debugFd = -1;

static void tui_debugOpen(void) {
  tui_debugFd = open("/tmp/tui_keys.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (tui_debugFd >= 0) {
    char msg[] = "=== tui debug start ===\n";
    write(tui_debugFd, msg, sizeof(msg) - 1);
  }
}

static void tui_debugClose(void) {
  if (tui_debugFd >= 0) { close(tui_debugFd); tui_debugFd = -1; }
}

static void tui_debugByte(const char *label, int b) {
  if (tui_debugFd >= 0) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%s 0x%02X (%d)\n", label, b & 0xFF, b);
    if (n > 0) write(tui_debugFd, buf, n);
  }
}

/* ----------------------------------------------------------------
   ANSI escape sequence helpers
   ---------------------------------------------------------------- */

#define ESC "\x27"  /* EBCDIC ESC = 0x27 */

static void tui_write(const char *s) {
  write(STDOUT_FILENO, s, strlen(s));
}

static void tui_printf(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n > 0) write(STDOUT_FILENO, buf, n);
}

static void tui_moveTo(int row, int col) {
  tui_printf(ESC "[%d;%dH", row, col);
}

static void tui_clearScreen(void) {
  tui_write(ESC "[2J");
}

static void tui_clearLine(void) {
  tui_write(ESC "[2K");
}

static void tui_enterAltScreen(void) {
  tui_write(ESC "[?1049h");
}

static void tui_leaveAltScreen(void) {
  tui_write(ESC "[?1049l");
}

static void tui_showCursor(void) {
  tui_write(ESC "[?25h");
}

static void tui_hideCursor(void) {
  tui_write(ESC "[?25l");
}

/* SGR attributes */
#define SGR_RESET    ESC "[0m"
#define SGR_BOLD     ESC "[1m"
#define SGR_REVERSE  ESC "[7m"

/* Foreground colors */
#define FG_WHITE     ESC "[37m"
#define FG_YELLOW    ESC "[33m"
#define FG_CYAN      ESC "[36m"
#define FG_BLACK     ESC "[30m"
#define FG_GREEN     ESC "[32m"
#define FG_BLUE      ESC "[34m"
#define SGR_DIM      ESC "[2m"

/* Background colors */
#define BG_BLUE      ESC "[44m"
#define BG_BLACK     ESC "[40m"
#define BG_CYAN      ESC "[46m"
#define BG_WHITE     ESC "[47m"

/* Mouse tracking */
static void tui_enableMouse(void) {
  tui_write(ESC "[?1000h");  /* X10 basic mouse */
  tui_write(ESC "[?1006h");  /* SGR extended mouse */
}

static void tui_disableMouse(void) {
  tui_write(ESC "[?1006l");
  tui_write(ESC "[?1000l");
}

/* ----------------------------------------------------------------
   Terminal size
   ---------------------------------------------------------------- */

static void tui_getSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
    *rows = ws.ws_row;
    *cols = ws.ws_col;
  } else {
    *rows = 24;
    *cols = 80;
  }
}

/* ----------------------------------------------------------------
   Raw terminal mode
   ---------------------------------------------------------------- */

static struct termios tui_origTermios;

static void tui_rawMode(void) {
  struct termios raw;
  tcgetattr(STDIN_FILENO, &tui_origTermios);
  raw = tui_origTermios;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void tui_restoreMode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tui_origTermios);
}

/* ----------------------------------------------------------------
   Key reading with escape sequence decoding
   ---------------------------------------------------------------- */

static int tui_readByte(void) {
  unsigned char c;
  int n = read(STDIN_FILENO, &c, 1);
  if (n <= 0) return -1;
  tui_debugByte("read", (int)c);
  return (int)c;
}

/*
  Parse a decimal number from the input stream.
  Reads digits, stops at non-digit, returns the number and
  stores the terminator in *term.
*/
static int tui_readNum(int firstCh, int *term) {
  int num = 0;
  int ch = firstCh;
  /* Handle EBCDIC digits (0xF0-0xF9) and ASCII digits (0x30-0x39) */
  while ((ch >= '0' && ch <= '9') || (ch >= 0xF0 && ch <= 0xF9)
         || (ch >= 0x30 && ch <= 0x39)) {
    if (ch >= 0xF0)
      num = num * 10 + (ch - 0xF0);
    else if (ch >= 0x30 && ch <= 0x39)
      num = num * 10 + (ch - 0x30);
    else
      num = num * 10 + (ch - '0');
    ch = tui_readByte();
    if (ch < 0) break;
  }
  *term = ch;
  return num;
}

static int tuiReadKeyInternal(TuiTable *tui) {
  int c = tui_readByte();
  if (c < 0) return -1;

  /* ESC: EBCDIC=0x27, ASCII=0x1B — handle both since SSH pty
     conversion behavior varies */
  if (c == 0x27 || c == 0x1B) {
    int c2 = tui_readByte();
    if (c2 < 0) return TUI_KEY_ESCAPE;

    /* CSI sequence: ESC [ ...  (EBCDIC '[' = 0xAD, ASCII '[' = 0x5B) */
    if (c2 == '[' || c2 == 0xAD || c2 == 0x5B) {
      int c3 = tui_readByte();
      if (c3 < 0) return TUI_KEY_ESCAPE;

      /* Backtab: ESC [ Z  (EBCDIC Z=0xE9, ASCII Z=0x5A) */
      if (c3 == 'Z' || c3 == 0xE9 || c3 == 0x5A) return TUI_KEY_BTAB;

      /* Arrow keys: ESC [ A/B/C/D  (handle both EBCDIC and ASCII codes) */
      if (c3 == 'A' || c3 == 0xC1 || c3 == 0x41) return TUI_KEY_UP;
      if (c3 == 'B' || c3 == 0xC2 || c3 == 0x42) return TUI_KEY_DOWN;
      if (c3 == 'C' || c3 == 0xC3 || c3 == 0x43) return TUI_KEY_RIGHT;
      if (c3 == 'D' || c3 == 0xC4 || c3 == 0x44) return TUI_KEY_LEFT;
      if (c3 == 'H' || c3 == 0xC8 || c3 == 0x48) return TUI_KEY_HOME;
      if (c3 == 'F' || c3 == 0xC6 || c3 == 0x46) return TUI_KEY_END;

      /* SGR mouse: ESC [ < btn ; col ; row M/m */
      if (c3 == '<' || c3 == 0x4C || c3 == 0x3C) {  /* EBCDIC '<' = 0x4C, ASCII '<' = 0x3C */
        int term;
        int btn = tui_readNum(tui_readByte(), &term);
        /* term should be ';' */
        int col = tui_readNum(tui_readByte(), &term);
        int row = tui_readNum(tui_readByte(), &term);
        /* term = 'M' (press) or 'm' (release) — EBCDIC M=0xD4, ASCII M=0x4D */
        int isPress = (term == 'M' || term == 0xD4 || term == 0x4D);
        if (isPress && btn == 0) {  /* button 1 press */
          if (tui) {
            tui->mouseButton = btn;
            tui->mouseRow = row;
            tui->mouseCol = col;
          }
          return TUI_KEY_MOUSE;
        }
        /* scroll wheel: btn 64 = up, 65 = down */
        if (btn == 64) return TUI_KEY_PGUP;
        if (btn == 65) return TUI_KEY_PGDN;
        return -1;  /* ignore releases and other buttons */
      }

      /* Numeric sequences: ESC [ n ~ (ASCII '0'-'9' = 0x30-0x39) */
      if ((c3 >= '0' && c3 <= '9') || (c3 >= 0xF0 && c3 <= 0xF9)
          || (c3 >= 0x30 && c3 <= 0x39)) {
        int term;
        int num = tui_readNum(c3, &term);
        if (term == '~' || term == 0xA1 || term == 0x7E) {  /* ASCII '~' = 0x7E */
          switch (num) {
          case 5:  return TUI_KEY_PGUP;
          case 6:  return TUI_KEY_PGDN;
          case 1:  return TUI_KEY_HOME;
          case 4:  return TUI_KEY_END;
          case 13: return TUI_KEY_F3;
          case 15: return TUI_KEY_F5;
          case 17: return TUI_KEY_F6;
          case 18: return TUI_KEY_F7;
          case 19: return TUI_KEY_F8;
          }
        }
        return TUI_KEY_ESCAPE;
      }

      /* PF keys: ESC [ O P/Q/R/S for F1-F4 (ASCII O=0x4F) */
      if (c3 == 'O' || c3 == 0xD6 || c3 == 0x4F) {
        int c4 = tui_readByte();
        if (c4 == 'R' || c4 == 0xD9 || c4 == 0x52) return TUI_KEY_F3;
        return TUI_KEY_ESCAPE;
      }
    }

    /* ESC O sequences (some terminals)  ASCII O=0x4F */
    if (c2 == 'O' || c2 == 0xD6 || c2 == 0x4F) {
      int c3 = tui_readByte();
      if (c3 == 'R' || c3 == 0xD9 || c3 == 0x52) return TUI_KEY_F3;
      if (c3 == 'A' || c3 == 0xC1 || c3 == 0x41) return TUI_KEY_UP;
      if (c3 == 'B' || c3 == 0xC2 || c3 == 0x42) return TUI_KEY_DOWN;
      if (c3 == 'C' || c3 == 0xC3 || c3 == 0x43) return TUI_KEY_RIGHT;
      if (c3 == 'D' || c3 == 0xC4 || c3 == 0x44) return TUI_KEY_LEFT;
      return TUI_KEY_ESCAPE;
    }

    return TUI_KEY_ESCAPE;
  }

  /* Tab: EBCDIC=0x05, ASCII=0x09 */
  if (c == 0x05 || c == 0x09) {
    return TUI_KEY_TAB;
  }

  /* Enter: EBCDIC CR=0x0D, NL=0x15/0x25; ASCII CR=0x0D, LF=0x0A */
  if (c == 0x0D || c == 0x0A || c == 0x15 || c == 0x25 || c == '\n' || c == '\r') {
    return TUI_KEY_ENTER;
  }

  /* Backspace: EBCDIC=0x16, ASCII=0x08/0x7F */
  if (c == 0x16 || c == 0x08 || c == 127) {
    return TUI_KEY_BACKSPACE;
  }

  tui_debugByte("unhandled", c);
  return c;
}

/* Public version for external callers (zcmd, etc.) */
int tuiReadKey(void) {
  return tuiReadKeyInternal(NULL);
}

/* ----------------------------------------------------------------
   Screen layout constants
   ---------------------------------------------------------------- */
#define TITLE_ROW    1
#define CMD_ROW      2
#define HEADER_ROW   3
#define FILTER_ROW   4
/* status bar at screenRows */

static int tuiDataStart(TuiTable *tui) {
  return tui->filterActive ? 5 : 4;
}

/* Map a screen x-coordinate to a column index.
   Returns -1 if no column at that position. */
static int hitTestColumn(TuiTable *tui, int screenX) {
  int xPos = 1;
  /* Fixed columns first */
  for (int c = 0; c < tui->fixedColumns && c < tui->numColumns; c++) {
    int w = tui->columns[c].width;
    if (screenX >= xPos && screenX < xPos + w) return c;
    xPos += w + 1;
  }
  /* Scrollable columns */
  for (int c = tui->scrollCol; c < tui->numColumns; c++) {
    int w = tui->columns[c].width;
    if (xPos + w > tui->screenCols) break;
    if (screenX >= xPos && screenX < xPos + w) return c;
    xPos += w + 1;
  }
  return -1;
}

/* Get the screen x-position of a given column. Returns 1-based position. */
static int computeColumnX(TuiTable *tui, int targetCol) {
  int xPos = 1;
  for (int c = 0; c < tui->fixedColumns && c < tui->numColumns; c++) {
    if (c == targetCol) return xPos;
    xPos += tui->columns[c].width + 1;
  }
  for (int c = tui->scrollCol; c < tui->numColumns; c++) {
    if (c == targetCol) return xPos;
    xPos += tui->columns[c].width + 1;
  }
  return xPos;
}

/* ----------------------------------------------------------------
   Init / term
   ---------------------------------------------------------------- */

int tuiInit(TuiTable *tui) {
  tui_debugOpen();
  tui_rawMode();
  tui->origTermValid = 1;
  tui_enterAltScreen();
  tui_hideCursor();
  tui_enableMouse();
  tui_clearScreen();
  tui->selectedRow = -1;
  tui->sortColumn = -1;
  tui->sortAscending = 1;
  tui->filterActive = 0;
  tui->filterEditCol = -1;
  tui->filterEditPos = 0;

  tui_getSize(&tui->screenRows, &tui->screenCols);
  tui->dataRows = tui->screenRows - tuiDataStart(tui);  /* minus status bar */
  if (tui->dataRows < 1) tui->dataRows = 1;

  tui->scrollRow = 0;
  tui->scrollCol = tui->fixedColumns;
  tui->cmdBuf[0] = '\0';
  tui->cmdPos = 0;
  tui->cmdActive = 1;
  tui->statusMsg[0] = '\0';
  tui->useColor = 1;  /* assume VT100 color support */

  return 0;
}

void tuiTerm(TuiTable *tui) {
  tui_debugClose();
  tui_disableMouse();
  tui_showCursor();
  tui_write(SGR_RESET);
  tui_leaveAltScreen();
  if (tui->origTermValid) {
    tui_restoreMode();
    tui->origTermValid = 0;
  }
}

/* ----------------------------------------------------------------
   Column setup
   ---------------------------------------------------------------- */

void tuiSetColumns(TuiTable *tui, TuiColumn *cols, int numCols, int fixedCols) {
  tui->numColumns = numCols;
  tui->fixedColumns = fixedCols;
  for (int i = 0; i < numCols && i < TUI_MAX_COLUMNS; i++) {
    tui->columns[i] = cols[i];
  }
  tui->scrollCol = fixedCols;
}

void tuiSetTitle(TuiTable *tui, const char *title) {
  strncpy(tui->title, title, sizeof(tui->title) - 1);
  tui->title[sizeof(tui->title) - 1] = '\0';
}

void tuiSetStatus(TuiTable *tui, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tui->statusMsg, sizeof(tui->statusMsg), fmt, ap);
  va_end(ap);
}

/* ----------------------------------------------------------------
   Rendering
   ---------------------------------------------------------------- */

static void renderBar(TuiTable *tui, int row, const char *left,
                      const char *right, const char *sgr) {
  tui_moveTo(row, 1);
  tui_write(sgr);
  /* fill entire row with spaces first */
  tui_printf("%-*s", tui->screenCols, "");
  tui_moveTo(row, 1);
  tui_printf(" %s", left);
  if (right && right[0]) {
    int rlen = (int)strlen(right);
    int rpos = tui->screenCols - rlen;
    if (rpos > 0) {
      tui_moveTo(row, rpos);
      tui_printf("%s", right);
    }
  }
  tui_write(SGR_RESET);
}

static void renderTitleBar(TuiTable *tui) {
  char right[80];
  int lastVisible = tui->scrollRow + tui->dataRows;
  if (lastVisible > tui->rowCount) lastVisible = tui->rowCount;
  snprintf(right, sizeof(right), "LINE %d-%d (%d) ",
           tui->rowCount > 0 ? tui->scrollRow + 1 : 0,
           lastVisible, tui->rowCount);
  renderBar(tui, TITLE_ROW, tui->title, right,
            SGR_BOLD FG_WHITE BG_BLUE);
}

static void renderCommandLine(TuiTable *tui) {
  tui_moveTo(CMD_ROW, 1);
  tui_write(SGR_RESET);
  tui_clearLine();
  tui_printf("COMMAND ===> %s", tui->cmdBuf);
}

static int renderColumnRange(TuiTable *tui, int row,
                             int startCol, int endCol, int xPos,
                             int isHeader, int dataRow) {
  char buf[TUI_MAX_COL_WIDTH + 1];

  for (int c = startCol; c < endCol && c < tui->numColumns; c++) {
    TuiColumn *col = &tui->columns[c];
    int w = col->width;

    if (xPos + w > tui->screenCols) {
      w = tui->screenCols - xPos;
      if (w <= 0) break;
    }

    if (isHeader) {
      if (tui && c == tui->sortColumn) {
        char display[TUI_MAX_COL_WIDTH + 1];
        snprintf(display, sizeof(display), "%s%s", col->name,
                 tui->sortAscending ? "^" : "v");
        tui_write(SGR_BOLD FG_WHITE BG_CYAN);
        snprintf(buf, sizeof(buf), "%-*.*s", w, w, display);
      } else {
        snprintf(buf, sizeof(buf), "%-*.*s", w, w, col->name);
      }
    } else {
      buf[0] = '\0';
      if (tui->cellFormatter) {
        tui->cellFormatter(dataRow, c, buf, sizeof(buf), tui->userData);
      }
      if (col->align == TUI_ALIGN_RIGHT) {
        int slen = (int)strlen(buf);
        if (slen < w) {
          char tmp[TUI_MAX_COL_WIDTH + 1];
          snprintf(tmp, sizeof(tmp), "%*s", w, buf);
          strncpy(buf, tmp, sizeof(buf) - 1);
          buf[sizeof(buf) - 1] = '\0';
        }
      }
    }

    int slen = (int)strlen(buf);
    if (slen < w) {
      memset(buf + slen, ' ', w - slen);
    }
    buf[w] = '\0';

    tui_moveTo(row, xPos);
    tui_printf("%s", buf);
    /* Reset back to header style after sort-highlighted column */
    if (isHeader && tui && c == tui->sortColumn) {
      tui_write(SGR_BOLD FG_YELLOW);
    }
    xPos += w + 1;
  }
  return xPos;
}

static void renderFilterField(TuiTable *tui, int col, int xPos) {
  int w = tui->columns[col].width;
  int hasFilter = (tui->columns[col].filter[0] != '\0');
  int isEditing = (col == tui->filterEditCol);

  tui_moveTo(FILTER_ROW, xPos);

  if (isEditing) {
    /* Active edit field: reverse video, green on white */
    tui_write(SGR_REVERSE FG_GREEN);
    tui_printf("%-*.*s", w, w, tui->columns[col].filter);
    tui_write(SGR_RESET);
  } else if (hasFilter) {
    /* Has a filter value: bright green */
    tui_write(SGR_BOLD FG_GREEN);
    tui_printf("%-*.*s", w, w, tui->columns[col].filter);
    tui_write(SGR_RESET);
  } else {
    /* Empty: show dots as placeholder so fields are visible */
    tui_write(SGR_DIM FG_CYAN);
    char dots[TUI_MAX_COL_WIDTH + 1];
    memset(dots, '.', w);
    dots[w] = '\0';
    tui_printf("%s", dots);
    tui_write(SGR_RESET);
  }
}

static void renderFilterRow(TuiTable *tui) {
  if (!tui->filterActive) return;

  tui_moveTo(FILTER_ROW, 1);
  tui_write(SGR_RESET);
  tui_clearLine();

  int xPos = 1;
  /* Fixed columns */
  for (int c = 0; c < tui->fixedColumns && c < tui->numColumns; c++) {
    if (xPos + tui->columns[c].width > tui->screenCols) break;
    renderFilterField(tui, c, xPos);
    xPos += tui->columns[c].width + 1;
  }
  /* Scrollable columns */
  for (int c = tui->scrollCol; c < tui->numColumns; c++) {
    if (xPos + tui->columns[c].width > tui->screenCols) break;
    renderFilterField(tui, c, xPos);
    xPos += tui->columns[c].width + 1;
  }
}

static void renderHeaders(TuiTable *tui) {
  tui_moveTo(HEADER_ROW, 1);
  tui_write(SGR_BOLD FG_YELLOW);
  tui_clearLine();

  int xPos = renderColumnRange(tui, HEADER_ROW, 0, tui->fixedColumns, 1,
                               1, -1);
  renderColumnRange(tui, HEADER_ROW, tui->scrollCol, tui->numColumns, xPos,
                    1, -1);
  tui_write(SGR_RESET);
}

static void renderDataRows(TuiTable *tui) {
  int dataStart = tuiDataStart(tui);
  for (int i = 0; i < tui->dataRows; i++) {
    int dataRow = tui->scrollRow + i;
    int screenRow = dataStart + i;

    tui_moveTo(screenRow, 1);
    tui_write(SGR_RESET);
    tui_clearLine();

    if (dataRow >= tui->rowCount) {
      continue;
    }

    /* highlight selected row */
    if (dataRow == tui->selectedRow) {
      tui_write(SGR_REVERSE);
      tui_printf("%-*s", tui->screenCols, "");
      tui_moveTo(screenRow, 1);
      tui_write(SGR_REVERSE);  /* keep reverse on for cell content */
    }

    int xPos = renderColumnRange(tui, screenRow, 0, tui->fixedColumns, 1,
                                 0, dataRow);
    renderColumnRange(tui, screenRow, tui->scrollCol, tui->numColumns, xPos,
                      0, dataRow);

    if (dataRow == tui->selectedRow) {
      tui_write(SGR_RESET);
    }
  }
}

static void renderStatusBar(TuiTable *tui) {
  char left[256];
  if (tui->statusMsg[0]) {
    snprintf(left, sizeof(left), "%s", tui->statusMsg);
  } else if (tui->filterActive && tui->filterEditCol >= 0) {
    snprintf(left, sizeof(left),
             "%d rows  |  Wildcards: * ?  |  Tab=Next  Enter=Apply  Esc=Done  F6=Close",
             tui->rowCount);
  } else {
    snprintf(left, sizeof(left),
             "%d rows  |  F3=Exit  F5=Refresh  F6=Filter  PgUp/PgDn  Click header=Sort",
             tui->rowCount);
  }
  renderBar(tui, tui->screenRows, left, NULL,
            SGR_BOLD FG_WHITE BG_BLUE);
}

/* ----------------------------------------------------------------
   Full render
   ---------------------------------------------------------------- */

void tuiRender(TuiTable *tui) {
  tui_getSize(&tui->screenRows, &tui->screenCols);
  tui->dataRows = tui->screenRows - tuiDataStart(tui);
  if (tui->dataRows < 1) tui->dataRows = 1;

  tui_hideCursor();
  renderTitleBar(tui);
  renderCommandLine(tui);
  renderHeaders(tui);
  renderFilterRow(tui);
  renderDataRows(tui);
  renderStatusBar(tui);

  /* position cursor: filter field if editing, else command line */
  tui_showCursor();
  if (tui->filterActive && tui->filterEditCol >= 0) {
    int fx = computeColumnX(tui, tui->filterEditCol) + tui->filterEditPos;
    tui_moveTo(FILTER_ROW, fx);
  } else {
    tui_moveTo(CMD_ROW, 14 + tui->cmdPos);
  }
}

/* ----------------------------------------------------------------
   Event loop
   ---------------------------------------------------------------- */

int tuiEventLoop(TuiTable *tui) {
  tuiRender(tui);

  while (1) {
    int ch = tuiReadKeyInternal(tui);
    if (ch < 0) continue;

    if (ch == TUI_KEY_F3) {
      return 0;
    }

    /* Mouse click */
    if (ch == TUI_KEY_MOUSE) {
      /* Click on header row — toggle sort */
      if (tui->mouseRow == HEADER_ROW) {
        int col = hitTestColumn(tui, tui->mouseCol);
        if (col >= 0) {
          if (col == tui->sortColumn) {
            tui->sortAscending = !tui->sortAscending;
          } else {
            tui->sortColumn = col;
            tui->sortAscending = 1;
          }
          if (tui->sortHandler) {
            tui->sortHandler(tui->sortColumn, tui->sortAscending, tui->userData);
          }
        }
        tuiRender(tui);
        continue;
      }
      /* Click on filter row — select filter field */
      if (tui->filterActive && tui->mouseRow == FILTER_ROW) {
        int col = hitTestColumn(tui, tui->mouseCol);
        if (col >= 0) {
          tui->filterEditCol = col;
          tui->filterEditPos = (int)strlen(tui->columns[col].filter);
        }
        tuiRender(tui);
        continue;
      }
      /* Click on data row — select */
      int dataStart = tuiDataStart(tui);
      int clickedRow = tui->mouseRow - dataStart;  /* terminal rows are 1-based */
      int dataRow = tui->scrollRow + clickedRow;
      if (clickedRow >= 0 && dataRow >= 0 && dataRow < tui->rowCount) {
        tui->selectedRow = dataRow;
        if (tui->selectHandler) {
          int rc = tui->selectHandler(dataRow, tui->userData);
          if (rc != 0) return rc;
        }
      }
      tuiRender(tui);
      continue;
    }

    if (ch == TUI_KEY_F5) {
      if (tui->refreshHandler) {
        tui->rowCount = tui->refreshHandler(tui->userData);
      }
      tui->statusMsg[0] = '\0';
      tuiRender(tui);
      continue;
    }

    /* F6: toggle filter row */
    if (ch == TUI_KEY_F6) {
      tui->filterActive = !tui->filterActive;
      if (tui->filterActive) {
        tui->filterEditCol = 0;
        tui->filterEditPos = (int)strlen(tui->columns[0].filter);
      } else {
        tui->filterEditCol = -1;
      }
      tui->dataRows = tui->screenRows - tuiDataStart(tui);
      if (tui->dataRows < 1) tui->dataRows = 1;
      tuiRender(tui);
      continue;
    }

    /* Filter field editing — intercept keys when a filter field is active */
    if (tui->filterActive && tui->filterEditCol >= 0) {
      if (ch == TUI_KEY_TAB) {
        tui->filterEditCol++;
        if (tui->filterEditCol >= tui->numColumns) {
          tui->filterEditCol = -1;  /* back to command line */
        } else {
          tui->filterEditPos = (int)strlen(tui->columns[tui->filterEditCol].filter);
        }
        tuiRender(tui);
        continue;
      }
      if (ch == TUI_KEY_BTAB) {
        tui->filterEditCol--;
        if (tui->filterEditCol < 0) {
          tui->filterEditCol = -1;  /* back to command line */
        } else {
          tui->filterEditPos = (int)strlen(tui->columns[tui->filterEditCol].filter);
        }
        tuiRender(tui);
        continue;
      }
      if (ch == TUI_KEY_ENTER) {
        /* Commit — leave filter field, go to command line */
        tui->filterEditCol = -1;
        if (tui->filterHandler) {
          tui->filterHandler(tui->userData);
        }
        tuiRender(tui);
        continue;
      }
      if (ch == TUI_KEY_ESCAPE) {
        tui->filterEditCol = -1;  /* deselect filter field */
        tuiRender(tui);
        continue;
      }
      if (ch == TUI_KEY_BACKSPACE) {
        char *f = tui->columns[tui->filterEditCol].filter;
        if (tui->filterEditPos > 0) {
          f[--tui->filterEditPos] = '\0';
          if (tui->filterHandler) {
            tui->filterHandler(tui->userData);
          }
        }
        tuiRender(tui);
        continue;
      }
      /* Printable character to filter field */
      if (ch >= 0x40 && ch <= 0xFE) {
        char *f = tui->columns[tui->filterEditCol].filter;
        if (tui->filterEditPos < TUI_MAX_COL_WIDTH - 1) {
          f[tui->filterEditPos++] = (char)ch;
          f[tui->filterEditPos] = '\0';
          if (tui->filterHandler) {
            tui->filterHandler(tui->userData);
          }
        }
        tuiRender(tui);
        continue;
      }
    }

    if (ch == TUI_KEY_UP) {
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->scrollRow;
      } else if (tui->selectedRow > 0) {
        tui->selectedRow--;
      }
      /* auto-scroll to keep selection visible */
      if (tui->selectedRow < tui->scrollRow) {
        tui->scrollRow = tui->selectedRow;
      }
      tuiRender(tui);
      continue;
    }
    if (ch == TUI_KEY_DOWN) {
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->scrollRow;
      } else if (tui->selectedRow < tui->rowCount - 1) {
        tui->selectedRow++;
      }
      /* auto-scroll to keep selection visible */
      if (tui->selectedRow >= tui->scrollRow + tui->dataRows) {
        tui->scrollRow = tui->selectedRow - tui->dataRows + 1;
      }
      tuiRender(tui);
      continue;
    }
    if (ch == TUI_KEY_PGUP || ch == TUI_KEY_F7) {
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->scrollRow;
      }
      tui->selectedRow -= tui->dataRows;
      if (tui->selectedRow < 0) tui->selectedRow = 0;
      tui->scrollRow = tui->selectedRow;
      tuiRender(tui);
      continue;
    }
    if (ch == TUI_KEY_PGDN || ch == TUI_KEY_F8) {
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->scrollRow;
      }
      tui->selectedRow += tui->dataRows;
      if (tui->selectedRow >= tui->rowCount) {
        tui->selectedRow = tui->rowCount - 1;
      }
      tui->scrollRow = tui->selectedRow;
      if (tui->scrollRow + tui->dataRows > tui->rowCount) {
        tui->scrollRow = tui->rowCount - tui->dataRows;
      }
      if (tui->scrollRow < 0) tui->scrollRow = 0;
      tuiRender(tui);
      continue;
    }

    if (ch == TUI_KEY_RIGHT) {
      if (tui->scrollCol + 1 < tui->numColumns) {
        tui->scrollCol++;
      }
      tuiRender(tui);
      continue;
    }
    if (ch == TUI_KEY_LEFT) {
      if (tui->scrollCol > tui->fixedColumns) {
        tui->scrollCol--;
      }
      tuiRender(tui);
      continue;
    }

    if (ch == TUI_KEY_HOME) {
      tui->selectedRow = 0;
      tui->scrollRow = 0;
      tui->scrollCol = tui->fixedColumns;
      tuiRender(tui);
      continue;
    }
    if (ch == TUI_KEY_END) {
      tui->selectedRow = tui->rowCount - 1;
      if (tui->selectedRow < 0) tui->selectedRow = 0;
      tui->scrollRow = tui->rowCount - tui->dataRows;
      if (tui->scrollRow < 0) tui->scrollRow = 0;
      tuiRender(tui);
      continue;
    }

    /* Tab: cycle selection down through rows, wrap to command line */
    if (ch == TUI_KEY_TAB) {
      if (tui->selectedRow < 0) {
        /* from command line → first visible row */
        tui->selectedRow = tui->scrollRow;
      } else if (tui->selectedRow < tui->rowCount - 1) {
        tui->selectedRow++;
        /* auto-scroll */
        if (tui->selectedRow >= tui->scrollRow + tui->dataRows) {
          tui->scrollRow = tui->selectedRow - tui->dataRows + 1;
        }
      } else {
        /* past last row → back to command line */
        tui->selectedRow = -1;
      }
      tuiRender(tui);
      continue;
    }

    /* Backtab (Shift-Tab): cycle selection up, wrap to command line */
    if (ch == TUI_KEY_BTAB) {
      if (tui->selectedRow < 0) {
        /* from command line → last row */
        tui->selectedRow = tui->rowCount - 1;
        if (tui->selectedRow < 0) tui->selectedRow = 0;
        tui->scrollRow = tui->rowCount - tui->dataRows;
        if (tui->scrollRow < 0) tui->scrollRow = 0;
      } else if (tui->selectedRow > 0) {
        tui->selectedRow--;
        if (tui->selectedRow < tui->scrollRow) {
          tui->scrollRow = tui->selectedRow;
        }
      } else {
        /* before first row → back to command line */
        tui->selectedRow = -1;
      }
      tuiRender(tui);
      continue;
    }

    if (ch == TUI_KEY_ENTER) {
      if (tui->cmdBuf[0]) {
        /* Built-in SORT command */
        if ((tui->cmdBuf[0] == 'S' || tui->cmdBuf[0] == 's') &&
            (tui->cmdBuf[1] == 'O' || tui->cmdBuf[1] == 'o') &&
            (tui->cmdBuf[2] == 'R' || tui->cmdBuf[2] == 'r') &&
            (tui->cmdBuf[3] == 'T' || tui->cmdBuf[3] == 't') &&
            tui->cmdBuf[4] == ' ') {
          const char *colName = tui->cmdBuf + 5;
          while (*colName == ' ') colName++;
          int found = 0;
          for (int i = 0; i < tui->numColumns; i++) {
            /* Case-insensitive column name match */
            const char *a = tui->columns[i].name;
            const char *b = colName;
            int match = 1;
            while (*a && *b) {
              char ca = *a, cb = *b;
              if (ca >= 'a' && ca <= 'z') ca -= 32;
              if (cb >= 'a' && cb <= 'z') cb -= 32;
              if (ca != cb) { match = 0; break; }
              a++; b++;
            }
            if (match && !*a && (!*b || *b == ' ')) {
              if (i == tui->sortColumn) {
                tui->sortAscending = !tui->sortAscending;
              } else {
                tui->sortColumn = i;
                tui->sortAscending = 1;
              }
              if (tui->sortHandler) {
                tui->sortHandler(tui->sortColumn, tui->sortAscending,
                                 tui->userData);
              }
              found = 1;
              break;
            }
          }
          if (!found) {
            tuiSetStatus(tui, "Unknown column: %s", colName);
          }
          tui->cmdBuf[0] = '\0';
          tui->cmdPos = 0;
        } else if (tui->commandHandler) {
          int rc = tui->commandHandler(tui->cmdBuf, tui->userData);
          if (rc != 0) return rc;
          tui->cmdBuf[0] = '\0';
          tui->cmdPos = 0;
        }
      } else if (tui->selectedRow >= 0 && tui->selectHandler) {
        int rc = tui->selectHandler(tui->selectedRow, tui->userData);
        if (rc != 0) return rc;
      }
      tuiRender(tui);
      continue;
    }

    if (ch == TUI_KEY_BACKSPACE) {
      if (tui->cmdPos > 0) {
        tui->cmdPos--;
        tui->cmdBuf[tui->cmdPos] = '\0';
      }
      tuiRender(tui);
      continue;
    }

    if (ch == TUI_KEY_ESCAPE) {
      tui->cmdBuf[0] = '\0';
      tui->cmdPos = 0;
      tui->statusMsg[0] = '\0';
      tuiRender(tui);
      continue;
    }

    /* printable character — goes to command line */
    if (ch >= 0x40 && ch <= 0xFE) {  /* EBCDIC printable range */
      if (tui->cmdPos < (int)sizeof(tui->cmdBuf) - 1) {
        tui->cmdBuf[tui->cmdPos++] = (char)ch;
        tui->cmdBuf[tui->cmdPos] = '\0';
      }
      tuiRender(tui);
      continue;
    }
  }

  return 0;
}

/* ----------------------------------------------------------------
   Filter utilities
   ---------------------------------------------------------------- */

void tuiClearFilters(TuiTable *tui) {
  for (int i = 0; i < tui->numColumns; i++) {
    tui->columns[i].filter[0] = '\0';
  }
  tui->filterEditCol = -1;
  tui->filterEditPos = 0;
}

/*
  Simple wildcard match: * matches zero or more, ? matches exactly one.
  Case-insensitive.
*/
int tuiMatchFilter(const char *pattern, const char *value) {
  if (!pattern || !pattern[0]) return 1;  /* empty = match all */

  const char *p = pattern;
  const char *v = value;
  const char *starP = NULL;
  const char *starV = NULL;

  while (*v) {
    char pc = *p, vc = *v;
    /* Case fold */
    if (pc >= 'a' && pc <= 'z') pc -= 32;
    if (vc >= 'a' && vc <= 'z') vc -= 32;

    if (*p == '?') {
      p++; v++;
    } else if (*p == '*') {
      starP = p++;
      starV = v;
    } else if (pc == vc) {
      p++; v++;
    } else if (starP) {
      p = starP + 1;
      v = ++starV;
    } else {
      return 0;
    }
  }
  while (*p == '*') p++;
  return (*p == '\0');
}

/* ----------------------------------------------------------------
   Text viewer — full-screen scrollable text display
   ---------------------------------------------------------------- */

void tuiTextViewShow(TuiTextView *tv) {
  tui_getSize(&tv->screenRows, &tv->screenCols);
  tv->dataRows = tv->screenRows - 3;  /* title + header + status bar */
  if (tv->dataRows < 1) tv->dataRows = 1;
  tv->scrollRow = 0;
  tv->scrollCol = 0;

  while (1) {
    tui_getSize(&tv->screenRows, &tv->screenCols);
    tv->dataRows = tv->screenRows - 3;
    if (tv->dataRows < 1) tv->dataRows = 1;

    tui_hideCursor();

    /* Title bar */
    tui_moveTo(1, 1);
    tui_write(SGR_BOLD FG_WHITE BG_BLUE);
    tui_printf("%-*s", tv->screenCols, "");
    tui_moveTo(1, 1);
    {
      char right[80];
      int lastVis = tv->scrollRow + tv->dataRows;
      if (lastVis > tv->lineCount) lastVis = tv->lineCount;
      snprintf(right, sizeof(right), "LINE %d-%d (%d)",
               tv->lineCount > 0 ? tv->scrollRow + 1 : 0,
               lastVis, tv->lineCount);
      tui_printf(" %s", tv->title);
      int rlen = (int)strlen(right);
      int rpos = tv->screenCols - rlen;
      if (rpos > 0) {
        tui_moveTo(1, rpos);
        tui_printf("%s", right);
      }
    }
    tui_write(SGR_RESET);

    /* Column header — line numbers */
    tui_moveTo(2, 1);
    tui_write(SGR_BOLD FG_YELLOW);
    tui_clearLine();
    tui_printf("%-7s%s", "LINE", "CONTENT");
    tui_write(SGR_RESET);

    /* Data rows */
    for (int i = 0; i < tv->dataRows; i++) {
      int lineIdx = tv->scrollRow + i;
      int screenRow = 3 + i;

      tui_moveTo(screenRow, 1);
      tui_write(SGR_RESET);
      tui_clearLine();

      if (lineIdx < tv->lineCount && tv->lines[lineIdx]) {
        tui_printf(FG_CYAN "%6d " SGR_RESET, lineIdx + 1);
        const char *line = tv->lines[lineIdx];
        int lineLen = (int)strlen(line);
        int avail = tv->screenCols - 7;
        if (avail < 0) avail = 0;
        if (tv->scrollCol < lineLen) {
          int showLen = lineLen - tv->scrollCol;
          if (showLen > avail) showLen = avail;
          tui_printf("%.*s", showLen, line + tv->scrollCol);
        }
      }
    }

    /* Status bar */
    tui_moveTo(tv->screenRows, 1);
    tui_write(SGR_BOLD FG_WHITE BG_BLUE);
    tui_printf("%-*s", tv->screenCols, "");
    tui_moveTo(tv->screenRows, 1);
    tui_printf(" F3=Back  PgUp/PgDn  Up/Down  Left/Right=Scroll  Home/End");
    tui_write(SGR_RESET);

    int key = tuiReadKeyInternal(NULL);

    if (key == TUI_KEY_F3 || key == TUI_KEY_ESCAPE) {
      break;
    }
    if (key == TUI_KEY_UP) {
      if (tv->scrollRow > 0) tv->scrollRow--;
    } else if (key == TUI_KEY_DOWN) {
      if (tv->scrollRow + tv->dataRows < tv->lineCount) tv->scrollRow++;
    } else if (key == TUI_KEY_PGUP || key == TUI_KEY_F7) {
      tv->scrollRow -= tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    } else if (key == TUI_KEY_PGDN || key == TUI_KEY_F8) {
      tv->scrollRow += tv->dataRows;
      if (tv->scrollRow + tv->dataRows > tv->lineCount)
        tv->scrollRow = tv->lineCount - tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    } else if (key == TUI_KEY_LEFT) {
      if (tv->scrollCol > 0) tv->scrollCol--;
    } else if (key == TUI_KEY_RIGHT) {
      tv->scrollCol++;
    } else if (key == TUI_KEY_HOME) {
      tv->scrollRow = 0;
      tv->scrollCol = 0;
    } else if (key == TUI_KEY_END) {
      tv->scrollRow = tv->lineCount - tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    }
    /* 'q'/'Q' also exits (EBCDIC q=0x98, Q=0xD8) */
    if (key == 'q' || key == 'Q' || key == 0x98 || key == 0xD8) {
      break;
    }
  }
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
