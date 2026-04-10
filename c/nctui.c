

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  ncurses-based TUI framework for z/OS.

  Hybrid EBCDIC/ASCII link: ncurses (zopen) compiled ASCII, our code EBCDIC.
  All strings crossing the boundary are converted with __etoa_l / __atoe_l.
  Box drawing uses Unicode characters via putp() through ncurses's internal
  output path (the only reliable way to emit raw bytes in this environment).

  Build with xlclang++ (zoslib is C++):
    xlclang++ ... -I $NC_INC -I $NC_PARENT -I $ZOSLIB_INC \
      nctui.c ... $NC_LIB/libpanelw.a $NC_LIB/libncursesw.a $ZOSLIB_LIB/libzoslib.x
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <locale.h>

#include "nctui.h"

/* term.h provides putp(), mvcur(), tigetstr() for box drawing.
   It also pollutes the namespace with macros like 'columns' and 'lines'
   that collide with common struct field names — undef them immediately. */
#include <term.h>
#undef columns
#undef lines

/* ================================================================
   Layer 1: EBCDIC/ASCII boundary conversion
   ================================================================ */

int __atoe_l(char *bufferptr, int leng);
int __etoa_l(char *bufferptr, int leng);

#define CONV_BUFS    16
#define CONV_BUFLEN  512

static char convBufRing[CONV_BUFS][CONV_BUFLEN];
static int  convBufIdx = 0;

char *nctuiToAscii(const char *ebcStr) {
  char *buf = convBufRing[convBufIdx];
  convBufIdx = (convBufIdx + 1) % CONV_BUFS;
  int len = (int)strlen(ebcStr);
  if (len >= CONV_BUFLEN) len = CONV_BUFLEN - 1;
  memcpy(buf, ebcStr, len);
  buf[len] = '\0';
  __etoa_l(buf, len);
  return buf;
}

char *nctuiToEbcdic(char *ascStr, int len) {
  __atoe_l(ascStr, len);
  return ascStr;
}

int nctuiKeyToEbcdic(int ch) {
  if (ch >= 0 && ch <= 127) {
    char c = (char)ch;
    __atoe_l(&c, 1);
    return (unsigned char)c;
  }
  return ch;
}

/* ncurses string wrappers — convert EBCDIC to ASCII before passing */

static int nc_mvwaddstr(WINDOW *w, int y, int x, const char *str) {
  char *astr = nctuiToAscii(str);
  return mvwaddstr(w, y, x, astr);
}

static int nc_waddstr(WINDOW *w, const char *str) {
  char *astr = nctuiToAscii(str);
  return waddstr(w, astr);
}

static int nc_mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...) {
  char fmtBuf[CONV_BUFLEN];
  int flen = (int)strlen(fmt);
  if (flen >= CONV_BUFLEN) flen = CONV_BUFLEN - 1;
  memcpy(fmtBuf, fmt, flen);
  fmtBuf[flen] = '\0';
  __etoa_l(fmtBuf, flen);

  va_list ap;
  va_start(ap, fmt);
  wmove(w, y, x);
  int rc = vw_printw(w, fmtBuf, ap);
  va_end(ap);
  return rc;
}

/* Fill a window row with ASCII spaces (0x20, not EBCDIC 0x40) */
static void nc_fillRow(WINDOW *w, int y, int cols) {
  char spaces[256];
  int n = cols < 255 ? cols : 255;
  memset(spaces, 0x20, n);
  spaces[n] = '\0';
  mvwaddstr(w, y, 0, spaces);
}

/* ================================================================
   Layer 1a: ASCII key constants for rawCh comparisons

   getch() returns ASCII values, but EBCDIC-compiled character literals
   like '\n', '\r', '\t' have different numeric values.  Use explicit
   ASCII codes so comparisons work correctly.
   ================================================================ */

#define ASCII_LF   0x0A  /* '\n' in ASCII — Enter/newline */
#define ASCII_CR   0x0D  /* '\r' in ASCII — carriage return */
#define ASCII_TAB  0x09  /* '\t' in ASCII — tab */
#define ASCII_ESC  0x1B  /* ESC — same in both encodings */
#define ASCII_BS   0x08  /* backspace */
#define ASCII_DEL  0x7F  /* delete */

/* ================================================================
   Layer 1b: VT220 function key fallback

   When TERM=xterm, ncurses expects F-keys as ESC O P/Q/R/S (app mode).
   PuTTY sends ESC [ nn ~ (VT220 style) which ncurses can't decode,
   so getch() returns the raw bytes one at a time: ESC, [, digits, ~.
   We accumulate these and translate to KEY_F(n).
   ================================================================ */

static int pendingKeys[16];
static int pendingCount = 0;
static int pendingPos   = 0;

/* SGR mouse state — filled by readSGRMouseParams() after KEY_MOUSE.
   Format: ESC [ < button ; col ; row M (press) or m (release).
   define_key() consumes the ESC[< prefix; we read the rest manually.
   Col and row are 1-based in the protocol, stored here as 0-based. */
#define NCTUI_SGR_MOUSE 0x10000  /* synthetic key code for SGR mouse */
static int  sgrMouseButton = 0;
static int  sgrMouseCol    = 0;  /* 0-based */
static int  sgrMouseRow    = 0;  /* 0-based */
static int  sgrMousePress  = 0;  /* 1=press (M), 0=release (m) */

/* Read SGR mouse parameters after KEY_MOUSE.
   define_key consumed ESC[<, so input has: button;col;rowM or button;col;rowm
   Returns 1 on success, 0 on failure/timeout. */
static int readSGRMouseParams(void) {
  int params[3] = {0};
  int pi = 0;

  nodelay(stdscr, TRUE);
  for (int i = 0; i < 30; i++) {
    int ch = getch();
    if (ch == ERR) { napms(1); continue; }
    if (ch >= 0x30 && ch <= 0x39) {
      if (pi < 3) params[pi] = params[pi] * 10 + (ch - 0x30);
      continue;
    }
    if (ch == 0x3B) { pi++; continue; }
    if (ch == 0x4D || ch == 0x6D) {  /* M=press, m=release */
      nodelay(stdscr, FALSE);
      sgrMouseButton = params[0];
      sgrMouseCol    = (params[1] > 0) ? params[1] - 1 : 0;
      sgrMouseRow    = (pi >= 2 && params[2] > 0) ? params[2] - 1 : 0;
      sgrMousePress  = (ch == 0x4D) ? 1 : 0;
      return 1;
    }
    break;  /* unexpected byte */
  }
  nodelay(stdscr, FALSE);
  return 0;
}

/* Resolve a CSI letter to an ncurses KEY_* constant.
   Handles ESC[A..D (arrows), ESC[H/F (home/end), ESC[Z (backtab). */
static int csiLetterToKey(int letter) {
  switch (letter) {
    case 0x41: return KEY_UP;       /* A */
    case 0x42: return KEY_DOWN;     /* B */
    case 0x43: return KEY_RIGHT;    /* C */
    case 0x44: return KEY_LEFT;     /* D */
    case 0x48: return KEY_HOME;     /* H */
    case 0x46: return KEY_END;      /* F */
    case 0x5A: return KEY_BTAB;     /* Z (shift-tab) */
    default:   return -1;
  }
}

/* Resolve a CSI numeric code (from ESC[nn~) to KEY_*. */
static int csiCodeToKey(int code) {
  switch (code) {
    case 11: return KEY_F(1);
    case 12: return KEY_F(2);
    case 13: return KEY_F(3);
    case 14: return KEY_F(4);
    case 15: return KEY_F(5);
    case 17: return KEY_F(6);
    case 18: return KEY_F(7);
    case 19: return KEY_F(8);
    case 20: return KEY_F(9);
    case 21: return KEY_F(10);
    case 23: return KEY_F(11);
    case 24: return KEY_F(12);
    case 1:  return KEY_HOME;
    case 2:  return KEY_IC;    /* Insert */
    case 3:  return KEY_DC;    /* Delete */
    case 4:  return KEY_END;
    case 5:  return KEY_PPAGE;
    case 6:  return KEY_NPAGE;
    default: return -1;
  }
}

/* Try to parse a CSI sequence after ESC was received by getch().
   Handles both forms:
     ESC [ <letter>       — cursor keys, home/end, backtab
     ESC [ <digits> ~     — VT220 F-keys, ins/del, pgup/pgdn
     ESC [ <digits> ; <digits> <letter> — modifier combos (ignored, mapped to base key)
   Returns KEY_* on success, or stuffs partial bytes into
   pendingKeys[] and returns ASCII_ESC. */
static int resolveEscSeq(void) {
  nodelay(stdscr, TRUE);
  int c1 = getch();
  if (c1 == ERR) { nodelay(stdscr, FALSE); return ASCII_ESC; }

  /* ESC O P/Q/R/S — application mode F1-F4 */
  if (c1 == 0x4F) { /* 'O' */
    int c2 = getch();
    nodelay(stdscr, FALSE);
    if (c2 == ERR) {
      pendingKeys[0] = 0x4F; pendingCount = 1; pendingPos = 0;
      return ASCII_ESC;
    }
    switch (c2) {
      case 0x50: return KEY_F(1);  /* P */
      case 0x51: return KEY_F(2);  /* Q */
      case 0x52: return KEY_F(3);  /* R */
      case 0x53: return KEY_F(4);  /* S */
      default:
        pendingKeys[0] = 0x4F; pendingKeys[1] = c2;
        pendingCount = 2; pendingPos = 0;
        return ASCII_ESC;
    }
  }

  if (c1 != 0x5B) { /* not '[' — not a CSI sequence */
    pendingKeys[0] = c1; pendingCount = 1; pendingPos = 0;
    nodelay(stdscr, FALSE);
    return ASCII_ESC;
  }

  /* CSI sequence: read bytes after '[' */
  char buf[32];
  int  bufLen = 0;

  /* Peek at the first byte to detect SGR mouse: ESC [ < ... M/m */
  int first = getch();
  if (first == ERR) {
    nodelay(stdscr, FALSE);
    pendingKeys[0] = 0x5B; pendingCount = 1; pendingPos = 0;
    return ASCII_ESC;
  }

  if (first == 0x3C) {
    /* SGR mouse sequence: ESC [ < button ; col ; row M/m
       Read params until we get 'M' (0x4D=press) or 'm' (0x6D=release). */
    int params[3] = {0};
    int pi = 0;
    while (bufLen < 30) {
      int d = getch();
      if (d == ERR) break;
      if (d >= 0x30 && d <= 0x39) {
        if (pi < 3) params[pi] = params[pi] * 10 + (d - 0x30);
        continue;
      }
      if (d == 0x3B) { pi++; continue; }
      if (d == 0x4D || d == 0x6D) {
        /* Complete SGR mouse event */
        nodelay(stdscr, FALSE);
        sgrMouseButton = params[0];
        sgrMouseCol    = (params[1] > 0) ? params[1] - 1 : 0;  /* 1-based → 0-based */
        sgrMouseRow    = (pi >= 2 && params[2] > 0) ? params[2] - 1 : 0;
        sgrMousePress  = (d == 0x4D) ? 1 : 0;
        return NCTUI_SGR_MOUSE;
      }
      break;  /* unexpected byte — bail */
    }
    nodelay(stdscr, FALSE);
    return ASCII_ESC;  /* incomplete or malformed mouse sequence */
  }

  /* Not SGR mouse — put first byte into buf and continue normal CSI parse */
  buf[bufLen++] = (char)first;

  /* Check if first byte is already a final byte */
  if (first >= 0x40 && first <= 0x7E) {
    nodelay(stdscr, FALSE);
    int key = csiLetterToKey(first);
    if (key >= 0) return key;
    return ASCII_ESC;
  }

  while (bufLen < 15) {
    int d = getch();
    if (d == ERR) break;
    buf[bufLen++] = (char)d;

    /* Check if this byte is a CSI final byte (letter 0x40-0x7E) */
    if (d >= 0x40 && d <= 0x7E) {
      nodelay(stdscr, FALSE);

      if (d == 0x7E) {
        /* ESC [ nn ~ — VT220 style */
        int code = 0;
        for (int i = 0; i < bufLen - 1; i++) {
          char c = buf[i];
          if (c >= 0x30 && c <= 0x39)
            code = code * 10 + (c - 0x30);
          else if (c == 0x3B)  /* ';' — modifier separator, ignore */
            break;
          else
            break;
        }
        int key = csiCodeToKey(code);
        if (key >= 0) return key;
        return ASCII_ESC; /* unrecognized code */
      }

      /* ESC [ (digits;digits) letter — check letter */
      int key = csiLetterToKey(d);
      if (key >= 0) return key;

      /* Unrecognized final letter — discard sequence */
      return ASCII_ESC;
    }
  }

  /* Ran out of bytes — incomplete sequence, stuff into pending */
  pendingKeys[0] = 0x5B;
  int p = 1;
  for (int i = 0; i < bufLen && p < 15; i++) pendingKeys[p++] = buf[i];
  pendingCount = p; pendingPos = 0;
  nodelay(stdscr, FALSE);
  return ASCII_ESC;
}

