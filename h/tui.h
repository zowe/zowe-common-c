

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __TUI__
#define __TUI__ 1

/* ----------------------------------------------------------------
   Column model
   ---------------------------------------------------------------- */

#define TUI_MAX_COLUMNS     64
#define TUI_MAX_COL_WIDTH   64

#define TUI_ALIGN_LEFT   0
#define TUI_ALIGN_RIGHT  1

typedef struct TuiColumn_tag {
  char     name[32];         /* column header text */
  int      width;            /* display width */
  int      align;            /* TUI_ALIGN_LEFT or TUI_ALIGN_RIGHT */
  char     filter[TUI_MAX_COL_WIDTH]; /* per-column wildcard filter */
} TuiColumn;

/* ----------------------------------------------------------------
   Callbacks
   ---------------------------------------------------------------- */

typedef void (*TuiCellFormatter)(int row, int col, char *buf, int bufLen,
                                 void *userData);
typedef int (*TuiCommandHandler)(const char *command, void *userData);
typedef int (*TuiRefreshHandler)(void *userData);
typedef int (*TuiNPHandler)(const char *npCmd, int row, void *userData);
typedef void (*TuiFilterHandler)(void *userData);
typedef void (*TuiSortHandler)(int col, int ascending, void *userData);

/* ----------------------------------------------------------------
   Key codes returned by tuiReadKey
   ---------------------------------------------------------------- */

#define TUI_KEY_UP       0x100
#define TUI_KEY_DOWN     0x101
#define TUI_KEY_LEFT     0x102
#define TUI_KEY_RIGHT    0x103
#define TUI_KEY_PGUP     0x104
#define TUI_KEY_PGDN     0x105
#define TUI_KEY_HOME     0x106
#define TUI_KEY_END      0x107
#define TUI_KEY_F3       0x110
#define TUI_KEY_F5       0x111
#define TUI_KEY_F7       0x112
#define TUI_KEY_F6       0x114
#define TUI_KEY_F8       0x113
#define TUI_KEY_ENTER    0x120
#define TUI_KEY_BACKSPACE 0x121
#define TUI_KEY_ESCAPE   0x122
#define TUI_KEY_TAB      0x123
#define TUI_KEY_BTAB     0x124
#define TUI_KEY_MOUSE    0x130
#define TUI_KEY_RESIZE   0x131

/* ----------------------------------------------------------------
   Row select callback: invoked when Enter or mouse click on a
   data row.  row is the data row index.
   Return 0 to continue, non-zero to exit the event loop.
   ---------------------------------------------------------------- */
typedef int (*TuiSelectHandler)(int row, void *userData);

/* ----------------------------------------------------------------
   Main TUI table state
   ---------------------------------------------------------------- */

typedef struct TuiTable_tag {
  /* title */
  char           title[128];

  /* column definitions */
  TuiColumn      columns[TUI_MAX_COLUMNS];
  int            numColumns;
  int            fixedColumns;

  /* data dimensions */
  int            rowCount;

  /* scroll state */
  int            scrollRow;
  int            scrollCol;

  /* selection */
  int            selectedRow;     /* highlighted row (-1 = none) */

  /* screen geometry */
  int            screenRows;
  int            screenCols;
  int            dataRows;

  /* command line */
  char           cmdBuf[256];
  int            cmdPos;
  int            cmdActive;

  /* status message */
  char           statusMsg[256];

  /* sort state */
  int            sortColumn;       /* column owning sort, -1 = none */
  int            sortAscending;    /* 1 = ascending, 0 = descending */

  /* filter state */
  int            filterActive;     /* 1 = filter row is shown */
  int            filterEditCol;    /* column being edited, -1 = none */
  int            filterEditPos;    /* cursor pos in active filter field */

  /* callbacks */
  TuiCellFormatter  cellFormatter;
  TuiCommandHandler commandHandler;
  TuiRefreshHandler refreshHandler;
  TuiNPHandler      npHandler;
  TuiSelectHandler  selectHandler;
  TuiFilterHandler  filterHandler;
  TuiSortHandler    sortHandler;
  void             *userData;

  /* mouse state (from last mouse event) */
  int            mouseRow;
  int            mouseCol;
  int            mouseButton;

  /* internal */
  int            useColor;
  int            origTermValid;
} TuiTable;

/* ----------------------------------------------------------------
   API
   ---------------------------------------------------------------- */

int  tuiInit(TuiTable *tui);
void tuiSetColumns(TuiTable *tui, TuiColumn *cols, int numCols, int fixedCols);
void tuiSetTitle(TuiTable *tui, const char *title);
void tuiRender(TuiTable *tui);
int  tuiEventLoop(TuiTable *tui);
void tuiTerm(TuiTable *tui);
void tuiSetStatus(TuiTable *tui, const char *fmt, ...);
int  tuiReadKey(void);
void tuiClearFilters(TuiTable *tui);
int  tuiMatchFilter(const char *pattern, const char *value);

/* ----------------------------------------------------------------
   Text viewer — full-screen scrollable text display.
   Used for sysout content viewing.
   Returns when user presses F3.
   ---------------------------------------------------------------- */

typedef struct TuiTextView_tag {
  char           title[128];
  char         **lines;          /* array of line strings */
  int            lineCount;
  int            scrollRow;
  int            scrollCol;      /* horizontal scroll offset */
  int            screenRows;
  int            screenCols;
  int            dataRows;
} TuiTextView;

void tuiTextViewShow(TuiTextView *tv);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
