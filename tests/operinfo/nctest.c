

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  nctest — proof-of-concept for ncurses + xlclang hybrid linking.

  ncurses (from zopen) is compiled in ASCII mode.
  Our code compiles in EBCDIC mode (xlclang default).
  All strings crossing the boundary need conversion.

  Tests:
    1. initscr / endwin lifecycle
    2. Color support detection and usage
    3. Window creation and text output
    4. Keyboard input (keypad mode)
    5. Panel overlay (modal dialog simulation)
    6. Basic form field (if linking works)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <locale.h>

#define _XOPEN_SOURCE_EXTENDED 1
#include <ncurses.h>
#include <panel.h>

/* ----------------------------------------------------------------
   ASCII/EBCDIC boundary conversion

   ncurses expects ASCII strings.  Our EBCDIC string literals
   must be converted before passing to ncurses.
   ---------------------------------------------------------------- */

int __atoe_l(char *bufferptr, int leng);
int __etoa_l(char *bufferptr, int leng);

/* Convert EBCDIC string literal to ASCII in a static buffer.
   Good for short strings (labels, format strings).
   Uses a ring of buffers so a few calls can be nested. */
#define A2E_BUFS 8
#define A2E_BUFLEN 256

static char a2eBufRing[A2E_BUFS][A2E_BUFLEN];
static int  a2eBufIdx = 0;

static char *toAscii(const char *ebcStr) {
  char *buf = a2eBufRing[a2eBufIdx];
  a2eBufIdx = (a2eBufIdx + 1) % A2E_BUFS;
  int len = (int)strlen(ebcStr);
  if (len >= A2E_BUFLEN) len = A2E_BUFLEN - 1;
  memcpy(buf, ebcStr, len);
  buf[len] = '\0';
  __etoa_l(buf, len);
  return buf;
}

/* Convert ASCII string from ncurses back to EBCDIC */
static char *toEbcdic(char *ascStr, int len) {
  __atoe_l(ascStr, len);
  return ascStr;
}

/* Convert a single ASCII char (from wgetch) to EBCDIC so we can
   compare with normal character literals like 'q', 'd', etc.
   Only converts printable ASCII range; passes through ncurses
   special keys (KEY_UP etc.) unchanged since they're > 0xFF. */
static int keyToEbcdic(int ch) {
  if (ch >= 0 && ch <= 127) {
    char c = (char)ch;
    __atoe_l(&c, 1);
    return (unsigned char)c;
  }
  return ch;  /* special keys pass through */
}

/* Wrappers for common ncurses calls that take strings */
static int nc_mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...) {
  /* Convert format string, then use vw_printw */
  char afmt[A2E_BUFLEN];
  int flen = (int)strlen(fmt);
  if (flen >= A2E_BUFLEN) flen = A2E_BUFLEN - 1;
  memcpy(afmt, fmt, flen);
  afmt[flen] = '\0';
  __etoa_l(afmt, flen);

  va_list ap;
  va_start(ap, fmt);
  wmove(w, y, x);
  int rc = vw_printw(w, afmt, ap);
  va_end(ap);
  return rc;
}

static int nc_mvwaddstr(WINDOW *w, int y, int x, const char *str) {
  char *astr = toAscii(str);
  return mvwaddstr(w, y, x, astr);
}

static int nc_waddstr(WINDOW *w, const char *str) {
  char *astr = toAscii(str);
  return waddstr(w, astr);
}

/* ----------------------------------------------------------------
   Test 1: Basic lifecycle + color detection
   ---------------------------------------------------------------- */