/* Wrapper around getch() that handles pending keys and VT220 fallback */
static int nctuiGetch(void) {
  if (pendingPos < pendingCount) {
    return pendingKeys[pendingPos++];
  }
  pendingCount = 0; pendingPos = 0;
  int ch = getch();
  if (ch == ASCII_ESC) {
    return resolveEscSeq();
  }
  return ch;
}

/* ================================================================
   Layer 1c: Unicode box drawing via putp()

   putp() goes through ncurses's internal output path — the same one
   that correctly delivers SGR and CUP as ASCII.  We use it to emit
   raw UTF-8 box-drawing characters.  mvcur() handles positioning.
   ================================================================ */

static const char BOX_UL[] = {0xE2, 0x94, 0x8C, 0};  /* U+250C */
static const char BOX_UR[] = {0xE2, 0x94, 0x90, 0};  /* U+2510 */
static const char BOX_LL[] = {0xE2, 0x94, 0x94, 0};  /* U+2514 */
static const char BOX_LR[] = {0xE2, 0x94, 0x98, 0};  /* U+2518 */
static const char BOX_HL[] = {0xE2, 0x94, 0x80, 0};  /* U+2500 */
static const char BOX_VL[] = {0xE2, 0x94, 0x82, 0};  /* U+2502 */

/* Sort direction indicators — UTF-8 triangles */
static const char SORT_ASC[]  = {0xE2, 0x96, 0xB2, 0};  /* U+25B2 ▲ */
static const char SORT_DESC[] = {0xE2, 0x96, 0xBC, 0};  /* U+25BC ▼ */

/* Selection marker for NP column — UTF-8 arrow */
static const char NP_CURSOR[] = {0xE2, 0x96, 0xB6, 0};  /* U+25B6 ▶ */

/* ASCII fallback glyphs for terminals without Unicode rendering */
static const char BOX_UL_FB[] = {0x2B, 0};  /* + */
static const char BOX_UR_FB[] = {0x2B, 0};  /* + */
static const char BOX_LL_FB[] = {0x2B, 0};  /* + */
static const char BOX_LR_FB[] = {0x2B, 0};  /* + */
static const char BOX_HL_FB[] = {0x2D, 0};  /* - */
static const char BOX_VL_FB[] = {0x7C, 0};  /* | */
static const char SORT_ASC_FB[]  = {0x5E, 0};  /* ^ */
static const char SORT_DESC_FB[] = {0x76, 0};  /* v */
static const char NP_CURSOR_FB[] = {0x3E, 0};  /* > */

/* ================================================================
   Layer 1d: Unicode glyph capability detection

   Probes the terminal at init time by writing a known UTF-8 glyph
   and querying cursor position via DSR (Device Status Report).
   If the glyph rendered as 1 cell → Unicode works.
   If 3 cells (one per raw byte) → terminal doesn't support Unicode.
   Override with NCTUI_UNICODE=0 or NCTUI_UNICODE=1 env var.
   ================================================================ */

static int nctuiUnicodeSupported = 0;

/* Accessor macro — picks Unicode or ASCII fallback glyph */
#define GLYPH(u, fb) (nctuiUnicodeSupported ? (u) : (fb))

static void probeUnicodeSupport(void) {
  nctuiUnicodeSupported = 0;

  /* Environment override: NCTUI_UNICODE=0|1.
     getenv() returns EBCDIC on z/OS: '0'=0xF0, '1'=0xF1 */
  const char *ov = getenv("NCTUI_UNICODE");
  if (ov && ov[0]) {
    if (ov[0] == '1' || ov[0] == 0xF1) { nctuiUnicodeSupported = 1; return; }
    if (ov[0] == '0' || ov[0] == 0xF0) { nctuiUnicodeSupported = 0; return; }
  }

  /* Cursor position probe:
     1. Move to column 0 on last row
     2. Output ▲ (3-byte UTF-8)
     3. Send DSR ESC[6n to query cursor position
     4. col==2 → 1-cell glyph (Unicode), col==4 → 3 raw bytes (no Unicode) */
  refresh();

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int testRow = rows - 1;

  mvcur(-1, -1, testRow, 0);
  putp(SORT_ASC);  /* test character: 3-byte UTF-8 */

  /* DSR: ESC [ 6 n — all bytes < 0x80, safe as-is */
  static const char DSR[] = {0x1B, 0x5B, 0x36, 0x6E, 0};
  putp(DSR);
  fflush(stdout);

  /* Read CPR response: ESC [ row ; col R
     keypad is already FALSE — ncurses won't interfere. */
  nodelay(stdscr, TRUE);

  int respBuf[32];
  int respLen = 0;
  int gotEsc = 0, gotBracket = 0;

  for (int wait = 0; wait < 50; wait++) {  /* 50 * 10ms = 500ms max */
    int ch = getch();
    if (ch == ERR) { napms(10); continue; }

    if (!gotEsc)     { if (ch == 0x1B) gotEsc = 1;    continue; }
    if (!gotBracket) { if (ch == 0x5B) gotBracket = 1; else gotEsc = 0; continue; }

    respBuf[respLen++] = ch;
    if (ch == 0x52) break;  /* ASCII 'R' — end of CPR */
    if (respLen >= 30) break;
  }

  nodelay(stdscr, FALSE);

  /* Parse: row-digits ; col-digits R */
  if (respLen > 0 && respBuf[respLen - 1] == 0x52) {
    int i = 0, respCol = 0;
    /* skip row digits */
    while (i < respLen && respBuf[i] >= 0x30 && respBuf[i] <= 0x39) i++;
    /* skip semicolon (ASCII 0x3B) */
    if (i < respLen && respBuf[i] == 0x3B) i++;
    /* parse col */
    while (i < respLen && respBuf[i] >= 0x30 && respBuf[i] <= 0x39) {
      respCol = respCol * 10 + (respBuf[i] - 0x30);
      i++;
    }
    /* Started at col 1 (1-based).  1-cell glyph → col 2. */
    if (respCol == 2) nctuiUnicodeSupported = 1;
  }

  /* Clean up test row */
  mvcur(-1, -1, testRow, 0);
  putp(tigetstr("el"));  /* clear to end of line */
  fflush(stdout);
}

/* ================================================================
   Layer 1e: Terminal capability probe (DA1/DA2/XTVERSION)

   Queries the terminal at init time to discover real capabilities
   instead of trusting static terminfo.  Runs after initscr() using
   the same putp()/getch() pattern as probeUnicodeSupport().

   DA1  (ESC[c)   → ESC[?<params>c   — feature flags
   DA2  (ESC[>c)  → ESC[><Pp>;<Pv>;<Pc>c — terminal type/version
   XTVERSION (ESC[>0q) → DCS>|<name>ST  — terminal name string

   Based on responses, registers escape sequences with define_key()
   so ncurses can parse them even if terminfo is incomplete.
   ================================================================ */

/* Probe result storage */
static int  nctuiTermProbed = 0;

/* DA2 terminal type codes (Pp parameter) */
#define DA2_VT100    0
#define DA2_VT220    1
#define DA2_VT320    2
#define DA2_XTERM   41
#define DA2_MLTERM   24
#define DA2_MINTTY   77
#define DA2_ITERM2   99  /* iTerm2 sometimes uses this */

/* Probe results */
static int  nctuiDA1Params[16];
static int  nctuiDA1Count = 0;
static int  nctuiDA2Type = -1;     /* Pp from DA2 */
static int  nctuiDA2Version = -1;  /* Pv from DA2 */
static char nctuiTermName[128];    /* from XTVERSION */
static int  nctuiHasSGRMouse = 0;  /* SGR mouse capability detected or assumed */

/* Read a CSI response from the terminal with timeout.
   Expects: ESC [ <prefix> <params> <final>
   Returns the final byte, fills params[] with semicolon-separated
   integers, sets *paramCount.  Returns -1 on timeout/error.
   If firstParamChar is non-zero, it must be the first byte after '['.
   If a different prefix arrives (wrong response), drains to final byte
   and retries — handles out-of-order responses gracefully. */
static int readCSIResponse(int params[], int *paramCount, int maxParams,
                           int firstParamChar, int timeoutMs) {
  *paramCount = 0;
  int curParam = 0;
  int hasDigit = 0;
  int state = 0;  /* 0=waitESC 1=waitBracket 2=waitPrefix 3=readParams */
  int draining = 0; /* 1=wrong prefix, skip to final byte then retry */

  for (int elapsed = 0; elapsed < timeoutMs; elapsed += 5) {
    int ch = getch();
    if (ch == ERR) { napms(5); continue; }
    elapsed = 0;  /* reset timeout when we get data */

    switch (state) {
      case 0: /* waiting for ESC */
        if (ch == 0x1B) state = 1;
        continue;

      case 1: /* waiting for '[' */
        if (ch == 0x5B) { state = firstParamChar ? 2 : 3; continue; }
        state = 0;
        if (ch == 0x1B) state = 1;  /* could be start of another ESC */
        continue;

      case 2: /* waiting for prefix char (e.g. '?' or '>') */
        if (ch == firstParamChar) { state = 3; continue; }
        /* Wrong prefix — this is a different CSI response.
           Drain it to the final byte and restart. */
        draining = 1;
        state = 3;
        /* fall through to process this byte as param data */
        /* FALLTHROUGH */

      case 3: /* reading parameters */
        if (ch >= 0x30 && ch <= 0x39) {
          curParam = curParam * 10 + (ch - 0x30);
          hasDigit = 1;
          continue;
        }
        if (ch == 0x3B) {
          if (!draining && *paramCount < maxParams)
            params[(*paramCount)++] = curParam;
          curParam = 0; hasDigit = 0;
          continue;
        }
        if (ch >= 0x40 && ch <= 0x7E) {
          /* Final byte — end of this response */
          if (draining) {
            /* We consumed a wrong-type response; reset and try again */
            draining = 0;
            *paramCount = 0;
            curParam = 0; hasDigit = 0;
            state = 0;
            continue;
          }
          if (hasDigit && *paramCount < maxParams)
            params[(*paramCount)++] = curParam;
          return ch;
        }
        /* Non-CSI byte in param position — protocol error, reset */
        state = 0;
        *paramCount = 0;
        curParam = 0; hasDigit = 0; draining = 0;
        continue;
    }
  }
  return -1;  /* timeout */
}

/* Read a DCS response: DCS > | <text> ST
   DCS = ESC P, ST = ESC \
   Fills buf with the text portion. Returns length or -1 on timeout. */
static int readDCSResponse(char *buf, int bufSize, int timeoutMs) {
  int state = 0;  /* 0=waitESC 1=waitP 2=waitPipe 3=readText 4=waitST */
  int pos = 0;

  for (int elapsed = 0; elapsed < timeoutMs; elapsed += 5) {
    int ch = getch();
    if (ch == ERR) { napms(5); continue; }

    switch (state) {
      case 0: if (ch == 0x1B) state = 1; break;
      case 1:
        if (ch == 0x50) state = 2;          /* ESC P = DCS */
        else { state = 0; if (ch == 0x1B) state = 1; }
        break;
      case 2:
        if (ch == 0x3E) state = 3;          /* > after DCS */
        else return -1;
        break;
      case 3:
        if (ch == 0x7C) state = 4;          /* | separator */
        else return -1;                      /* unexpected format */
        break;
      case 4:
        if (ch == 0x1B) {                   /* ESC — start of ST */
          if (pos < bufSize) buf[pos] = '\0';
          return pos;
        }
        if (pos < bufSize - 1) buf[pos++] = (char)ch;
        break;
    }
  }
  return -1;  /* timeout */
}

