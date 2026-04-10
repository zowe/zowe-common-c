

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __NCTUI__
#define __NCTUI__ 1

/*
  ncurses-based TUI framework for z/OS.

  Hybrid EBCDIC/ASCII environment: the application is compiled in EBCDIC
  mode (xlclang default) while ncurses (from zopen) is compiled in ASCII.
  This header and its implementation handle all boundary conversion.

  API mirrors tui.h (the raw VT100 version) so tools can switch between
  them by changing the include and the prefix.
*/

#include <ncurses.h>
#include <panel.h>
/* NOTE: term.h is NOT included here — it #defines 'columns' and 'lines'
   which collide with our struct field names.  nctui.c includes term.h
   locally and #undefs the offending macros. */

/* ----------------------------------------------------------------
   Column model (same as tui.h)
   ---------------------------------------------------------------- */

#define NCTUI_MAX_COLUMNS     64
#define NCTUI_MAX_COL_WIDTH   64

#define NCTUI_ALIGN_LEFT   0
#define NCTUI_ALIGN_RIGHT  1

typedef struct NcTuiColumn_tag {
  char     name[32];
  int      width;
  int      align;
  char     filter[NCTUI_MAX_COL_WIDTH];
} NcTuiColumn;

/* ----------------------------------------------------------------
   Selection modes
   ---------------------------------------------------------------- */

#define NCTUI_SELECT_NONE     0   /* no row selection */
#define NCTUI_SELECT_SINGLE   1   /* one row at a time (default) */
#define NCTUI_SELECT_MULTI    2   /* multiple rows via NP column */

/* ----------------------------------------------------------------
   Focus areas — determines where printable keystrokes are routed
   ---------------------------------------------------------------- */

#define NCTUI_FOCUS_COMMAND   0   /* keystrokes go to command line */
#define NCTUI_FOCUS_DATA      1   /* keystrokes go to NP column */

/* ----------------------------------------------------------------
   Forward declarations
   ---------------------------------------------------------------- */

struct NcTuiNav_tag;
typedef struct NcTuiNav_tag NcTuiNav;

/* ----------------------------------------------------------------
   Callbacks
   ---------------------------------------------------------------- */

typedef void (*NcTuiCellFormatter)(int row, int col, char *buf, int bufLen,
                                   void *userData);
typedef int (*NcTuiCommandHandler)(const char *command, void *userData);
typedef int (*NcTuiRefreshHandler)(void *userData);
typedef void (*NcTuiFilterHandler)(void *userData);
typedef void (*NcTuiSortHandler)(int col, int ascending, void *userData);
typedef int (*NcTuiSelectHandler)(int row, void *userData);

/* ----------------------------------------------------------------
   Color pairs (pre-defined)
   ---------------------------------------------------------------- */

enum {
  NCTUI_PAIR_TITLE = 1,    /* white on blue */
  NCTUI_PAIR_HEADER,       /* bold yellow on black */
  NCTUI_PAIR_DATA,         /* default (white on black) */
  NCTUI_PAIR_SELECTED,     /* black on cyan (reverse select) */
  NCTUI_PAIR_STATUS,       /* white on blue (same as title) */
  NCTUI_PAIR_SORT_HDR,     /* white on cyan (sorted column header) */
  NCTUI_PAIR_FILTER_EDIT,  /* green on white (active filter) */
  NCTUI_PAIR_FILTER_SET,   /* bold green on black */
  NCTUI_PAIR_FILTER_EMPTY, /* dim cyan (placeholder dots) */
  NCTUI_PAIR_ALERT,        /* white on red */
  NCTUI_PAIR_CMD,          /* default for command line */
  NCTUI_PAIR_BOX,          /* white on blue (dialog borders) */
  NCTUI_PAIR_INPUT,        /* yellow on black (input field) */
  NCTUI_PAIR_COUNT
};

/* ----------------------------------------------------------------
   Row actions (NP column commands)
   ---------------------------------------------------------------- */

#define NCTUI_MAX_ACTIONS     16
#define NCTUI_MAX_ROWS       8192  /* max rows with NP column state */