static void showColorInfo(WINDOW *win, int *row) {
  int hasCol = has_colors();
  nc_mvwaddstr(win, (*row)++, 2, hasCol ? "Colors: YES" : "Colors: NO");

  if (hasCol) {
    start_color();
    char buf[64];
    sprintf(buf, "COLORS=%d  COLOR_PAIRS=%d", COLORS, COLOR_PAIRS);
    nc_mvwaddstr(win, (*row)++, 2, buf);

    int canChange = can_change_color();
    nc_mvwaddstr(win, (*row)++, 2,
                 canChange ? "can_change_color: YES (truecolor possible)"
                           : "can_change_color: NO (fixed palette)");

    /* Set up some color pairs */
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_BLACK, COLOR_CYAN);
    init_pair(5, COLOR_WHITE, COLOR_RED);

    (*row)++;
    wattron(win, COLOR_PAIR(1));
    nc_mvwaddstr(win, (*row)++, 2, " White on Blue  ");
    wattroff(win, COLOR_PAIR(1));

    wattron(win, COLOR_PAIR(2) | A_BOLD);
    nc_mvwaddstr(win, (*row)++, 2, " Bold Yellow on Black ");
    wattroff(win, COLOR_PAIR(2) | A_BOLD);

    wattron(win, COLOR_PAIR(3));
    nc_mvwaddstr(win, (*row)++, 2, " Green on Black ");
    wattroff(win, COLOR_PAIR(3));

    wattron(win, COLOR_PAIR(4));
    nc_mvwaddstr(win, (*row)++, 2, " Black on Cyan (sort indicator) ");
    wattroff(win, COLOR_PAIR(4));

    wattron(win, COLOR_PAIR(5) | A_BOLD);
    nc_mvwaddstr(win, (*row)++, 2, " Bold White on Red (alert) ");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);
  }
}

/* ----------------------------------------------------------------
   Box drawing with Unicode characters via putp().

   ncurses ACS is fundamentally broken in the EBCDIC/ASCII hybrid:
   - mvwaddch silently fails
   - A_ALTCHARSET cells don't trigger smacs/rmacs in refresh
   - tputs with our putchar callback gets EBCDIC-converted by zoslib
   - VT.exe doesn't implement SCS G0 charset switching anyway

   Solution: putp() goes through ncurses's INTERNAL output path —
   the same one that correctly delivers SGR, CUP, etc. as ASCII.
   We use putp() to emit raw UTF-8 box-drawing characters directly.
   VT.exe is a full Unicode SDL2 terminal and renders them natively.

   Two-phase rendering:
   1. Write dialog content via ncurses (mvwaddstr etc.) + doupdate
   2. Overlay the border using mvcur() + putp() with UTF-8 bytes
   ---------------------------------------------------------------- */

/* UTF-8 box-drawing characters (raw bytes, no charset conversion) */
static const char BOX_UL[] = {0xE2, 0x94, 0x8C, 0};  /* ┌ U+250C */
static const char BOX_UR[] = {0xE2, 0x94, 0x90, 0};  /* ┐ U+2510 */
static const char BOX_LL[] = {0xE2, 0x94, 0x94, 0};  /* └ U+2514 */
static const char BOX_LR[] = {0xE2, 0x94, 0x98, 0};  /* ┘ U+2518 */
static const char BOX_HL[] = {0xE2, 0x94, 0x80, 0};  /* ─ U+2500 */
static const char BOX_VL[] = {0xE2, 0x94, 0x82, 0};  /* │ U+2502 */

/* Draw a box border on window w.  Call AFTER update_panels/doupdate.
   Uses mvcur for positioning (ncurses output path) and putp for
   the UTF-8 box chars (also ncurses output path). */
static void nc_box(WINDOW *w) {
  int rows, cols, begy, begx;
  getmaxyx(w, rows, cols);
  getbegyx(w, begy, begx);

  /* Top row */
  mvcur(-1, -1, begy, begx);
  putp(BOX_UL);
  for (int x = 1; x < cols - 1; x++) putp(BOX_HL);
  putp(BOX_UR);

  /* Side edges */
  for (int y = 1; y < rows - 1; y++) {
    mvcur(begy + y, begx + cols, begy + y, begx);
    putp(BOX_VL);
    mvcur(begy + y, begx + 1, begy + y, begx + cols - 1);
    putp(BOX_VL);
  }

  /* Bottom row */
  mvcur(begy + rows - 2, begx + cols, begy + rows - 1, begx);
  putp(BOX_LL);
  for (int x = 1; x < cols - 1; x++) putp(BOX_HL);
  putp(BOX_LR);

  fflush(stdout);
}