static void probeTerminalCapabilities(void) {
  if (nctuiTermProbed) return;
  nctuiTermProbed = 1;
  nctuiTermName[0] = '\0';

  /* Use ncurses I/O — same pattern as probeUnicodeSupport.
     keypad is already FALSE — ncurses won't interfere. */
  nodelay(stdscr, TRUE);
  refresh();

  /* Send DA1 + DA2 together to minimize round-trip latency.
     Responses arrive in order: DA1 first (ESC[?...c), then DA2 (ESC[>...c).
     Skip XTVERSION for now — many terminals don't support it and
     it uses DCS (different framing), adding complexity for little gain.
     Can be added later when we need terminal name identification. */

  /* DA1: ESC[c  |  DA2: ESC[>c */
  static const char PROBES[] = {
    0x1B, 0x5B, 0x63,              /* DA1 */
    0x1B, 0x5B, 0x3E, 0x63,       /* DA2 */
    0
  };
  putp(PROBES);
  fflush(stdout);

  /* --- Read DA1 response: ESC [ ? <params> c --- */
  int final = readCSIResponse(nctuiDA1Params, &nctuiDA1Count, 16, 0x3F, 300);
  if (final == 0x63) {
    /* DA1 success. Params are feature flags — mostly informational for now.
       Common: 1=132cols 4=sixel 6=selectiveErase 22=ANSI-color */
  }

  /* --- Read DA2 response: ESC [ > Pp ; Pv ; Pc c --- */
  int da2Params[4] = {0};
  int da2Count = 0;
  final = readCSIResponse(da2Params, &da2Count, 4, 0x3E, 300);
  if (final == 0x63 && da2Count >= 1) {
    nctuiDA2Type = da2Params[0];
    if (da2Count >= 2) nctuiDA2Version = da2Params[1];
  }

  nodelay(stdscr, FALSE);

  /* --- Decide capabilities based on probe results ---

     SGR mouse (mode 1006) is supported by essentially every modern
     terminal: xterm, VT.exe, PuTTY (3.4+), iTerm2, Windows Terminal,
     mintty, mlterm, foot, alacritty, kitty, etc.

     If DA1 or DA2 responded, the terminal understands standard queries
     and is certainly modern enough for SGR mouse.  Even if neither
     responded (ancient terminal or bad connection), registering the
     SGR mouse prefix with define_key() is harmless — it just means
     ncurses will recognize ESC[< as KEY_MOUSE if it ever arrives. */
  nctuiHasSGRMouse = 1;  /* assume yes — safe default */

  /* Mouse is handled entirely outside ncurses — DECSET sent in init,
     SGR parsing in resolveEscSeq().  No define_key or mousemask needed. */
}

void nctuiDrawBox(WINDOW *w) {
  int rows, cols, begy, begx;
  getmaxyx(w, rows, cols);
  getbegyx(w, begy, begx);

  /* Top row */
  mvcur(-1, -1, begy, begx);
  putp(GLYPH(BOX_UL, BOX_UL_FB));
  for (int x = 1; x < cols - 1; x++) putp(GLYPH(BOX_HL, BOX_HL_FB));
  putp(GLYPH(BOX_UR, BOX_UR_FB));

  /* Side edges */
  for (int y = 1; y < rows - 1; y++) {
    mvcur(begy + y, begx + cols, begy + y, begx);
    putp(GLYPH(BOX_VL, BOX_VL_FB));
    mvcur(begy + y, begx + 1, begy + y, begx + cols - 1);
    putp(GLYPH(BOX_VL, BOX_VL_FB));
  }

  /* Bottom row */
  mvcur(begy + rows - 2, begx + cols, begy + rows - 1, begx);
  putp(GLYPH(BOX_LL, BOX_LL_FB));
  for (int x = 1; x < cols - 1; x++) putp(GLYPH(BOX_HL, BOX_HL_FB));
  putp(GLYPH(BOX_LR, BOX_LR_FB));

  fflush(stdout);
}

/* ================================================================
   Layer 2: Color initialization
   ================================================================ */

static void initColors(void) {
  if (!has_colors()) return;
  start_color();
  use_default_colors();

  init_pair(NCTUI_PAIR_TITLE,       COLOR_WHITE,  COLOR_BLUE);
  init_pair(NCTUI_PAIR_HEADER,      COLOR_YELLOW, COLOR_BLACK);
  init_pair(NCTUI_PAIR_DATA,        COLOR_WHITE,  COLOR_BLACK);
  init_pair(NCTUI_PAIR_SELECTED,    COLOR_BLACK,  COLOR_CYAN);
  init_pair(NCTUI_PAIR_STATUS,      COLOR_WHITE,  COLOR_BLUE);
  init_pair(NCTUI_PAIR_SORT_HDR,    COLOR_WHITE,  COLOR_CYAN);
  init_pair(NCTUI_PAIR_FILTER_EDIT, COLOR_GREEN,  COLOR_WHITE);
  init_pair(NCTUI_PAIR_FILTER_SET,  COLOR_GREEN,  COLOR_BLACK);
  init_pair(NCTUI_PAIR_FILTER_EMPTY,COLOR_CYAN,   COLOR_BLACK);
  init_pair(NCTUI_PAIR_ALERT,       COLOR_WHITE,  COLOR_RED);
  init_pair(NCTUI_PAIR_CMD,         COLOR_WHITE,  COLOR_BLACK);
  init_pair(NCTUI_PAIR_BOX,         COLOR_WHITE,  COLOR_BLUE);
  init_pair(NCTUI_PAIR_INPUT,       COLOR_YELLOW, COLOR_BLACK);
}

/* ================================================================
   Layout constants
   ================================================================ */

#define TITLE_ROW    0
#define CMD_ROW      1
/* Filter row sits between command line and headers (like SDSF).
   When filter is inactive, headers move up to row 2. */
#define FILTER_ROW   2
#define HEADER_ROW_BASE 2

static int headerRow(NcTuiTable *tui) {
  return tui->filterActive ? 3 : 2;
}

static int dataStartRow(NcTuiTable *tui) {
  return tui->filterActive ? 4 : 3;
}

/* ================================================================
   Column geometry helpers
   ================================================================ */

static int hitTestColumn(NcTuiTable *tui, int screenX) {
  int xPos = 0;
  for (int c = 0; c < tui->fixedColumns && c < tui->numColumns; c++) {
    int w = tui->columns[c].width;
    if (screenX >= xPos && screenX < xPos + w) return c;
    xPos += w + 1;
  }
  for (int c = tui->scrollCol; c < tui->numColumns; c++) {
    int w = tui->columns[c].width;
    if (xPos + w > tui->screenCols) break;
    if (screenX >= xPos && screenX < xPos + w) return c;
    xPos += w + 1;
  }
  return -1;
}

static int computeColumnX(NcTuiTable *tui, int targetCol) {
  int xPos = 0;
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

/* ================================================================
   Init / Term
   ================================================================ */

int nctuiInit(NcTuiTable *tui) {
  /* ncurses init — setlocale needs ASCII string */
  setlocale(LC_ALL, nctuiToAscii(""));
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, FALSE);
  /* Enable mouse tracking ourselves — bypass ncurses's mouse subsystem
     entirely.  ncurses's mousemask()/getmouse() can't parse SGR mouse
     without kmous in terminfo, and define_key() conflicts with its
     internal mouse state.  We send DECSET directly and parse SGR events
     in resolveEscSeq(). */
  {
    /* DECSET 1000 = button tracking, 1006 = SGR extended format */
    static const char MOUSE_ON[] = {
      0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x30, 0x68,  /* ESC[?1000h */
      0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x36, 0x68,  /* ESC[?1006h */
      0
    };
    putp(MOUSE_ON);
    fflush(stdout);
  }

  initColors();
  probeUnicodeSupport();
  probeTerminalCapabilities();

  getmaxyx(stdscr, tui->screenRows, tui->screenCols);
  tui->dataRows = tui->screenRows - dataStartRow(tui) - 1; /* -1 for status */
  if (tui->dataRows < 1) tui->dataRows = 1;

  tui->selectedRow = -1;
  tui->sortColumn = -1;
  tui->sortAscending = 1;
  tui->filterActive = 0;
  tui->filterEditCol = -1;
  tui->filterEditPos = 0;
  tui->scrollRow = 0;
  tui->scrollCol = tui->fixedColumns;
  tui->cmdBuf[0] = '\0';
  tui->cmdPos = 0;
  tui->cmdActive = 1;
  tui->statusMsg[0] = '\0';

  tui->titleWin = NULL;
  tui->cmdWin = NULL;
  tui->headerWin = NULL;
  tui->filterWin = NULL;
  tui->dataWin = NULL;
  tui->statusWin = NULL;

  return 0;
}

void nctuiTerm(NcTuiTable *tui) {
  /* Disable mouse tracking before exiting */
  static const char MOUSE_OFF[] = {
    0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x36, 0x6C,  /* ESC[?1006l */
    0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x30, 0x6C,  /* ESC[?1000l */
    0
  };
  putp(MOUSE_OFF);
  fflush(stdout);
  endwin();
}

/* ================================================================
   Column setup
   ================================================================ */

void nctuiSetColumns(NcTuiTable *tui, NcTuiColumn *cols, int numCols,
                     int fixedCols) {
  tui->numColumns = numCols;
  tui->fixedColumns = fixedCols;
  for (int i = 0; i < numCols && i < NCTUI_MAX_COLUMNS; i++) {
    tui->columns[i] = cols[i];
  }
  tui->scrollCol = fixedCols;
}

void nctuiSetTitle(NcTuiTable *tui, const char *title) {
  strncpy(tui->title, title, sizeof(tui->title) - 1);
  tui->title[sizeof(tui->title) - 1] = '\0';
}

void nctuiSetStatus(NcTuiTable *tui, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tui->statusMsg, sizeof(tui->statusMsg), fmt, ap);
  va_end(ap);
}

/* ================================================================
   Rendering
   ================================================================ */

static void renderTitleBar(NcTuiTable *tui) {
  attron(COLOR_PAIR(NCTUI_PAIR_TITLE) | A_BOLD);
  nc_fillRow(stdscr, TITLE_ROW, tui->screenCols);
  nc_mvwaddstr(stdscr, TITLE_ROW, 1, tui->title);

  /* Right-aligned line counter */
  char right[80];
  int lastVisible = tui->scrollRow + tui->dataRows;
  if (lastVisible > tui->rowCount) lastVisible = tui->rowCount;
  snprintf(right, sizeof(right), "LINE %d-%d (%d)",
           tui->rowCount > 0 ? tui->scrollRow + 1 : 0,
           lastVisible, tui->rowCount);
  int rlen = (int)strlen(right);
  int rpos = tui->screenCols - rlen - 1;
  if (rpos > 0) nc_mvwaddstr(stdscr, TITLE_ROW, rpos, right);
  attroff(COLOR_PAIR(NCTUI_PAIR_TITLE) | A_BOLD);
}

static void renderCommandLine(NcTuiTable *tui) {
  move(CMD_ROW, 0);
  clrtoeol();
  attron(COLOR_PAIR(NCTUI_PAIR_CMD));
  nc_mvwaddstr(stdscr, CMD_ROW, 0, "COMMAND ===> ");
  nc_waddstr(stdscr, tui->cmdBuf);
  attroff(COLOR_PAIR(NCTUI_PAIR_CMD));
}