#define NCTUI_ACTION_DEFAULT  0x01  /* triggered by Enter with no NP cmd */
#define NCTUI_ACTION_DRILLDOWN 0x02 /* pushes a new page on the nav stack */

/*
  Action handler return codes:
    0 = done, stay on current page
   >0 = framework interprets (reserved)
   <0 = error, framework shows status message
  For drill-down actions, the handler should call nctuiNavPush().
*/
typedef int (*NcTuiActionHandler)(int row, void *userData, NcTuiNav *nav);

typedef struct NcTuiAction_tag {
  char           key;       /* NP column character (EBCDIC), e.g. 'S', 'P' */
  char           label[24]; /* display name for help */
  int            flags;     /* NCTUI_ACTION_DEFAULT, NCTUI_ACTION_DRILLDOWN */
  NcTuiActionHandler handler;
} NcTuiAction;

/* ----------------------------------------------------------------
   Main TUI table state
   ---------------------------------------------------------------- */

typedef struct NcTuiTable_tag {
  /* title */
  char           title[128];

  /* column definitions (column 0 = NP, owned by framework) */
  NcTuiColumn    columns[NCTUI_MAX_COLUMNS];
  int            numColumns;
  int            fixedColumns;

  /* data dimensions */
  int            rowCount;

  /* scroll state */
  int            scrollRow;
  int            scrollCol;

  /* selection */
  int            selectMode;     /* NCTUI_SELECT_NONE/SINGLE/MULTI */
  int            cursorRow;      /* implicit cursor (arrow/mouse) */
  int            selectedRow;    /* legacy compat — same as cursorRow for SINGLE */

  /* NP column state — per-row pending action characters */
  char           npCommands[NCTUI_MAX_ROWS];
  int            npCount;        /* count of rows with pending commands */

  /* actions */
  NcTuiAction    actions[NCTUI_MAX_ACTIONS];
  int            numActions;

  /* filter status */
  int            totalRowCount;  /* unfiltered total (app sets this) */
  int            filterEnabled;  /* whether this table supports filtering */

  /* screen geometry (managed by framework) */
  int            screenRows;
  int            screenCols;
  int            dataRows;

  /* focus model — determines where keystrokes are routed */
  int            focusArea;       /* NCTUI_FOCUS_COMMAND or NCTUI_FOCUS_DATA */

  /* command line */
  char           cmdBuf[256];
  int            cmdPos;
  int            cmdActive;

  /* status message */
  char           statusMsg[256];

  /* sort state */
  int            sortColumn;
  int            sortAscending;

  /* filter state */
  int            filterActive;
  int            filterEditCol;
  int            filterEditPos;

  /* callbacks */
  NcTuiCellFormatter  cellFormatter;
  NcTuiCommandHandler commandHandler;
  NcTuiRefreshHandler refreshHandler;
  NcTuiSelectHandler  selectHandler;    /* legacy — use actions for new code */
  NcTuiFilterHandler  filterHandler;
  NcTuiSortHandler    sortHandler;
  void               *userData;

  /* back-pointer to navigator (set by framework when pushed) */
  NcTuiNav       *nav;

  /* mouse state (from last mouse event) */
  int            mouseRow;
  int            mouseCol;
  int            mouseButton;

  /* internal ncurses windows */
  WINDOW        *titleWin;
  WINDOW        *cmdWin;
  WINDOW        *headerWin;
  WINDOW        *filterWin;
  WINDOW        *dataWin;
  WINDOW        *statusWin;
} NcTuiTable;

/* ----------------------------------------------------------------
   API
   ---------------------------------------------------------------- */

int  nctuiInit(NcTuiTable *tui);
void nctuiSetColumns(NcTuiTable *tui, NcTuiColumn *cols, int numCols,
                     int fixedCols);
void nctuiSetTitle(NcTuiTable *tui, const char *title);
void nctuiRender(NcTuiTable *tui);
int  nctuiEventLoop(NcTuiTable *tui);
void nctuiTerm(NcTuiTable *tui);
void nctuiSetStatus(NcTuiTable *tui, const char *fmt, ...);
void nctuiClearFilters(NcTuiTable *tui);
int  nctuiMatchFilter(const char *pattern, const char *value);