/* ----------------------------------------------------------------
   Test 2: Panel overlay (modal dialog)
   ---------------------------------------------------------------- */

static void showModalDialog(void) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int dh = 10, dw = 44;
  int dy = (rows - dh) / 2;
  int dx = (cols - dw) / 2;

  WINDOW *dialog = newwin(dh, dw, dy, dx);
  PANEL *panel = new_panel(dialog);

  /* Fill background with white-on-blue so the dialog is visible */
  wbkgd(dialog, COLOR_PAIR(1));
  werase(dialog);

  wattron(dialog, COLOR_PAIR(1) | A_BOLD);
  nc_mvwaddstr(dialog, 0, 2, " Filter: JOBNAME ");
  wattroff(dialog, COLOR_PAIR(1) | A_BOLD);

  wattron(dialog, COLOR_PAIR(1));
  nc_mvwaddstr(dialog, 2, 2, "Enter wildcard pattern:");
  nc_mvwaddstr(dialog, 3, 2, "(* = any, ? = single char)");

  /* Simulated input field with underscores */
  wattron(dialog, COLOR_PAIR(2) | A_UNDERLINE);
  nc_mvwaddstr(dialog, 5, 2, "                                        ");
  wattroff(dialog, COLOR_PAIR(2) | A_UNDERLINE);

  wattron(dialog, COLOR_PAIR(1));
  nc_mvwaddstr(dialog, 7, 2, "Enter=OK  Esc=Cancel");
  nc_mvwaddstr(dialog, 8, 2, "This is a panel overlay test");
  wattroff(dialog, COLOR_PAIR(1));

  /* Flush ncurses content first, then overlay the box border
     using tputs smacs/rmacs + putp for drawing-mode characters
     (ncurses ACS is broken in the hybrid EBCDIC/ASCII environment). */
  update_panels();
  doupdate();
  nc_box(dialog);

  /* Wait for any key */
  wgetch(dialog);

  del_panel(panel);
  delwin(dialog);
  update_panels();
  doupdate();
}

/* ----------------------------------------------------------------
   Test 3: Keyboard input
   ---------------------------------------------------------------- */