static int renderColumnRange(NcTuiTable *tui, int row,
                             int startCol, int endCol, int xPos,
                             int isHeader, int dataRow) {
  char buf[NCTUI_MAX_COL_WIDTH + 1];

  for (int c = startCol; c < endCol && c < tui->numColumns; c++) {
    NcTuiColumn *col = &tui->columns[c];
    int w = col->width;

    if (xPos + w > tui->screenCols) {
      w = tui->screenCols - xPos;
      if (w <= 0) break;
    }

    if (isHeader) {
      if (c == tui->sortColumn) {
        attron(COLOR_PAIR(NCTUI_PAIR_SORT_HDR) | A_BOLD);
        /* Reserve 2 chars for the sort glyph (UTF-8 triangle is 3 bytes
           but occupies 1 display column; we reserve 2 for spacing) */
        int nameW = w - 2;
        if (nameW < 1) nameW = 1;
        snprintf(buf, sizeof(buf), "%-*.*s", nameW, nameW, col->name);
      } else {
        snprintf(buf, sizeof(buf), "%-*.*s", w, w, col->name);
      }
    } else {
      buf[0] = '\0';
      if (tui->cellFormatter) {
        tui->cellFormatter(dataRow, c, buf, sizeof(buf), tui->userData);
      }
      if (col->align == NCTUI_ALIGN_RIGHT) {
        int slen = (int)strlen(buf);
        if (slen < w) {
          char tmp[NCTUI_MAX_COL_WIDTH + 1];
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

    nc_mvwaddstr(stdscr, row, xPos, buf);

    /* Emit sort glyph for sorted column header.
       Write through ncurses (not putp) to keep cursor tracking in sync. */
    if (isHeader && c == tui->sortColumn) {
      mvwaddstr(stdscr, row, xPos + w - 1,
                tui->sortAscending ? GLYPH(SORT_ASC, SORT_ASC_FB)
                                   : GLYPH(SORT_DESC, SORT_DESC_FB));
      attroff(COLOR_PAIR(NCTUI_PAIR_SORT_HDR) | A_BOLD);
      attron(COLOR_PAIR(NCTUI_PAIR_HEADER) | A_BOLD);
    }
    xPos += w + 1;
  }
  return xPos;
}

static void renderHeaders(NcTuiTable *tui) {
  int hRow = headerRow(tui);
  move(hRow, 0);
  clrtoeol();
  attron(COLOR_PAIR(NCTUI_PAIR_HEADER) | A_BOLD);

  int xPos = renderColumnRange(tui, hRow, 0, tui->fixedColumns, 0,
                               1, -1);
  renderColumnRange(tui, hRow, tui->scrollCol, tui->numColumns, xPos,
                    1, -1);
  attroff(COLOR_PAIR(NCTUI_PAIR_HEADER) | A_BOLD);
}

static void renderFilterField(NcTuiTable *tui, int col, int xPos) {
  int w = tui->columns[col].width;
  int hasFilter = (tui->columns[col].filter[0] != '\0');
  int isEditing = (col == tui->filterEditCol);

  char display[NCTUI_MAX_COL_WIDTH + 1];

  if (isEditing) {
    attron(COLOR_PAIR(NCTUI_PAIR_FILTER_EDIT) | A_REVERSE);
    snprintf(display, sizeof(display), "%-*.*s", w, w,
             tui->columns[col].filter);
    nc_mvwaddstr(stdscr, FILTER_ROW, xPos, display);
    attroff(COLOR_PAIR(NCTUI_PAIR_FILTER_EDIT) | A_REVERSE);
  } else if (hasFilter) {
    attron(COLOR_PAIR(NCTUI_PAIR_FILTER_SET) | A_BOLD);
    snprintf(display, sizeof(display), "%-*.*s", w, w,
             tui->columns[col].filter);
    nc_mvwaddstr(stdscr, FILTER_ROW, xPos, display);
    attroff(COLOR_PAIR(NCTUI_PAIR_FILTER_SET) | A_BOLD);
  } else {
    attron(COLOR_PAIR(NCTUI_PAIR_FILTER_EMPTY) | A_DIM);
    memset(display, '.', w);
    display[w] = '\0';
    nc_mvwaddstr(stdscr, FILTER_ROW, xPos, display);
    attroff(COLOR_PAIR(NCTUI_PAIR_FILTER_EMPTY) | A_DIM);
  }
}

static void renderFilterRow(NcTuiTable *tui) {
  if (!tui->filterActive) return;

  move(FILTER_ROW, 0);
  clrtoeol();

  int xPos = 0;
  for (int c = 0; c < tui->fixedColumns && c < tui->numColumns; c++) {
    if (xPos + tui->columns[c].width > tui->screenCols) break;
    /* Skip NP column — not filterable */
    if (tui->numActions > 0 && c == 0) {
      xPos += tui->columns[c].width + 1;
      continue;
    }
    renderFilterField(tui, c, xPos);
    xPos += tui->columns[c].width + 1;
  }
  for (int c = tui->scrollCol; c < tui->numColumns; c++) {
    if (xPos + tui->columns[c].width > tui->screenCols) break;
    renderFilterField(tui, c, xPos);
    xPos += tui->columns[c].width + 1;
  }
}

static void renderDataRows(NcTuiTable *tui) {
  int dStart = dataStartRow(tui);
  int isCursor;
  for (int i = 0; i < tui->dataRows; i++) {
    int dataRow = tui->scrollRow + i;
    int screenRow = dStart + i;

    move(screenRow, 0);
    clrtoeol();

    if (dataRow >= tui->rowCount) continue;

    /* Highlight: cursorRow gets SELECTED color, selectedRow for legacy compat */
    isCursor = (dataRow == tui->cursorRow || dataRow == tui->selectedRow);
    if (isCursor) {
      attron(COLOR_PAIR(NCTUI_PAIR_SELECTED));
    } else {
      attron(COLOR_PAIR(NCTUI_PAIR_DATA));
    }
    /* Fill entire row with spaces using the correct attribute.
       This ensures consistent column alignment — relying on clrtoeol
       alone can cause ncurses to skip writes for space-only columns. */
    nc_fillRow(stdscr, screenRow, tui->screenCols);
    move(screenRow, 0);

    /* Render columns first, then overlay NP glyph.
       The NP glyph (▶) is a 3-byte UTF-8 character occupying 1 display
       column.  Writing it via mvwaddstr before column rendering causes
       ncurses to mistrack the cursor (advances by byte count, not display
       width).  So we render columns first, then overlay the glyph via
       putp which bypasses ncurses cursor tracking entirely. */
    int xPos = renderColumnRange(tui, screenRow, 0, tui->fixedColumns, 0,
                                 0, dataRow);
    renderColumnRange(tui, screenRow, tui->scrollCol, tui->numColumns, xPos,
                      0, dataRow);

    /* NP column overlay — pending action char or cursor marker */
    if (tui->numActions > 0 && tui->selectMode != NCTUI_SELECT_NONE) {
      if (dataRow < NCTUI_MAX_ROWS && tui->npCommands[dataRow]) {
        /* Pending action character — single EBCDIC byte, safe for ncurses */
        char npBuf[6];
        npBuf[0] = tui->npCommands[dataRow];
        npBuf[1] = '\0';
        nc_mvwaddstr(stdscr, screenRow, 1, npBuf);
      } else if (isCursor) {
        /* Cursor marker glyph — write via putp to avoid byte-count
           cursor mistrack from the multi-byte UTF-8 glyph. */
        refresh();
        mvcur(-1, -1, screenRow, 1);
        putp(GLYPH(NP_CURSOR, NP_CURSOR_FB));
        fflush(stdout);
      }
    }

    if (isCursor) {
      attroff(COLOR_PAIR(NCTUI_PAIR_SELECTED));
    } else {
      attroff(COLOR_PAIR(NCTUI_PAIR_DATA));
    }
  }
}

static void renderStatusBar(NcTuiTable *tui) {
  char left[256];
  if (tui->statusMsg[0]) {
    snprintf(left, sizeof(left), "%s", tui->statusMsg);
  } else if (tui->filterActive && tui->filterEditCol >= 0) {
    snprintf(left, sizeof(left),
             "%d rows | Wildcards: * ? | Tab=Next Enter=Apply Esc=Done F6=Close",
             tui->rowCount);
  } else {
    /* Show filtered count if filtering is active */
    if (tui->totalRowCount > 0 && tui->rowCount < tui->totalRowCount) {
      snprintf(left, sizeof(left),
               "%d of %d rows | F3=Exit F5=Refresh F6=Filter PgUp/PgDn Sort",
               tui->rowCount, tui->totalRowCount);
    } else {
      snprintf(left, sizeof(left),
               "%d rows | F3=Exit F5=Refresh F6=Filter PgUp/PgDn Click header=Sort",
               tui->rowCount);
    }
  }

  attron(COLOR_PAIR(NCTUI_PAIR_STATUS) | A_BOLD);
  nc_fillRow(stdscr, tui->screenRows - 1, tui->screenCols);
  nc_mvwaddstr(stdscr, tui->screenRows - 1, 1, left);
  attroff(COLOR_PAIR(NCTUI_PAIR_STATUS) | A_BOLD);
}

void nctuiRender(NcTuiTable *tui) {
  getmaxyx(stdscr, tui->screenRows, tui->screenCols);
  tui->dataRows = tui->screenRows - dataStartRow(tui) - 1;
  if (tui->dataRows < 1) tui->dataRows = 1;

  /* Full clear — wipes any rogue printf output that bypassed ncurses */
  clear();

  curs_set(0);
  renderTitleBar(tui);
  renderCommandLine(tui);
  renderHeaders(tui);
  renderFilterRow(tui);
  renderDataRows(tui);
  renderStatusBar(tui);

  /* Position cursor based on focus area */
  if (tui->filterActive && tui->filterEditCol >= 0) {
    curs_set(1);
    int fx = computeColumnX(tui, tui->filterEditCol) + tui->filterEditPos;
    move(FILTER_ROW, fx);
  } else if (tui->focusArea == NCTUI_FOCUS_DATA) {
    curs_set(0);  /* hide cursor — highlight row serves as cursor */
  } else {
    curs_set(1);
    move(CMD_ROW, 13 + tui->cmdPos);
  }
  refresh();
}

/* Forward declarations for NP action helpers (defined after navigator code) */
static NcTuiAction *findAction(NcTuiTable *tui, char key);

/* ================================================================
   Event loop
   ================================================================ */

int nctuiEventLoop(NcTuiTable *tui) {
  nctuiRender(tui);

  while (1) {
    int rawCh = nctuiGetch();
    if (rawCh == ERR) continue;

    /* Convert to EBCDIC for printable chars, pass through special keys */
    int ch;
    if (rawCh >= KEY_MIN) {
      ch = rawCh;  /* ncurses special key — don't convert */
    } else {
      ch = nctuiKeyToEbcdic(rawCh);
    }

    /* F3 = exit */
    if (rawCh == KEY_F(3)) return 0;

    /* F5 = refresh */
    if (rawCh == KEY_F(5)) {
      if (tui->refreshHandler) {
        tui->rowCount = tui->refreshHandler(tui->userData);
      }
      tui->statusMsg[0] = '\0';
      nctuiRender(tui);
      continue;
    }

    /* F6 = toggle filter */
    if (rawCh == KEY_F(6)) {
      tui->filterActive = !tui->filterActive;
      if (tui->filterActive) {
        /* Skip NP column (col 0) when table has actions */
        int firstCol = (tui->numActions > 0) ? 1 : 0;
        tui->filterEditCol = firstCol;
        tui->filterEditPos = (int)strlen(tui->columns[firstCol].filter);
      } else {
        tui->filterEditCol = -1;
      }
      tui->dataRows = tui->screenRows - dataStartRow(tui) - 1;
      if (tui->dataRows < 1) tui->dataRows = 1;
      nctuiRender(tui);
      continue;
    }

    /* Mouse — KEY_MOUSE from define_key (SGR prefix) or NCTUI_SGR_MOUSE
       from resolveEscSeq fallback */
    if (rawCh == KEY_MOUSE || rawCh == NCTUI_SGR_MOUSE) {
      int mRow, mCol, mBtn, isPress;

      if (rawCh == KEY_MOUSE) {
        /* define_key consumed ESC[<, read remaining params */
        if (!readSGRMouseParams()) continue;
      }
      /* SGR data is in sgrMouse* globals */
      mRow    = sgrMouseRow;
      mCol    = sgrMouseCol;
      mBtn    = sgrMouseButton;
      isPress = sgrMousePress;

      tui->mouseRow = mRow;
      tui->mouseCol = mCol;
      tui->mouseButton = mBtn;

      /* Only act on press, ignore release */
      if (!isPress) continue;

      /* SGR button 0=left, 1=middle, 2=right, 64=scrollUp, 65=scrollDn */
      int isLeftClick = (mBtn == 0);
      int isScrollUp  = (mBtn == 64);
      int isScrollDn  = (mBtn == 65);

      /* Click on header — sort */
      if (isLeftClick && mRow == headerRow(tui)) {
        int col = hitTestColumn(tui, mCol);
        if (col >= 0) {
          if (col == tui->sortColumn) {
            tui->sortAscending = !tui->sortAscending;
          } else {
            tui->sortColumn = col;
            tui->sortAscending = 1;
          }
          if (tui->sortHandler) {
            tui->sortHandler(tui->sortColumn, tui->sortAscending,
                             tui->userData);
          }
        }
        nctuiRender(tui);
        continue;
      }

      /* Click on filter row */
      if (isLeftClick && tui->filterActive && mRow == FILTER_ROW) {
        int col = hitTestColumn(tui, mCol);
        int minCol = (tui->numActions > 0) ? 1 : 0;
        if (col >= minCol) {
          tui->filterEditCol = col;
          tui->filterEditPos =
            (int)strlen(tui->columns[col].filter);
        }
        nctuiRender(tui);
        continue;
      }

      /* Click on command line — focus command */
      if (isLeftClick && mRow == CMD_ROW) {
        tui->focusArea = NCTUI_FOCUS_COMMAND;
        nctuiRender(tui);
        continue;
      }

      /* Click on data row — select, focus data area */
      if (isLeftClick) {
        int dStart = dataStartRow(tui);
        int clickedRow = mRow - dStart;
        int dataRow = tui->scrollRow + clickedRow;
        if (clickedRow >= 0 && dataRow >= 0 && dataRow < tui->rowCount) {
          tui->selectedRow = dataRow;
          tui->focusArea = NCTUI_FOCUS_DATA;
          if (tui->selectHandler) {
            int rc = tui->selectHandler(dataRow, tui->userData);
            if (rc != 0) return rc;
          }
        }
        nctuiRender(tui);
        continue;
      }

      /* Scroll wheel */
      if (isScrollUp) {
        tui->scrollRow -= 3;
        if (tui->scrollRow < 0) tui->scrollRow = 0;
        nctuiRender(tui);
        continue;
      }
      if (isScrollDn) {
        tui->scrollRow += 3;
        if (tui->scrollRow + tui->dataRows > tui->rowCount)
          tui->scrollRow = tui->rowCount - tui->dataRows;
        if (tui->scrollRow < 0) tui->scrollRow = 0;
        nctuiRender(tui);
        continue;
      }
      continue;
    }

    /* Resize */
    if (rawCh == KEY_RESIZE) {
      getmaxyx(stdscr, tui->screenRows, tui->screenCols);
      tui->dataRows = tui->screenRows - dataStartRow(tui) - 1;
      if (tui->dataRows < 1) tui->dataRows = 1;
      clear();
      nctuiRender(tui);
      continue;
    }

    /* Filter field editing */
    if (tui->filterActive && tui->filterEditCol >= 0) {
      if (rawCh == ASCII_TAB || rawCh == KEY_STAB) {
        tui->filterEditCol++;
        if (tui->filterEditCol >= tui->numColumns) {
          tui->filterEditCol = -1;
        } else {
          tui->filterEditPos =
            (int)strlen(tui->columns[tui->filterEditCol].filter);
        }
        nctuiRender(tui);
        continue;
      }
      if (rawCh == KEY_BTAB) {
        int minCol = (tui->numActions > 0) ? 1 : 0;
        tui->filterEditCol--;
        if (tui->filterEditCol < minCol) {
          tui->filterEditCol = -1;
        } else {
          tui->filterEditPos =
            (int)strlen(tui->columns[tui->filterEditCol].filter);
        }
        nctuiRender(tui);
        continue;
      }
      if (rawCh == ASCII_LF || rawCh == ASCII_CR || rawCh == KEY_ENTER) {
        tui->filterEditCol = -1;
        if (tui->filterHandler) {
          tui->filterHandler(tui->userData);
        }
        nctuiRender(tui);
        continue;
      }
      if (rawCh == ASCII_ESC) {
        tui->filterEditCol = -1;
        nctuiRender(tui);
        continue;
      }
      if (rawCh == KEY_BACKSPACE || rawCh == ASCII_DEL || rawCh == ASCII_BS) {
        char *f = tui->columns[tui->filterEditCol].filter;
        if (tui->filterEditPos > 0) {
          f[--tui->filterEditPos] = '\0';
          if (tui->filterHandler) {
            tui->filterHandler(tui->userData);
          }
        }
        nctuiRender(tui);
        continue;
      }
      /* Printable character to filter — ch is EBCDIC */
      if (ch >= 0x40 && ch <= 0xFE) {
        char *f = tui->columns[tui->filterEditCol].filter;
        if (tui->filterEditPos < NCTUI_MAX_COL_WIDTH - 1) {
          f[tui->filterEditPos++] = (char)ch;
          f[tui->filterEditPos] = '\0';
          if (tui->filterHandler) {
            tui->filterHandler(tui->userData);
          }
        }
        nctuiRender(tui);
        continue;
      }
    }

    /* Tab — toggle focus between command line and data area */
    if (rawCh == '\t' || rawCh == KEY_BTAB) {
      if (tui->focusArea == NCTUI_FOCUS_COMMAND) {
        tui->focusArea = NCTUI_FOCUS_DATA;
      } else {
        tui->focusArea = NCTUI_FOCUS_COMMAND;
      }
      nctuiRender(tui);
      continue;
    }

    /* Navigation keys — arrow keys switch focus to data area */
    if (rawCh == KEY_UP) {
      tui->focusArea = NCTUI_FOCUS_DATA;
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->scrollRow;
      } else if (tui->selectedRow > 0) {
        tui->selectedRow--;
      }
      if (tui->selectedRow < tui->scrollRow) {
        tui->scrollRow = tui->selectedRow;
      }
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_DOWN) {
      tui->focusArea = NCTUI_FOCUS_DATA;
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->scrollRow;
      } else if (tui->selectedRow < tui->rowCount - 1) {
        tui->selectedRow++;
      }
      if (tui->selectedRow >= tui->scrollRow + tui->dataRows) {
        tui->scrollRow = tui->selectedRow - tui->dataRows + 1;
      }
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_PPAGE || rawCh == KEY_F(7)) {
      if (tui->selectedRow < 0) tui->selectedRow = tui->scrollRow;
      tui->selectedRow -= tui->dataRows;
      if (tui->selectedRow < 0) tui->selectedRow = 0;
      tui->scrollRow = tui->selectedRow;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_NPAGE || rawCh == KEY_F(8)) {
      if (tui->selectedRow < 0) tui->selectedRow = tui->scrollRow;
      tui->selectedRow += tui->dataRows;
      if (tui->selectedRow >= tui->rowCount)
        tui->selectedRow = tui->rowCount - 1;
      tui->scrollRow = tui->selectedRow;
      if (tui->scrollRow + tui->dataRows > tui->rowCount)
        tui->scrollRow = tui->rowCount - tui->dataRows;
      if (tui->scrollRow < 0) tui->scrollRow = 0;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_RIGHT) {
      if (tui->scrollCol + 1 < tui->numColumns) tui->scrollCol++;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_LEFT) {
      if (tui->scrollCol > tui->fixedColumns) tui->scrollCol--;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_HOME) {
      tui->selectedRow = 0;
      tui->scrollRow = 0;
      tui->scrollCol = tui->fixedColumns;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_END) {
      tui->selectedRow = tui->rowCount - 1;
      if (tui->selectedRow < 0) tui->selectedRow = 0;
      tui->scrollRow = tui->rowCount - tui->dataRows;
      if (tui->scrollRow < 0) tui->scrollRow = 0;
      nctuiRender(tui);
      continue;
    }

    /* Tab: cycle through data rows */
    if (rawCh == ASCII_TAB) {
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->scrollRow;
      } else if (tui->selectedRow < tui->rowCount - 1) {
        tui->selectedRow++;
        if (tui->selectedRow >= tui->scrollRow + tui->dataRows)
          tui->scrollRow = tui->selectedRow - tui->dataRows + 1;
      } else {
        tui->selectedRow = -1;
      }
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_BTAB) {
      if (tui->selectedRow < 0) {
        tui->selectedRow = tui->rowCount - 1;
        if (tui->selectedRow < 0) tui->selectedRow = 0;
        tui->scrollRow = tui->rowCount - tui->dataRows;
        if (tui->scrollRow < 0) tui->scrollRow = 0;
      } else if (tui->selectedRow > 0) {
        tui->selectedRow--;
        if (tui->selectedRow < tui->scrollRow)
          tui->scrollRow = tui->selectedRow;
      } else {
        tui->selectedRow = -1;
      }
      nctuiRender(tui);
      continue;
    }

    /* Enter */
    if (rawCh == ASCII_LF || rawCh == ASCII_CR || rawCh == KEY_ENTER) {
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
            nctuiSetStatus(tui, "Unknown column: %s", colName);
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
      nctuiRender(tui);
      continue;
    }

    /* Backspace */
    if (rawCh == KEY_BACKSPACE || rawCh == ASCII_DEL || rawCh == ASCII_BS) {
      if (tui->cmdPos > 0) {
        tui->cmdPos--;
        tui->cmdBuf[tui->cmdPos] = '\0';
      }
      nctuiRender(tui);
      continue;
    }

    /* Escape — return focus to command line */
    if (rawCh == ASCII_ESC) {
      tui->cmdBuf[0] = '\0';
      tui->cmdPos = 0;
      tui->statusMsg[0] = '\0';
      tui->focusArea = NCTUI_FOCUS_COMMAND;
      if (tui->npCount > 0) {
        for (int r = 0; r < NCTUI_MAX_ROWS; r++) tui->npCommands[r] = '\0';
        tui->npCount = 0;
      }
      nctuiRender(tui);
      continue;
    }

    /* Printable character — NP action or command line (EBCDIC) */
    if (ch >= 0x40 && ch <= 0xFE) {
      if (tui->focusArea == NCTUI_FOCUS_DATA && tui->numActions > 0) {
        unsigned char upper = (unsigned char)ch;
        /* EBCDIC uppercase: lowercase offset is 0x40 */
        if (upper >= 0x81 && upper <= 0x89) upper -= 0x40;
        else if (upper >= 0x91 && upper <= 0x99) upper -= 0x40;
        else if (upper >= 0xA2 && upper <= 0xA9) upper -= 0x40;
        NcTuiAction *act = findAction(tui, (char)upper);
        if (act) {
          int row = tui->cursorRow;
          if (row >= 0 && row < NCTUI_MAX_ROWS) {
            if (tui->npCommands[row] == (char)upper) {
              tui->npCommands[row] = '\0';
              tui->npCount--;
            } else {
              if (!tui->npCommands[row]) tui->npCount++;
              tui->npCommands[row] = (char)upper;
            }
            if (tui->cursorRow < tui->rowCount - 1) {
              tui->cursorRow++;
              tui->selectedRow = tui->cursorRow;
              int maxScroll = tui->cursorRow - tui->dataRows + 1;
              if (tui->scrollRow < maxScroll) tui->scrollRow = maxScroll;
            }
          }
          nctuiRender(tui);
          continue;
        }
      }
      /* Any other printable — goes to command line, switch focus */
      tui->focusArea = NCTUI_FOCUS_COMMAND;
      if (tui->cmdPos < (int)sizeof(tui->cmdBuf) - 1) {
        tui->cmdBuf[tui->cmdPos++] = (char)ch;
        tui->cmdBuf[tui->cmdPos] = '\0';
      }
      nctuiRender(tui);
      continue;
    }
  }

  return 0;
}