/* ----------------------------------------------------------------
   Text viewer
   ---------------------------------------------------------------- */

typedef struct NcTuiTextView_tag {
  char           title[128];
  char         **lines;
  int            lineCount;
  int            scrollRow;
  int            scrollCol;
  int            screenRows;
  int            screenCols;
  int            dataRows;
} NcTuiTextView;

void nctuiTextViewShow(NcTuiTextView *tv);

/* ----------------------------------------------------------------
   Modal dialogs
   ---------------------------------------------------------------- */

/* Text input dialog.  Returns 1 if user pressed Enter (result in buf),
   0 if user cancelled (Esc/F3).  buf is pre-filled and editable. */
int nctuiDialogInput(const char *title, const char *prompt,
                     char *buf, int bufLen);

/* Confirmation dialog.  Returns 1 for Yes, 0 for No/Cancel. */
int nctuiDialogConfirm(const char *title, const char *message);

/* ----------------------------------------------------------------
   EBCDIC/ASCII boundary helpers (public for tools that need them)
   ---------------------------------------------------------------- */

char *nctuiToAscii(const char *ebcStr);
char *nctuiToEbcdic(char *ascStr, int len);
int   nctuiKeyToEbcdic(int ch);

/* Unicode box drawing via putp (bypasses broken ACS) */
void nctuiDrawBox(WINDOW *w);

/* Register an action for the table's NP column.
   key is the EBCDIC character the user types in column 0.
   flags: NCTUI_ACTION_DEFAULT for Enter-invoked, NCTUI_ACTION_DRILLDOWN
   if the handler pushes a page. */
void nctuiAddAction(NcTuiTable *tui, char key, const char *label,
                    int flags, NcTuiActionHandler handler);

/* ----------------------------------------------------------------
   Page model — tagged union of Table or TextView
   ---------------------------------------------------------------- */

#define NCTUI_PAGE_TABLE     1
#define NCTUI_PAGE_TEXTVIEW  2

typedef struct NcTuiPage_tag {
  int            type;    /* NCTUI_PAGE_TABLE or NCTUI_PAGE_TEXTVIEW */
  char           breadcrumb[64]; /* short label for nav trail */
  union {
    NcTuiTable      table;
    NcTuiTextView   textView;
  };
} NcTuiPage;

/* ----------------------------------------------------------------
   Navigator — owns the page stack and top-level event loop
   ---------------------------------------------------------------- */

#define NCTUI_MAX_STACK   16

struct NcTuiNav_tag {
  NcTuiPage      stack[NCTUI_MAX_STACK];
  int            depth;       /* index of current top (-1 = empty) */
  int            running;     /* set to 0 to exit */
  int            screenRows;
  int            screenCols;
};

/* Initialize the navigator and ncurses.  Call once at program start. */
int  nctuiNavInit(NcTuiNav *nav);

/* Push a page onto the stack.  The page is COPIED into the stack.
   For tables, the framework sets the back-pointer (nav) automatically.
   Returns 0 on success, -1 if stack is full. */
int  nctuiNavPush(NcTuiNav *nav, NcTuiPage *page);

/* Pop the current page.  Returns to the previous page, or exits
   if the stack is empty.  Called automatically by F3. */
void nctuiNavPop(NcTuiNav *nav);

/* Run the navigator event loop.  Dispatches to the top-of-stack
   page until the stack is empty or running is set to 0.
   Call after pushing the initial page. */
void nctuiNavRun(NcTuiNav *nav);

/* Shut down ncurses.  Call once at program exit. */
void nctuiNavTerm(NcTuiNav *nav);

/* Helper: build a table page with common defaults set. */
void nctuiPageInitTable(NcTuiPage *page, const char *breadcrumb);

/* Helper: build a text viewer page. */
void nctuiPageInitTextView(NcTuiPage *page, const char *breadcrumb,
                           char **lines, int lineCount);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