static void showKeyTest(WINDOW *win, int startRow) {
  nc_mvwaddstr(win, startRow, 2,
               "Press keys to test (q=quit, d=dialog, arrows/F-keys):");
  wrefresh(win);

  int row = startRow + 1;
  int maxRows;
  int maxCols;
  getmaxyx(win, maxRows, maxCols);

  while (1) {
    int rawCh = wgetch(win);
    int ch = keyToEbcdic(rawCh);
    if (ch == 'q' || ch == 'Q') {
      break;
    }

    /* 'd' = show modal dialog */
    if (ch == 'd' || ch == 'D') {
      showModalDialog();
      touchwin(win);
      wrefresh(win);
      continue;
    }

    if (row >= maxRows - 1) {
      /* scroll area */
      row = startRow + 1;
      wmove(win, row, 0);
      wclrtobot(win);
    }

    char desc[80];
    /* ch is EBCDIC for printable, raw ncurses value for special keys */
    if (ch >= 0x100) {
      /* ncurses special keys — check raw value */
      switch (rawCh) {
      case KEY_UP:    sprintf(desc, "KEY_UP"); break;
      case KEY_DOWN:  sprintf(desc, "KEY_DOWN"); break;
      case KEY_LEFT:  sprintf(desc, "KEY_LEFT"); break;
      case KEY_RIGHT: sprintf(desc, "KEY_RIGHT"); break;
      case KEY_PPAGE: sprintf(desc, "KEY_PGUP"); break;
      case KEY_NPAGE: sprintf(desc, "KEY_PGDN"); break;
      case KEY_HOME:  sprintf(desc, "KEY_HOME"); break;
      case KEY_END:   sprintf(desc, "KEY_END"); break;
      case KEY_F(3):  sprintf(desc, "F3"); break;
      case KEY_F(5):  sprintf(desc, "F5"); break;
      case KEY_F(6):  sprintf(desc, "F6"); break;
      case KEY_F(7):  sprintf(desc, "F7"); break;
      case KEY_F(8):  sprintf(desc, "F8"); break;
      case KEY_MOUSE: sprintf(desc, "MOUSE"); break;
      case KEY_RESIZE:sprintf(desc, "RESIZE"); break;
      case KEY_BTAB:  sprintf(desc, "BACKTAB"); break;
      case KEY_ENTER: sprintf(desc, "ENTER (keypad)"); break;
      default:        sprintf(desc, "special 0x%04X", rawCh); break;
      }
    } else {
      /* Printable or control — ch is EBCDIC now */
      switch (ch) {
      case '\t':      sprintf(desc, "TAB"); break;
      case '\n':
      case '\r':      sprintf(desc, "ENTER"); break;
      case 0x27:      sprintf(desc, "ESCAPE"); break; /* EBCDIC ESC */
      default:
        if (ch >= 0x40 && ch <= 0xFE) {
          sprintf(desc, "char '%c' (raw=0x%02X ebc=0x%02X)", ch, rawCh, ch);
        } else {
          sprintf(desc, "ctrl raw=0x%02X ebc=0x%02X", rawCh, ch);
        }
        break;
      }
    }

    char line[128];
    sprintf(line, "  key: %-40s", desc);
    nc_mvwaddstr(win, row++, 2, line);
    wrefresh(win);
  }
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  /* Enable UTF-8 wide-char output for box-drawing characters.
     setlocale takes ASCII strings since it goes through the C runtime
     which is in ASCII mode when linked with zoslib. */
  {
    char lc[8] = "";
    __etoa_l(lc, 0);  /* empty string is fine */
    setlocale(LC_ALL, toAscii(""));
  }

  /* Initialize ncurses */
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  mouseinterval(0);

  /* Enable mouse if available */
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  /* Title bar */
  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_BLACK, COLOR_CYAN);
    init_pair(5, COLOR_WHITE, COLOR_RED);
  }

  attron(COLOR_PAIR(1) | A_BOLD);
  {
    char titleBar[256];
    /* Must use ASCII space (0x20) — this goes straight to ncurses */
    memset(titleBar, 0x20, cols < 255 ? cols : 255);
    titleBar[cols < 255 ? cols : 255] = '\0';
    char *t = toAscii("NCURSES TEST -- xlclang hybrid link POC");
    int tlen = (int)strlen(t);
    if (tlen + 2 < cols) memcpy(titleBar + 2, t, tlen);
    mvaddstr(0, 0, titleBar);
  }
  attroff(COLOR_PAIR(1) | A_BOLD);

  /* Show terminal info */
  int row = 2;
  char info[128];
  sprintf(info, "Terminal: %dx%d", cols, rows);
  nc_mvwaddstr(stdscr, row++, 2, info);

  char *termEnv = getenv(toAscii("TERM"));
  if (termEnv) {
    toEbcdic(termEnv, strlen(termEnv));
    sprintf(info, "TERM=%s", termEnv);
    nc_mvwaddstr(stdscr, row++, 2, info);
  }

  char *colorTerm = getenv(toAscii("COLORTERM"));
  if (colorTerm) {
    toEbcdic(colorTerm, strlen(colorTerm));
    sprintf(info, "COLORTERM=%s", colorTerm);
  } else {
    sprintf(info, "COLORTERM not set");
  }
  nc_mvwaddstr(stdscr, row++, 2, info);

  row++;

  /* Color tests */
  showColorInfo(stdscr, &row);

  row++;

  /* Key test */
  showKeyTest(stdscr, row);

  endwin();

  printf("ncurses test complete.\n");
  printf("If you saw colors, a title bar, and key echo, the hybrid link works.\n");

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