/* ================================================================
   Filter utilities
   ================================================================ */

void nctuiClearFilters(NcTuiTable *tui) {
  for (int i = 0; i < tui->numColumns; i++) {
    tui->columns[i].filter[0] = '\0';
  }
  tui->filterEditCol = -1;
  tui->filterEditPos = 0;
}

int nctuiMatchFilter(const char *pattern, const char *value) {
  if (!pattern || !pattern[0]) return 1;

  const char *p = pattern;
  const char *v = value;
  const char *starP = NULL;
  const char *starV = NULL;

  while (*v) {
    char pc = *p, vc = *v;
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

/* ================================================================
   Text viewer
   ================================================================ */

void nctuiTextViewShow(NcTuiTextView *tv) {
  getmaxyx(stdscr, tv->screenRows, tv->screenCols);
  tv->dataRows = tv->screenRows - 3;
  if (tv->dataRows < 1) tv->dataRows = 1;
  tv->scrollRow = 0;
  tv->scrollCol = 0;

  while (1) {
    getmaxyx(stdscr, tv->screenRows, tv->screenCols);
    tv->dataRows = tv->screenRows - 3;
    if (tv->dataRows < 1) tv->dataRows = 1;

    /* Full clear — wipes any rogue printf output that bypassed ncurses */
    clear();
    curs_set(0);

    /* Title bar */
    attron(COLOR_PAIR(NCTUI_PAIR_TITLE) | A_BOLD);
    nc_fillRow(stdscr, 0, tv->screenCols);
    nc_mvwaddstr(stdscr, 0, 1, tv->title);
    {
      char right[80];
      int lastVis = tv->scrollRow + tv->dataRows;
      if (lastVis > tv->lineCount) lastVis = tv->lineCount;
      snprintf(right, sizeof(right), "LINE %d-%d (%d)",
               tv->lineCount > 0 ? tv->scrollRow + 1 : 0,
               lastVis, tv->lineCount);
      int rlen = (int)strlen(right);
      int rpos = tv->screenCols - rlen - 1;
      if (rpos > 0) nc_mvwaddstr(stdscr, 0, rpos, right);
    }
    attroff(COLOR_PAIR(NCTUI_PAIR_TITLE) | A_BOLD);

    /* Column header */
    attron(COLOR_PAIR(NCTUI_PAIR_HEADER) | A_BOLD);
    move(1, 0);
    clrtoeol();
    nc_mvwaddstr(stdscr, 1, 0, "LINE   CONTENT");
    attroff(COLOR_PAIR(NCTUI_PAIR_HEADER) | A_BOLD);

    /* Data rows */
    for (int i = 0; i < tv->dataRows; i++) {
      int lineIdx = tv->scrollRow + i;
      int screenRow = 2 + i;

      move(screenRow, 0);
      clrtoeol();

      if (lineIdx < tv->lineCount && tv->lines[lineIdx]) {
        char numBuf[16];
        snprintf(numBuf, sizeof(numBuf), "%6d ", lineIdx + 1);
        attron(COLOR_PAIR(NCTUI_PAIR_FILTER_EMPTY));
        nc_mvwaddstr(stdscr, screenRow, 0, numBuf);
        attroff(COLOR_PAIR(NCTUI_PAIR_FILTER_EMPTY));

        const char *line = tv->lines[lineIdx];
        int lineLen = (int)strlen(line);
        int avail = tv->screenCols - 7;
        if (avail < 0) avail = 0;
        if (tv->scrollCol < lineLen) {
          char lineBuf[512];
          int showLen = lineLen - tv->scrollCol;
          if (showLen > avail) showLen = avail;
          if (showLen > (int)sizeof(lineBuf) - 1)
            showLen = (int)sizeof(lineBuf) - 1;
          memcpy(lineBuf, line + tv->scrollCol, showLen);
          lineBuf[showLen] = '\0';
          nc_mvwaddstr(stdscr, screenRow, 7, lineBuf);
        }
      }
    }

    /* Status bar */
    attron(COLOR_PAIR(NCTUI_PAIR_STATUS) | A_BOLD);
    nc_fillRow(stdscr, tv->screenRows - 1, tv->screenCols);
    nc_mvwaddstr(stdscr, tv->screenRows - 1, 1,
                 "F3=Back  PgUp/PgDn  Up/Down  Left/Right=Scroll  Home/End");
    attroff(COLOR_PAIR(NCTUI_PAIR_STATUS) | A_BOLD);

    refresh();

    int rawKey = nctuiGetch();
    int key = nctuiKeyToEbcdic(rawKey);

    if (rawKey == KEY_F(3) || rawKey == ASCII_ESC) break;
    if (key == 'q' || key == 'Q') break;

    if (rawKey == KEY_UP) {
      if (tv->scrollRow > 0) tv->scrollRow--;
    } else if (rawKey == KEY_DOWN) {
      if (tv->scrollRow + tv->dataRows < tv->lineCount) tv->scrollRow++;
    } else if (rawKey == KEY_PPAGE || rawKey == KEY_F(7)) {
      tv->scrollRow -= tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    } else if (rawKey == KEY_NPAGE || rawKey == KEY_F(8)) {
      tv->scrollRow += tv->dataRows;
      if (tv->scrollRow + tv->dataRows > tv->lineCount)
        tv->scrollRow = tv->lineCount - tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    } else if (rawKey == KEY_LEFT) {
      if (tv->scrollCol > 0) tv->scrollCol--;
    } else if (rawKey == KEY_RIGHT) {
      tv->scrollCol++;
    } else if (rawKey == KEY_HOME) {
      tv->scrollRow = 0;
      tv->scrollCol = 0;
    } else if (rawKey == KEY_END) {
      tv->scrollRow = tv->lineCount - tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    }
  }
}

/* ================================================================
   Modal dialogs
   ================================================================ */

int nctuiDialogInput(const char *title, const char *prompt,
                     char *buf, int bufLen) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int dw = 50;
  if (dw > cols - 4) dw = cols - 4;
  int dh = 8;
  int dy = (rows - dh) / 2;
  int dx = (cols - dw) / 2;

  WINDOW *dialog = newwin(dh, dw, dy, dx);
  PANEL *panel = new_panel(dialog);

  wbkgd(dialog, COLOR_PAIR(NCTUI_PAIR_BOX));
  werase(dialog);

  /* Content */
  wattron(dialog, COLOR_PAIR(NCTUI_PAIR_BOX));
  nc_mvwaddstr(dialog, 1, 2, prompt);

  /* Input field area */
  int fieldW = dw - 4;
  int pos = (int)strlen(buf);

  int result = 0;

  update_panels();
  doupdate();
  nctuiDrawBox(dialog);

  /* Title overlay on top border */
  {
    char titleBuf[128];
    snprintf(titleBuf, sizeof(titleBuf), " %s ", title);
    int tlen = (int)strlen(titleBuf);
    int tx = (dw - tlen) / 2;
    if (tx < 1) tx = 1;
    /* Position on the top border row and write title */
    mvcur(-1, -1, dy, dx + tx);
    putp(nctuiToAscii(titleBuf));
    fflush(stdout);
  }

  keypad(dialog, TRUE);

  while (1) {
    /* Draw current input */
    wattron(dialog, COLOR_PAIR(NCTUI_PAIR_INPUT) | A_UNDERLINE);
    {
      char display[256];
      memset(display, ' ', fieldW);
      display[fieldW] = '\0';
      int slen = (int)strlen(buf);
      if (slen > fieldW) slen = fieldW;
      memcpy(display, buf, slen);
      nc_mvwaddstr(dialog, 3, 2, display);
    }
    wattroff(dialog, COLOR_PAIR(NCTUI_PAIR_INPUT) | A_UNDERLINE);

    /* Status line inside dialog */
    wattron(dialog, COLOR_PAIR(NCTUI_PAIR_BOX));
    nc_mvwaddstr(dialog, 5, 2, "Enter=OK  Esc=Cancel");
    wattroff(dialog, COLOR_PAIR(NCTUI_PAIR_BOX));

    /* Show cursor in input field */
    wmove(dialog, 3, 2 + pos);
    curs_set(1);
    update_panels();
    doupdate();

    int rawCh = wgetch(dialog);
    int ch = nctuiKeyToEbcdic(rawCh);

    if (rawCh == ASCII_ESC) {
      result = 0;
      break;
    }
    if (rawCh == KEY_F(3)) {
      result = 0;
      break;
    }
    if (rawCh == ASCII_LF || rawCh == ASCII_CR || rawCh == KEY_ENTER) {
      result = 1;
      break;
    }
    if (rawCh == KEY_BACKSPACE || rawCh == ASCII_DEL || rawCh == ASCII_BS) {
      if (pos > 0) {
        buf[--pos] = '\0';
      }
      continue;
    }
    if (rawCh == KEY_LEFT) {
      if (pos > 0) pos--;
      continue;
    }
    if (rawCh == KEY_RIGHT) {
      if (pos < (int)strlen(buf)) pos++;
      continue;
    }

    /* Printable (EBCDIC) */
    if (ch >= 0x40 && ch <= 0xFE) {
      if (pos < bufLen - 1) {
        buf[pos++] = (char)ch;
        buf[pos] = '\0';
      }
    }
  }

  del_panel(panel);
  delwin(dialog);
  update_panels();
  doupdate();

  return result;
}

int nctuiDialogConfirm(const char *title, const char *message) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int dw = 44;
  if (dw > cols - 4) dw = cols - 4;
  int dh = 7;
  int dy = (rows - dh) / 2;
  int dx = (cols - dw) / 2;

  WINDOW *dialog = newwin(dh, dw, dy, dx);
  PANEL *panel = new_panel(dialog);

  wbkgd(dialog, COLOR_PAIR(NCTUI_PAIR_BOX));
  werase(dialog);

  wattron(dialog, COLOR_PAIR(NCTUI_PAIR_BOX));
  nc_mvwaddstr(dialog, 2, 2, message);
  nc_mvwaddstr(dialog, 4, 2, "Y=Yes  N=No  Esc=Cancel");
  wattroff(dialog, COLOR_PAIR(NCTUI_PAIR_BOX));

  update_panels();
  doupdate();
  nctuiDrawBox(dialog);

  /* Title on border */
  {
    char titleBuf[128];
    snprintf(titleBuf, sizeof(titleBuf), " %s ", title);
    int tlen = (int)strlen(titleBuf);
    int tx = (dw - tlen) / 2;
    if (tx < 1) tx = 1;
    mvcur(-1, -1, dy, dx + tx);
    putp(nctuiToAscii(titleBuf));
    fflush(stdout);
  }

  keypad(dialog, TRUE);
  curs_set(0);

  int result = 0;
  while (1) {
    int rawCh = wgetch(dialog);
    int ch = nctuiKeyToEbcdic(rawCh);

    if (rawCh == ASCII_ESC || rawCh == KEY_F(3)) { result = 0; break; }
    if (ch == 'y' || ch == 'Y') { result = 1; break; }
    if (ch == 'n' || ch == 'N') { result = 0; break; }
    if (rawCh == ASCII_LF || rawCh == ASCII_CR || rawCh == KEY_ENTER) {
      result = 1;
      break;
    }
  }

  del_panel(panel);
  delwin(dialog);
  update_panels();
  doupdate();

  return result;
}

