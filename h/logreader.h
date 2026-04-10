


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __LOGREADER__
#define __LOGREADER__ 1

#ifndef __LONGNAME__
#define logReaderCreateSAPI   LGRDSAPI
#define logReaderOpen         LGRDOPEN
#define logReaderSeekTime     LGRDSEEK
#define logReaderReadForward  LGRDRFWD
#define logReaderReadBackward LGRDRREV
#define logReaderClose        LGRDCLOS
#define logReaderDestroy      LGRDDEST
#define logRecordParseTime    LGRCPTIM
#endif

#include "zowetypes.h"
#include <time.h>

/*
  logreader — Abstraction for reading z/OS system logs.

  Three possible backends, probed at runtime:
    1. SAPI (SSI 79)     — reads SYSLOG as JES spool dataset.
                           Always available. Forward-only, no native
                           time seek. We fake seek by scanning.
    2. IXGBRWSE           — reads OPERLOG log stream via System Logger.
                           Only if OPERLOG is configured. Native time
                           seek, bidirectional, structured records.
    3. ZIS live WTO       — receives WTOs in real time via authorized
                           MCS console registered through ZIS.
                           Only if ZIS is running and FACILITY class
                           profile permits it.

  The consumer (zlog TUI) uses the same interface regardless of
  backend. Capabilities are advertised via flags so the TUI can
  adapt (e.g., gray out "backward" if not supported).
*/

/* ----------------------------------------------------------------
   Capability flags — what the active backend can do
   ---------------------------------------------------------------- */

#define LOGREADER_CAN_SEEK_TIME    0x0001  /* seekTime is efficient (not a scan) */
#define LOGREADER_CAN_BACKWARD     0x0002  /* readBackward is supported */
#define LOGREADER_CAN_TAIL         0x0004  /* live streaming available */
#define LOGREADER_CAN_STRUCTURED   0x0008  /* record fields parsed by backend */

/* ----------------------------------------------------------------
   Log record — normalized view of one log entry
   ---------------------------------------------------------------- */

/* Flags indicating which fields in LogRecord are valid */
#define LOGREC_HAS_TIMESTAMP   0x01
#define LOGREC_HAS_JOBNAME     0x02
#define LOGREC_HAS_MSGID       0x04
#define LOGREC_HAS_SYSNAME     0x08

typedef struct LogRecord_tag {
  const char *text;           /* raw text of the record (not null-terminated) */
  int         textLen;        /* length of text */
  time_t      timestamp;      /* parsed from text or from record header */
  char        jobname[9];     /* null-terminated, blank-trimmed */
  char        msgid[16];      /* e.g. "IEF196I" */
  char        sysname[9];     /* originating system */
  int         fields;         /* LOGREC_HAS_xxx bitmask */
} LogRecord;

/*
  Callback for record delivery.
  Return 0 to continue reading, non-zero to stop.
*/
typedef int (*LogRecordHandler)(const LogRecord *record,
                                int recordNumber,
                                void *userData);

/* ----------------------------------------------------------------
   LogReader — the opaque handle
   ---------------------------------------------------------------- */

typedef struct LogReaderSAPIState_tag LogReaderSAPIState;
/* typedef struct LogReaderIXGState_tag  LogReaderIXGState;  future */
/* typedef struct LogReaderZISState_tag  LogReaderZISState;  future */

typedef struct LogReader_tag {
  int    capabilities;        /* LOGREADER_CAN_xxx bitmask */
  int    lastRC;              /* return code from last operation */
  char   errorMsg[128];       /* human-readable error from last op */

  /* Position tracking */
  time_t positionTime;        /* timestamp of last record read */
  int    recordsDelivered;    /* total records delivered since open */
  int    atEnd;               /* 1 if last read hit end-of-data */

  /* Backend state — only one is non-NULL */
  LogReaderSAPIState *sapiState;
  /* LogReaderIXGState *ixgState;   future */
  /* LogReaderZISState *zisState;   future */
} LogReader;

/* ----------------------------------------------------------------
   Seek sentinel
   ---------------------------------------------------------------- */

/* Pass to seekTime to position at the end (for tail mode) */
#define LOGREADER_TIME_END   ((time_t)-1)

/* ----------------------------------------------------------------
   SAPI backend — constructor
   ---------------------------------------------------------------- */

struct JobService_tag;  /* forward decl, defined in jobservice.h */

/*
  Create a LogReader backed by SAPI.
  Reads SYSLOG as a JES spool dataset via SSI 79.
  The JobService must already be initialized.
  Returns NULL on allocation failure.
*/
LogReader *logReaderCreateSAPI(struct JobService_tag *jobService);

/* Future:
LogReader *logReaderCreateIXG(const char *logStreamName);
LogReader *logReaderCreateZIS(void *zisConnection);
*/

/* ----------------------------------------------------------------
   Operations — backend-agnostic
   ---------------------------------------------------------------- */

/*
  Open the log for reading.
  For SAPI: selects the SYSLOG job and prepares for sequential read.
  Must be called before any read or seek operation.
  Returns 0 on success.
*/
int logReaderOpen(LogReader *reader);

/*
  Position to a target time.
  For SAPI: scans forward, parsing timestamps from record text,
  skipping records until the target time is reached. Expensive
  but correct. Records before the target are not delivered.
  For IXGBRWSE (future): uses native STARTTIME parameter. Instant.
  Pass LOGREADER_TIME_END to seek to the end of the log.
  Returns 0 on success.
*/
int logReaderSeekTime(LogReader *reader, time_t target);

/*
  Read up to maxRecords forward from the current position.
  Calls handler for each record. Reading stops when:
    - maxRecords have been delivered, or
    - handler returns non-zero, or
    - end of data is reached (reader->atEnd set to 1)
  Returns 0 on success (even if 0 records delivered at end).
*/
int logReaderReadForward(LogReader *reader, int maxRecords,
                         LogRecordHandler handler, void *userData);

/*
  Read up to maxRecords backward from the current position.
  Only available if LOGREADER_CAN_BACKWARD is set.
  For SAPI: closes current session, reopens, scans to
  (currentPosition - maxRecords). Expensive.
  For IXGBRWSE (future): native backward browse. Cheap.
  Records are delivered in forward (chronological) order
  even though we're seeking backward.
  Returns 0 on success, -1 if not supported.
*/
int logReaderReadBackward(LogReader *reader, int maxRecords,
                          LogRecordHandler handler, void *userData);

/*
  Close the log and release backend resources.
  The LogReader can be reopened with logReaderOpen().
*/
void logReaderClose(LogReader *reader);

/*
  Destroy the LogReader and free all memory.
  Calls close() if still open.
*/
void logReaderDestroy(LogReader *reader);

/* ----------------------------------------------------------------
   Utility — SYSLOG timestamp parsing
   ---------------------------------------------------------------- */

/*
  Parse a timestamp from a SYSLOG hardcopy text record.
  SYSLOG records look like:
    N 1234567 SY1  14:30:02.15 STC12345 IEF196I ...
  The timestamp is at a predictable column offset.
  Returns 0 and fills *result on success, -1 if not parseable.
  Also extracts jobname/msgid/sysname into the LogRecord
  if the fields parameter is non-NULL.
*/
int logRecordParseTime(const char *text, int textLen,
                       time_t *result, LogRecord *fields);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