/* ================================================================
   NP column action registry
   ================================================================ */

void nctuiAddAction(NcTuiTable *tui, char key, const char *label,
                    int flags, NcTuiActionHandler handler) {
  if (tui->numActions >= NCTUI_MAX_ACTIONS) return;
  NcTuiAction *a = &tui->actions[tui->numActions++];
  a->key = key;
  strncpy(a->label, label, sizeof(a->label) - 1);
  a->label[sizeof(a->label) - 1] = '\0';
  a->flags = flags;
  a->handler = handler;
}

static NcTuiAction *findAction(NcTuiTable *tui, char key) {
  for (int i = 0; i < tui->numActions; i++) {
    if (tui->actions[i].key == key) return &tui->actions[i];
  }
  return NULL;
}

static NcTuiAction *findDefaultAction(NcTuiTable *tui) {
  for (int i = 0; i < tui->numActions; i++) {
    if (tui->actions[i].flags & NCTUI_ACTION_DEFAULT) return &tui->actions[i];
  }
  return NULL;
}

/* Dispatch all pending NP commands.  Returns 0 normally. */
static int dispatchNpCommands(NcTuiTable *tui) {
  for (int r = 0; r < tui->rowCount && r < NCTUI_MAX_ROWS; r++) {
    char cmd = tui->npCommands[r];
    if (!cmd) continue;

    NcTuiAction *act = findAction(tui, cmd);
    if (act && act->handler) {
      act->handler(r, tui->userData, tui->nav);
    } else {
      nctuiSetStatus(tui, "Unknown action: %c", cmd);
    }
    tui->npCommands[r] = '\0';
  }
  tui->npCount = 0;
  return 0;
}

/* ================================================================
   Page helpers
   ================================================================ */

void nctuiPageInitTable(NcTuiPage *page, const char *breadcrumb) {
  memset(page, 0, sizeof(*page));
  page->type = NCTUI_PAGE_TABLE;
  strncpy(page->breadcrumb, breadcrumb, sizeof(page->breadcrumb) - 1);
  page->table.selectedRow = -1;
  page->table.cursorRow = 0;
  page->table.sortColumn = -1;
  page->table.sortAscending = 1;
  page->table.selectMode = NCTUI_SELECT_SINGLE;
  page->table.cmdActive = 1;
}

void nctuiPageInitTextView(NcTuiPage *page, const char *breadcrumb,
                           char **lines, int lineCount) {
  memset(page, 0, sizeof(*page));
  page->type = NCTUI_PAGE_TEXTVIEW;
  strncpy(page->breadcrumb, breadcrumb, sizeof(page->breadcrumb) - 1);
  page->textView.lines = lines;
  page->textView.lineCount = lineCount;
}

/* ================================================================
   Navigator — page stack management
   ================================================================ */

int nctuiNavInit(NcTuiNav *nav) {
  memset(nav, 0, sizeof(*nav));
  nav->depth = -1;
  nav->running = 1;

  /* ncurses init */
  setlocale(LC_ALL, nctuiToAscii(""));
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, FALSE);
  /* Enable mouse tracking ourselves — bypass ncurses's mouse subsystem
     entirely.  ncurses's mousemask()/getmouse() can't parse SGR mouse
     without kmous in terminfo, and define_key() conflicts with its
     internal mouse state.  We send DECSET directly and parse SGR events
     in resolveEscSeq(). */
  {
    /* DECSET 1000 = button tracking, 1006 = SGR extended format */
    static const char MOUSE_ON[] = {
      0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x30, 0x68,  /* ESC[?1000h */
      0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x36, 0x68,  /* ESC[?1006h */
      0
    };
    putp(MOUSE_ON);
    fflush(stdout);
  }

  initColors();
  probeUnicodeSupport();
  probeTerminalCapabilities();

  getmaxyx(stdscr, nav->screenRows, nav->screenCols);
  return 0;
}

int nctuiNavPush(NcTuiNav *nav, NcTuiPage *page) {
  if (nav->depth + 1 >= NCTUI_MAX_STACK) return -1;
  nav->depth++;
  nav->stack[nav->depth] = *page;  /* copy */

  if (nav->stack[nav->depth].type == NCTUI_PAGE_TABLE) {
    NcTuiTable *tui = &nav->stack[nav->depth].table;
    tui->nav = nav;
    tui->screenRows = nav->screenRows;
    tui->screenCols = nav->screenCols;
    tui->dataRows = tui->screenRows - dataStartRow(tui) - 1;
    if (tui->dataRows < 1) tui->dataRows = 1;
    tui->scrollCol = tui->fixedColumns;
  }
  return 0;
}

void nctuiNavPop(NcTuiNav *nav) {
  if (nav->depth < 0) {
    nav->running = 0;
    return;
  }
  nav->depth--;
  if (nav->depth < 0) {
    nav->running = 0;
  }
}

/* ================================================================
   Navigator — table event loop (runs while page is on top)
   ================================================================ */

static void navRunTable(NcTuiNav *nav) {
  NcTuiTable *tui = &nav->stack[nav->depth].table;

  /* Ensure geometry is current */
  getmaxyx(stdscr, tui->screenRows, tui->screenCols);
  nav->screenRows = tui->screenRows;
  nav->screenCols = tui->screenCols;
  tui->dataRows = tui->screenRows - dataStartRow(tui) - 1;
  if (tui->dataRows < 1) tui->dataRows = 1;

  nctuiRender(tui);

  while (nav->running) {
    int rawCh = nctuiGetch();
    if (rawCh == ERR) continue;

    int ch;
    if (rawCh >= KEY_MIN) {
      ch = rawCh;
    } else {
      ch = nctuiKeyToEbcdic(rawCh);
    }

    /* F3 = pop (back) */
    if (rawCh == KEY_F(3)) {
      nctuiNavPop(nav);
      return;
    }

    /* F5 = refresh */
    if (rawCh == KEY_F(5)) {
      if (tui->refreshHandler) {
        tui->rowCount = tui->refreshHandler(tui->userData);
      }
      tui->statusMsg[0] = '\0';
      nctuiRender(tui);
      continue;
    }

    /* F6 = toggle filter */
    if (rawCh == KEY_F(6)) {
      tui->filterActive = !tui->filterActive;
      if (tui->filterActive) {
        /* Skip NP column (col 0) when table has actions */
        int firstCol = (tui->numActions > 0) ? 1 : 0;
        tui->filterEditCol = firstCol;
        tui->filterEditPos = (int)strlen(tui->columns[firstCol].filter);
      } else {
        tui->filterEditCol = -1;
      }
      tui->dataRows = tui->screenRows - dataStartRow(tui) - 1;
      if (tui->dataRows < 1) tui->dataRows = 1;
      nctuiRender(tui);
      continue;
    }

    /* Mouse — KEY_MOUSE from define_key (SGR prefix) or NCTUI_SGR_MOUSE
       from resolveEscSeq fallback */
    if (rawCh == KEY_MOUSE || rawCh == NCTUI_SGR_MOUSE) {
      int mRow, mCol, mBtn, isPress;

      if (rawCh == KEY_MOUSE) {
        /* define_key consumed ESC[<, read remaining params */
        if (!readSGRMouseParams()) continue;
      }
      /* SGR data is in sgrMouse* globals */
      mRow    = sgrMouseRow;
      mCol    = sgrMouseCol;
      mBtn    = sgrMouseButton;
      isPress = sgrMousePress;

      tui->mouseRow = mRow;
      tui->mouseCol = mCol;
      tui->mouseButton = mBtn;

      /* Only act on press, ignore release */
      if (!isPress) continue;

      /* SGR button 0=left, 1=middle, 2=right, 64=scrollUp, 65=scrollDn */
      int isLeftClick = (mBtn == 0);
      int isScrollUp  = (mBtn == 64);
      int isScrollDn  = (mBtn == 65);

      /* Click on header — sort */
      if (isLeftClick && mRow == headerRow(tui)) {
        int col = hitTestColumn(tui, mCol);
        if (col >= 0) {
          if (col == tui->sortColumn) {
            tui->sortAscending = !tui->sortAscending;
          } else {
            tui->sortColumn = col;
            tui->sortAscending = 1;
          }
          if (tui->sortHandler) {
            tui->sortHandler(tui->sortColumn, tui->sortAscending,
                             tui->userData);
          }
        }
        nctuiRender(tui);
        continue;
      }

      /* Click on filter row */
      if (isLeftClick && tui->filterActive && mRow == FILTER_ROW) {
        int col = hitTestColumn(tui, mCol);
        int minCol = (tui->numActions > 0) ? 1 : 0;
        if (col >= minCol) {
          tui->filterEditCol = col;
          tui->filterEditPos =
            (int)strlen(tui->columns[col].filter);
        }
        nctuiRender(tui);
        continue;
      }

      /* Click on command line — focus command */
      if (isLeftClick && mRow == CMD_ROW) {
        tui->focusArea = NCTUI_FOCUS_COMMAND;
        nctuiRender(tui);
        continue;
      }

      /* Click on data row — move cursor, focus data area */
      if (isLeftClick) {
        int dStart = dataStartRow(tui);
        int clickedRow = mRow - dStart;
        int dataRow = tui->scrollRow + clickedRow;
        if (clickedRow >= 0 && dataRow >= 0 && dataRow < tui->rowCount) {
          tui->cursorRow = dataRow;
          tui->selectedRow = dataRow;
          tui->focusArea = NCTUI_FOCUS_DATA;
        }
        nctuiRender(tui);
        continue;
      }

      /* Scroll wheel */
      if (isScrollUp) {
        tui->scrollRow -= 3;
        if (tui->scrollRow < 0) tui->scrollRow = 0;
        nctuiRender(tui);
        continue;
      }
      if (isScrollDn) {
        tui->scrollRow += 3;
        if (tui->scrollRow + tui->dataRows > tui->rowCount)
          tui->scrollRow = tui->rowCount - tui->dataRows;
        if (tui->scrollRow < 0) tui->scrollRow = 0;
        nctuiRender(tui);
        continue;
      }
      continue;
    }

    /* Resize */
    if (rawCh == KEY_RESIZE) {
      getmaxyx(stdscr, tui->screenRows, tui->screenCols);
      nav->screenRows = tui->screenRows;
      nav->screenCols = tui->screenCols;
      tui->dataRows = tui->screenRows - dataStartRow(tui) - 1;
      if (tui->dataRows < 1) tui->dataRows = 1;
      clear();
      nctuiRender(tui);
      continue;
    }

    /* Filter field editing */
    if (tui->filterActive && tui->filterEditCol >= 0) {
      if (rawCh == ASCII_TAB || rawCh == KEY_STAB) {
        tui->filterEditCol++;
        if (tui->filterEditCol >= tui->numColumns) {
          tui->filterEditCol = -1;
        } else {
          tui->filterEditPos =
            (int)strlen(tui->columns[tui->filterEditCol].filter);
        }
        nctuiRender(tui);
        continue;
      }
      if (rawCh == KEY_BTAB) {
        int minCol = (tui->numActions > 0) ? 1 : 0;
        tui->filterEditCol--;
        if (tui->filterEditCol < minCol) tui->filterEditCol = -1;
        else tui->filterEditPos =
          (int)strlen(tui->columns[tui->filterEditCol].filter);
        nctuiRender(tui);
        continue;
      }
      if (rawCh == ASCII_LF || rawCh == ASCII_CR || rawCh == KEY_ENTER) {
        tui->filterEditCol = -1;
        if (tui->filterHandler) tui->filterHandler(tui->userData);
        nctuiRender(tui);
        continue;
      }
      if (rawCh == ASCII_ESC) {
        tui->filterEditCol = -1;
        nctuiRender(tui);
        continue;
      }
      if (rawCh == KEY_BACKSPACE || rawCh == ASCII_DEL || rawCh == ASCII_BS) {
        char *f = tui->columns[tui->filterEditCol].filter;
        if (tui->filterEditPos > 0) {
          f[--tui->filterEditPos] = '\0';
          if (tui->filterHandler) tui->filterHandler(tui->userData);
        }
        nctuiRender(tui);
        continue;
      }
      if (ch >= 0x40 && ch <= 0xFE) {
        char *f = tui->columns[tui->filterEditCol].filter;
        if (tui->filterEditPos < NCTUI_MAX_COL_WIDTH - 1) {
          f[tui->filterEditPos++] = (char)ch;
          f[tui->filterEditPos] = '\0';
          if (tui->filterHandler) tui->filterHandler(tui->userData);
        }
        nctuiRender(tui);
        continue;
      }
    }

    /* Tab — toggle focus between command line and data area */
    if (rawCh == '\t' || rawCh == KEY_BTAB) {
      if (tui->focusArea == NCTUI_FOCUS_COMMAND) {
        tui->focusArea = NCTUI_FOCUS_DATA;
      } else {
        tui->focusArea = NCTUI_FOCUS_COMMAND;
      }
      nctuiRender(tui);
      continue;
    }

    /* Navigation keys — arrow keys switch focus to data area */
    if (rawCh == KEY_UP) {
      tui->focusArea = NCTUI_FOCUS_DATA;
      if (tui->cursorRow > 0) tui->cursorRow--;
      tui->selectedRow = tui->cursorRow;
      if (tui->cursorRow < tui->scrollRow) tui->scrollRow = tui->cursorRow;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_DOWN) {
      tui->focusArea = NCTUI_FOCUS_DATA;
      if (tui->cursorRow < tui->rowCount - 1) tui->cursorRow++;
      tui->selectedRow = tui->cursorRow;
      if (tui->cursorRow >= tui->scrollRow + tui->dataRows)
        tui->scrollRow = tui->cursorRow - tui->dataRows + 1;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_PPAGE || rawCh == KEY_F(7)) {
      tui->cursorRow -= tui->dataRows;
      if (tui->cursorRow < 0) tui->cursorRow = 0;
      tui->selectedRow = tui->cursorRow;
      tui->scrollRow = tui->cursorRow;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_NPAGE || rawCh == KEY_F(8)) {
      tui->cursorRow += tui->dataRows;
      if (tui->cursorRow >= tui->rowCount)
        tui->cursorRow = tui->rowCount - 1;
      if (tui->cursorRow < 0) tui->cursorRow = 0;
      tui->selectedRow = tui->cursorRow;
      tui->scrollRow = tui->cursorRow;
      if (tui->scrollRow + tui->dataRows > tui->rowCount)
        tui->scrollRow = tui->rowCount - tui->dataRows;
      if (tui->scrollRow < 0) tui->scrollRow = 0;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_RIGHT) {
      if (tui->scrollCol + 1 < tui->numColumns) tui->scrollCol++;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_LEFT) {
      if (tui->scrollCol > tui->fixedColumns) tui->scrollCol--;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_HOME) {
      tui->cursorRow = 0;
      tui->selectedRow = 0;
      tui->scrollRow = 0;
      tui->scrollCol = tui->fixedColumns;
      nctuiRender(tui);
      continue;
    }
    if (rawCh == KEY_END) {
      tui->cursorRow = tui->rowCount - 1;
      if (tui->cursorRow < 0) tui->cursorRow = 0;
      tui->selectedRow = tui->cursorRow;
      tui->scrollRow = tui->rowCount - tui->dataRows;
      if (tui->scrollRow < 0) tui->scrollRow = 0;
      nctuiRender(tui);
      continue;
    }

    /* Enter — dispatch NP commands or default action */
    if (rawCh == ASCII_LF || rawCh == ASCII_CR || rawCh == KEY_ENTER) {
      if (tui->cmdBuf[0]) {
        /* SORT command */
        if ((tui->cmdBuf[0] == 'S' || tui->cmdBuf[0] == 's') &&
            (tui->cmdBuf[1] == 'O' || tui->cmdBuf[1] == 'o') &&
            (tui->cmdBuf[2] == 'R' || tui->cmdBuf[2] == 'r') &&
            (tui->cmdBuf[3] == 'T' || tui->cmdBuf[3] == 't') &&
            tui->cmdBuf[4] == ' ') {
          const char *colName = tui->cmdBuf + 5;
          while (*colName == ' ') colName++;
          int found = 0;
          for (int i = 0; i < tui->numColumns; i++) {
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
              if (i == tui->sortColumn) tui->sortAscending = !tui->sortAscending;
              else { tui->sortColumn = i; tui->sortAscending = 1; }
              if (tui->sortHandler)
                tui->sortHandler(tui->sortColumn, tui->sortAscending, tui->userData);
              found = 1;
              break;
            }
          }
          if (!found) nctuiSetStatus(tui, "Unknown column: %s", colName);
          tui->cmdBuf[0] = '\0';
          tui->cmdPos = 0;
        } else if (tui->commandHandler) {
          int depthBefore = nav->depth;
          tui->commandHandler(tui->cmdBuf, tui->userData);
          tui->cmdBuf[0] = '\0';
          tui->cmdPos = 0;
          if (nav->depth != depthBefore) {
            return;  /* command handler pushed a page */
          }
        }
      } else if (tui->npCount > 0) {
        /* Dispatch all pending NP commands */
        dispatchNpCommands(tui);
      } else {
        /* No NP commands, no command line — invoke default action on cursor row */
        NcTuiAction *def = findDefaultAction(tui);
        if (def && def->handler) {
          int depthBefore = nav->depth;
          def->handler(tui->cursorRow, tui->userData, nav);
          if (nav->depth != depthBefore) {
            return;  /* a new page was pushed — exit this loop */
          }
        } else if (tui->selectHandler) {
          int depthBefore = nav->depth;
          tui->selectHandler(tui->cursorRow, tui->userData);
          if (nav->depth != depthBefore) {
            return;  /* select handler pushed a page */
          }
        }
      }
      nctuiRender(tui);
      continue;
    }

    /* Backspace */
    if (rawCh == KEY_BACKSPACE || rawCh == ASCII_DEL || rawCh == ASCII_BS) {
      if (tui->cmdPos > 0) {
        tui->cmdPos--;
        tui->cmdBuf[tui->cmdPos] = '\0';
      }
      nctuiRender(tui);
      continue;
    }

    /* Escape — clear command/status/NP, return focus to command line */
    if (rawCh == ASCII_ESC) {
      tui->cmdBuf[0] = '\0';
      tui->cmdPos = 0;
      tui->statusMsg[0] = '\0';
      tui->focusArea = NCTUI_FOCUS_COMMAND;
      /* Clear all pending NP commands */
      if (tui->npCount > 0) {
        for (int r = 0; r < NCTUI_MAX_ROWS; r++) tui->npCommands[r] = '\0';
        tui->npCount = 0;
      }
      nctuiRender(tui);
      continue;
    }

    /* Printable — NP action or command line (EBCDIC) */
    if (ch >= 0x40 && ch <= 0xFE) {
      /* When focus is on command line, all printable chars go to cmdBuf.
         NP actions only fire when focus is on the data area. */
      if (tui->focusArea == NCTUI_FOCUS_DATA && tui->numActions > 0) {
        unsigned char upper = (unsigned char)ch;
        /* EBCDIC: uppercase is higher than lowercase (offset +0x40) */
        if (upper >= 0x81 && upper <= 0x89) upper += 0x40;       /* a-i -> A-I */
        else if (upper >= 0x91 && upper <= 0x99) upper += 0x40;  /* j-r -> J-R */
        else if (upper >= 0xA2 && upper <= 0xA9) upper += 0x40;  /* s-z -> S-Z */
        NcTuiAction *act = findAction(tui, (char)upper);
        if (act) {
          int row = tui->cursorRow;
          if (row >= 0 && row < NCTUI_MAX_ROWS) {
            if (tui->npCommands[row] == (char)upper) {
              /* Toggle off if same action typed again */
              tui->npCommands[row] = '\0';
              tui->npCount--;
            } else {
              if (!tui->npCommands[row]) tui->npCount++;
              tui->npCommands[row] = (char)upper;
            }
            /* Advance cursor down */
            if (tui->cursorRow < tui->rowCount - 1) {
              tui->cursorRow++;
              tui->selectedRow = tui->cursorRow;
              int maxScroll = tui->cursorRow - tui->dataRows + 1;
              if (tui->scrollRow < maxScroll) tui->scrollRow = maxScroll;
            }
          }
          nctuiRender(tui);
          continue;
        }
      }
      /* Any other printable — goes to command line, switch focus */
      tui->focusArea = NCTUI_FOCUS_COMMAND;
      if (tui->cmdPos < (int)sizeof(tui->cmdBuf) - 1) {
        tui->cmdBuf[tui->cmdPos++] = (char)ch;
        tui->cmdBuf[tui->cmdPos] = '\0';
      }
      nctuiRender(tui);
      continue;
    }
  }
}

/* ================================================================
   Navigator — text viewer event loop
   ================================================================ */

static void navRunTextView(NcTuiNav *nav) {
  NcTuiTextView *tv = &nav->stack[nav->depth].textView;

  getmaxyx(stdscr, tv->screenRows, tv->screenCols);
  tv->dataRows = tv->screenRows - 3;
  if (tv->dataRows < 1) tv->dataRows = 1;

  /* Render + event loop (reuses nctuiTextViewShow logic inline) */
  while (nav->running) {
    getmaxyx(stdscr, tv->screenRows, tv->screenCols);
    tv->dataRows = tv->screenRows - 3;
    if (tv->dataRows < 1) tv->dataRows = 1;

    clear();
    curs_set(0);

    /* Title bar with breadcrumb */
    attron(COLOR_PAIR(NCTUI_PAIR_TITLE) | A_BOLD);
    nc_fillRow(stdscr, 0, tv->screenCols);
    nc_mvwaddstr(stdscr, 0, 1, tv->title);
    {
      char right[80];
      int lastVis = tv->scrollRow + tv->dataRows;
      if (lastVis > tv->lineCount) lastVis = tv->lineCount;
      snprintf(right, sizeof(right), "LINE %d-%d (%d)",
               tv->lineCount > 0 ? tv->scrollRow + 1 : 0,
               lastVis, tv->lineCount);
      int rlen = (int)strlen(right);
      int rpos = tv->screenCols - rlen - 1;
      if (rpos > 0) nc_mvwaddstr(stdscr, 0, rpos, right);
    }
    attroff(COLOR_PAIR(NCTUI_PAIR_TITLE) | A_BOLD);

    /* Header */
    attron(COLOR_PAIR(NCTUI_PAIR_HEADER) | A_BOLD);
    move(1, 0);
    clrtoeol();
    nc_mvwaddstr(stdscr, 1, 0, "LINE   CONTENT");
    attroff(COLOR_PAIR(NCTUI_PAIR_HEADER) | A_BOLD);

    /* Data */
    for (int i = 0; i < tv->dataRows; i++) {
      int lineIdx = tv->scrollRow + i;
      int screenRow = 2 + i;
      move(screenRow, 0);
      clrtoeol();
      if (lineIdx < tv->lineCount && tv->lines[lineIdx]) {
        char numBuf[16];
        snprintf(numBuf, sizeof(numBuf), "%6d ", lineIdx + 1);
        attron(COLOR_PAIR(NCTUI_PAIR_FILTER_EMPTY));
        nc_mvwaddstr(stdscr, screenRow, 0, numBuf);
        attroff(COLOR_PAIR(NCTUI_PAIR_FILTER_EMPTY));

        const char *line = tv->lines[lineIdx];
        int lineLen = (int)strlen(line);
        int avail = tv->screenCols - 7;
        if (avail < 0) avail = 0;
        if (tv->scrollCol < lineLen) {
          char lineBuf[512];
          int showLen = lineLen - tv->scrollCol;
          if (showLen > avail) showLen = avail;
          if (showLen > (int)sizeof(lineBuf) - 1)
            showLen = (int)sizeof(lineBuf) - 1;
          memcpy(lineBuf, line + tv->scrollCol, showLen);
          lineBuf[showLen] = '\0';
          nc_mvwaddstr(stdscr, screenRow, 7, lineBuf);
        }
      }
    }

    /* Status bar */
    attron(COLOR_PAIR(NCTUI_PAIR_STATUS) | A_BOLD);
    nc_fillRow(stdscr, tv->screenRows - 1, tv->screenCols);
    nc_mvwaddstr(stdscr, tv->screenRows - 1, 1,
                 "F3=Back  PgUp/PgDn  Up/Down  Left/Right=Scroll  Home/End");
    attroff(COLOR_PAIR(NCTUI_PAIR_STATUS) | A_BOLD);

    refresh();

    int rawKey = nctuiGetch();

    if (rawKey == KEY_F(3) || rawKey == ASCII_ESC) {
      nctuiNavPop(nav);
      return;
    }
    {
      int key = nctuiKeyToEbcdic(rawKey);
      if (key == 'q' || key == 'Q') { nctuiNavPop(nav); return; }
    }

    if (rawKey == KEY_UP) {
      if (tv->scrollRow > 0) tv->scrollRow--;
    } else if (rawKey == KEY_DOWN) {
      if (tv->scrollRow + tv->dataRows < tv->lineCount) tv->scrollRow++;
    } else if (rawKey == KEY_PPAGE || rawKey == KEY_F(7)) {
      tv->scrollRow -= tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    } else if (rawKey == KEY_NPAGE || rawKey == KEY_F(8)) {
      tv->scrollRow += tv->dataRows;
      if (tv->scrollRow + tv->dataRows > tv->lineCount)
        tv->scrollRow = tv->lineCount - tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    } else if (rawKey == KEY_LEFT) {
      if (tv->scrollCol > 0) tv->scrollCol--;
    } else if (rawKey == KEY_RIGHT) {
      tv->scrollCol++;
    } else if (rawKey == KEY_HOME) {
      tv->scrollRow = 0;
      tv->scrollCol = 0;
    } else if (rawKey == KEY_END) {
      tv->scrollRow = tv->lineCount - tv->dataRows;
      if (tv->scrollRow < 0) tv->scrollRow = 0;
    } else if (rawKey == KEY_RESIZE) {
      getmaxyx(stdscr, tv->screenRows, tv->screenCols);
      nav->screenRows = tv->screenRows;
      nav->screenCols = tv->screenCols;
    }
  }
}

/* ================================================================
   Navigator — top-level dispatch loop
   ================================================================ */

void nctuiNavRun(NcTuiNav *nav) {
  while (nav->running && nav->depth >= 0) {
    NcTuiPage *page = &nav->stack[nav->depth];
    if (page->type == NCTUI_PAGE_TABLE) {
      navRunTable(nav);
    } else if (page->type == NCTUI_PAGE_TEXTVIEW) {
      navRunTextView(nav);
    } else {
      /* Unknown page type — pop it */
      nctuiNavPop(nav);
    }
  }
}

void nctuiNavTerm(NcTuiNav *nav) {
  static const char MOUSE_OFF[] = {
    0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x36, 0x6C,  /* ESC[?1006l */
    0x1B, 0x5B, 0x3F, 0x31, 0x30, 0x30, 0x30, 0x6C,  /* ESC[?1000l */
    0
  };
  putp(MOUSE_OFF);
  fflush(stdout);
  endwin();
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
