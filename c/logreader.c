


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  logreader.c — LogReader implementation with SAPI backend.

  Provides paged, time-seekable reading of SYSLOG from JES spool.
  See logreader.h for interface documentation.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "zos.h"
#include "ssi.h"
#include "jobservice.h"
#include "logreader.h"
#include "dynalloc.h"

#include "iazsss2.h"

/* ----------------------------------------------------------------
   SAPI backend state
   ---------------------------------------------------------------- */

struct LogReaderSAPIState_tag {
  JobService *jobService;
  char       *sss2Raw;         /* SSS2 block, 31-bit storage */
  struct sss2 *sss2;           /* alias into sss2Raw */
  int         sss2AllocSize;
  FILE       *currentFile;     /* open spool dataset being read */
  char        currentDD[9];    /* allocated DD name */
  int         sessionOpen;     /* SSS2 session initialized */
  int         datasetOpen;     /* currentFile is valid */
  int         allExhausted;    /* SAPI returned end-of-datasets */
  time_t      lastTimestamp;   /* from last record parsed */
};

/* ----------------------------------------------------------------
   SAPI session lifecycle
   ---------------------------------------------------------------- */

/*
  Initialize the SSS2 block for selecting SYSLOG.
  Sets browse/read-only, selects jobname SYSLOG, all queues.
*/
static int sapiInitSSS2(LogReaderSAPIState *st) {
  st->sss2AllocSize = SSS2SIZE;
  st->sss2Raw = (char *)safeMalloc31(st->sss2AllocSize, "LR SSS2");
  if (!st->sss2Raw) return -1;
  memset(st->sss2Raw, 0, st->sss2AllocSize);

  st->sss2 = (struct sss2 *)st->sss2Raw;

  st->sss2->sss2len = SSS2SIZE;
  st->sss2->sss2ver = SSS2VJCR;
  memcpy(st->sss2->sss2eye, "\xE2\xE2\xE2\xF2", 4);  /* "SSS2" EBCDIC */
  st->sss2->sss2type = SSS2PUGE;

  /* Select jobname SYSLOG */
  memset(st->sss2->sss2jobn, 0x40, 8);  /* EBCDIC blanks */
  memcpy(st->sss2->sss2jobn, "SYSLOG", 6);
  st->sss2->sss2sjbn = 1;

  /* Browse, read-only */
  st->sss2->sss2sbro = 1;
  st->sss2->sss2sron = 1;

  /* All queues */
  st->sss2->sss2sawt = 7;

  /* All job types (STC, TSU, JOB, APPC) */
  st->sss2->sss2sstc = 1;
  st->sss2->sss2stsu = 1;
  st->sss2->sss2sjob = 1;
  st->sss2->sss2sapc = 1;

  /* Keep disposition */
  st->sss2->sss2dkpe = 1;

  st->sessionOpen = 1;
  return 0;
}

/*
  Ask SAPI for the next spool dataset. If one is available,
  DYNALLOC it and open for reading. Returns 0 if a dataset
  was opened, -1 if no more datasets or error.
*/
static int sapiOpenNextDataset(LogReaderSAPIState *st, LogReader *reader) {
  while (1) {
    int funcRC = 0;
    int rc = sapiCall(st->sss2, &funcRC);

    if (rc != 0) {
      snprintf(reader->errorMsg, sizeof(reader->errorMsg),
               "SAPI SSI call failed rc=%d", rc);
      reader->lastRC = rc;
      return -1;
    }

    if (funcRC == SSS2EODS) {
      st->allExhausted = 1;
      reader->atEnd = 1;
      return -1;
    }

    if (funcRC != SSS2RTOK) {
      snprintf(reader->errorMsg, sizeof(reader->errorMsg),
               "SAPI unexpected funcRC=%d", funcRC);
      reader->lastRC = funcRC;
      return -1;
    }

    /* Got a dataset — extract DSN and DYNALLOC it */
    char dsn[45];
    memcpy(dsn, st->sss2->sss2dsn, 44);
    dsn[44] = '\0';
    for (int i = 43; i >= 0 && dsn[i] == ' '; i--) dsn[i] = '\0';

    void * __ptr32 btok = (void * __ptr32)st->sss2->sss2btok;

    memset(st->currentDD, ' ', 8);
    st->currentDD[8] = '\0';

    int allocErr = 0, allocInfo = 0;
    rc = sapiAllocDataset(dsn, st->currentDD, "JES2", btok,
                          &allocErr, &allocInfo);
    if (rc != 0) {
      /* DYNALLOC failed — skip this dataset, try next */
      continue;
    }

    /* Trim DD name */
    for (int i = 7; i >= 0 && st->currentDD[i] == ' '; i--) {
      st->currentDD[i] = '\0';
    }

    /* Open for record I/O */
    char ddPath[16];
    sprintf(ddPath, "//DD:%s", st->currentDD);

    st->currentFile = fopen(ddPath, "rb,type=record");
    if (!st->currentFile) {
      DeallocDDName(st->currentDD);
      continue;
    }

    st->datasetOpen = 1;
    return 0;
  }
}

/*
  Close the current spool dataset (if open) and deallocate the DD.
*/
static void sapiCloseCurrentDataset(LogReaderSAPIState *st) {
  if (st->currentFile) {
    fclose(st->currentFile);
    st->currentFile = NULL;
  }
  if (st->datasetOpen) {
    DeallocDDName(st->currentDD);
    st->datasetOpen = 0;
  }
}

/*
  Terminate the SAPI session and free the SSS2 block.
*/
static void sapiTerminate(LogReaderSAPIState *st) {
  sapiCloseCurrentDataset(st);

  if (st->sessionOpen && st->sss2) {
    st->sss2->sss2ctrl = 1;
    int funcRC = 0;
    sapiCall(st->sss2, &funcRC);
    st->sessionOpen = 0;
  }

  if (st->sss2Raw) {
    safeFree31(st->sss2Raw, st->sss2AllocSize);
    st->sss2Raw = NULL;
    st->sss2 = NULL;
  }
}

/* ----------------------------------------------------------------
   Timestamp parser
   ----------------------------------------------------------------

   SYSLOG hardcopy records have varying formats, but the timestamp
   is always HH:MM:SS or HH.MM.SS in the first ~40 characters.
   Some systems include hundredths: HH:MM:SS.TT

   We scan for the pattern rather than hardcoding column offsets,
   since the layout depends on CONSOLxx settings and JES level.

   We also try to extract date (YYYY.DDD Julian or YYYY-MM-DD),
   jobname, and message ID from surrounding fields.
*/

/*
  Scan text for HH:MM:SS or HH.MM.SS pattern.
  Returns the byte offset of the first H, or -1 if not found.
*/
static int findTimePattern(const char *text, int textLen) {
  int limit = textLen - 7;  /* need at least 8 chars: HH:MM:SS */
  if (limit < 0) return -1;
  if (limit > 40) limit = 40;  /* don't scan past column 40 */

  for (int i = 0; i < limit; i++) {
    /* Check for DD:DD:DD or DD.DD.DD where D is a digit */
    char sep = text[i + 2];
    if (sep != ':' && sep != '.') continue;
    if (text[i + 5] != sep) continue;

    if (text[i]     >= '0' && text[i]     <= '9' &&
        text[i + 1] >= '0' && text[i + 1] <= '9' &&
        text[i + 3] >= '0' && text[i + 3] <= '9' &&
        text[i + 4] >= '0' && text[i + 4] <= '9' &&
        text[i + 6] >= '0' && text[i + 6] <= '9' &&
        text[i + 7] >= '0' && text[i + 7] <= '9') {
      /* Validate ranges: hours 00-23, minutes 00-59, seconds 00-59 */
      int hh = (text[i]     - '0') * 10 + (text[i + 1] - '0');
      int mm = (text[i + 3] - '0') * 10 + (text[i + 4] - '0');
      int ss = (text[i + 6] - '0') * 10 + (text[i + 7] - '0');
      if (hh <= 23 && mm <= 59 && ss <= 59) {
        return i;
      }
    }
  }
  return -1;
}

/*
  Parse a Julian date (YYYYDDD or YYYY.DDD) near a given position.
  Scans backward from timeOfs looking for 7 consecutive digits
  (or digits with a dot after position 4).
  Returns 0 on success, fills year and dayOfYear.
*/
static int findJulianDate(const char *text, int textLen, int timeOfs,
                          int *year, int *dayOfYear) {
  /* Look in the ~15 characters before the time */
  int start = timeOfs - 15;
  if (start < 0) start = 0;
  int end = timeOfs;

  for (int i = start; i < end - 6; i++) {
    /* Try YYYYDDD (7 digits) */
    int allDigit = 1;
    for (int j = 0; j < 7; j++) {
      if (text[i + j] < '0' || text[i + j] > '9') { allDigit = 0; break; }
    }
    if (allDigit) {
      *year = (text[i] - '0') * 1000 + (text[i+1] - '0') * 100 +
              (text[i+2] - '0') * 10 + (text[i+3] - '0');
      *dayOfYear = (text[i+4] - '0') * 100 + (text[i+5] - '0') * 10 +
                   (text[i+6] - '0');
      if (*year >= 2000 && *year <= 2099 && *dayOfYear >= 1 && *dayOfYear <= 366) {
        return 0;
      }
    }

    /* Try YYYY.DDD (with dot) */
    if (i + 7 < end &&
        text[i] >= '0' && text[i] <= '9' &&
        text[i+1] >= '0' && text[i+1] <= '9' &&
        text[i+2] >= '0' && text[i+2] <= '9' &&
        text[i+3] >= '0' && text[i+3] <= '9' &&
        text[i+4] == '.' &&
        text[i+5] >= '0' && text[i+5] <= '9' &&
        text[i+6] >= '0' && text[i+6] <= '9' &&
        text[i+7] >= '0' && text[i+7] <= '9') {
      *year = (text[i] - '0') * 1000 + (text[i+1] - '0') * 100 +
              (text[i+2] - '0') * 10 + (text[i+3] - '0');
      *dayOfYear = (text[i+5] - '0') * 100 + (text[i+6] - '0') * 10 +
                   (text[i+7] - '0');
      if (*year >= 2000 && *year <= 2099 && *dayOfYear >= 1 && *dayOfYear <= 366) {
        return 0;
      }
    }
  }
  return -1;
}

int logRecordParseTime(const char *text, int textLen,
                       time_t *result, LogRecord *fields) {
  int timeOfs = findTimePattern(text, textLen);
  if (timeOfs < 0) return -1;

  int hh = (text[timeOfs]     - '0') * 10 + (text[timeOfs + 1] - '0');
  int mm = (text[timeOfs + 3] - '0') * 10 + (text[timeOfs + 4] - '0');
  int ss = (text[timeOfs + 6] - '0') * 10 + (text[timeOfs + 7] - '0');

  /* Try to find a date */
  int year = 0, dayOfYear = 0;
  int hasDate = (findJulianDate(text, textLen, timeOfs, &year, &dayOfYear) == 0);

  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  if (hasDate) {
    tm.tm_year = year - 1900;
    tm.tm_mday = dayOfYear;  /* mktime normalizes yday→mon/mday */
    tm.tm_mon = 0;           /* January, let dayOfYear overflow */
  } else {
    /* No date found — use today */
    time_t now = time(NULL);
    struct tm *today = localtime(&now);
    if (today) {
      tm.tm_year = today->tm_year;
      tm.tm_mon  = today->tm_mon;
      tm.tm_mday = today->tm_mday;
    }
  }
  tm.tm_hour = hh;
  tm.tm_min  = mm;
  tm.tm_sec  = ss;
  tm.tm_isdst = -1;

  *result = mktime(&tm);

  /* Extract additional fields if requested */
  if (fields) {
    fields->timestamp = *result;
    fields->fields |= LOGREC_HAS_TIMESTAMP;

    /* Try to extract jobname: typically the token after the time.
       Skip past time (and optional hundredths .TT) then whitespace. */
    int pos = timeOfs + 8;
    if (pos < textLen && text[pos] == '.') {
      pos += 3;  /* skip .TT */
    }
    while (pos < textLen && text[pos] == ' ') pos++;

    /* Jobname: up to 8 non-blank characters */
    if (pos < textLen && text[pos] != ' ') {
      int jstart = pos;
      while (pos < textLen && text[pos] != ' ' && (pos - jstart) < 8) pos++;
      int jlen = pos - jstart;
      memcpy(fields->jobname, text + jstart, jlen);
      fields->jobname[jlen] = '\0';
      fields->fields |= LOGREC_HAS_JOBNAME;

      /* Skip whitespace, then possible ASID, then message ID */
      while (pos < textLen && text[pos] == ' ') pos++;
      /* Skip ASID (8 hex digits) if present */
      if (pos + 8 <= textLen) {
        int isHex = 1;
        for (int i = 0; i < 8; i++) {
          char c = text[pos + i];
          if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
                (c >= 'a' && c <= 'f'))) {
            isHex = 0; break;
          }
        }
        if (isHex && (pos + 8 >= textLen || text[pos + 8] == ' ')) {
          pos += 8;
          while (pos < textLen && text[pos] == ' ') pos++;
        }
      }

      /* Message ID: token like IEF196I, up to 15 chars */
      if (pos < textLen && text[pos] != ' ') {
        int mstart = pos;
        while (pos < textLen && text[pos] != ' ' && (pos - mstart) < 15) pos++;
        int mlen = pos - mstart;
        memcpy(fields->msgid, text + mstart, mlen);
        fields->msgid[mlen] = '\0';
        fields->fields |= LOGREC_HAS_MSGID;
      }
    }

    /* System name: typically before the date, or in a fixed column.
       Look for 1-8 char token before the date/time area. */
    int sstart = 0;
    /* Skip leading control/flag characters and spaces */
    while (sstart < timeOfs && text[sstart] == ' ') sstart++;
    /* Skip sequence number or flag char */
    if (sstart < timeOfs && (text[sstart] < '0' || text[sstart] > '9')) {
      sstart++;  /* skip flag char like N, D, etc. */
      while (sstart < timeOfs && text[sstart] == ' ') sstart++;
    }
    /* Skip sequence number (digits) */
    while (sstart < timeOfs && text[sstart] >= '0' && text[sstart] <= '9') sstart++;
    while (sstart < timeOfs && text[sstart] == ' ') sstart++;

    /* What remains before date/time should be the system name */
    if (sstart < timeOfs) {
      int send = sstart;
      while (send < timeOfs && text[send] != ' ' && (send - sstart) < 8) send++;
      int slen = send - sstart;
      if (slen > 0 && slen <= 8) {
        memcpy(fields->sysname, text + sstart, slen);
        fields->sysname[slen] = '\0';
        fields->fields |= LOGREC_HAS_SYSNAME;
      }
    }
  }

  return 0;
}

/* ----------------------------------------------------------------
   LogReader constructor — SAPI backend
   ---------------------------------------------------------------- */

LogReader *logReaderCreateSAPI(JobService *jobService) {
  LogReader *reader = (LogReader *)malloc(sizeof(LogReader));
  if (!reader) return NULL;
  memset(reader, 0, sizeof(LogReader));

  LogReaderSAPIState *st = (LogReaderSAPIState *)malloc(sizeof(LogReaderSAPIState));
  if (!st) { free(reader); return NULL; }
  memset(st, 0, sizeof(LogReaderSAPIState));

  st->jobService = jobService;
  reader->sapiState = st;

  /* SAPI capabilities: no efficient time seek, no backward, no live tail */
  reader->capabilities = 0;

  return reader;
}

/* ----------------------------------------------------------------
   Open
   ---------------------------------------------------------------- */

int logReaderOpen(LogReader *reader) {
  if (!reader) return -1;

  LogReaderSAPIState *st = reader->sapiState;
  if (!st) {
    snprintf(reader->errorMsg, sizeof(reader->errorMsg),
             "No backend configured");
    return -1;
  }

  if (st->sessionOpen) {
    sapiTerminate(st);
  }

  /* Reset state */
  reader->atEnd = 0;
  reader->recordsDelivered = 0;
  reader->positionTime = 0;
  reader->lastRC = 0;
  reader->errorMsg[0] = '\0';
  st->allExhausted = 0;
  st->lastTimestamp = 0;

  int rc = sapiInitSSS2(st);
  if (rc != 0) {
    snprintf(reader->errorMsg, sizeof(reader->errorMsg),
             "Failed to allocate SSS2 block");
    reader->lastRC = rc;
    return rc;
  }

  return 0;
}

/* ----------------------------------------------------------------
   Read forward
   ---------------------------------------------------------------- */

int logReaderReadForward(LogReader *reader, int maxRecords,
                         LogRecordHandler handler, void *userData) {
  if (!reader || !handler) return -1;

  LogReaderSAPIState *st = reader->sapiState;
  if (!st || !st->sessionOpen) {
    snprintf(reader->errorMsg, sizeof(reader->errorMsg),
             "Reader not open");
    return -1;
  }

  char recordBuf[32768];
  int delivered = 0;

  while (delivered < maxRecords) {
    /* Ensure we have an open dataset */
    if (!st->datasetOpen) {
      if (st->allExhausted) {
        reader->atEnd = 1;
        break;
      }
      if (sapiOpenNextDataset(st, reader) != 0) {
        break;  /* no more datasets or error */
      }
    }

    /* Read records from the current dataset */
    while (delivered < maxRecords && st->datasetOpen) {
      int bytesRead = fread(recordBuf, 1, sizeof(recordBuf), st->currentFile);
      if (bytesRead <= 0) {
        /* End of this dataset — close and try next */
        sapiCloseCurrentDataset(st);
        break;
      }

      /* Build a LogRecord for the handler */
      LogRecord rec;
      memset(&rec, 0, sizeof(rec));
      rec.text = recordBuf;
      rec.textLen = bytesRead;

      /* Try to parse timestamp and fields */
      time_t ts;
      if (logRecordParseTime(recordBuf, bytesRead, &ts, &rec) == 0) {
        st->lastTimestamp = ts;
        reader->positionTime = ts;
      }

      delivered++;
      reader->recordsDelivered++;

      if (handler(&rec, reader->recordsDelivered, userData) != 0) {
        return 0;  /* handler requested stop */
      }
    }
  }

  return 0;
}

/* ----------------------------------------------------------------
   Seek to time (SAPI: scan-forward)
   ---------------------------------------------------------------- */

/* Internal handler that discards records until target time is reached */
typedef struct SeekState_tag {
  time_t targetTime;
  int    found;
} SeekState;

static int seekScanHandler(const LogRecord *record, int recordNumber,
                           void *userData) {
  SeekState *ss = (SeekState *)userData;

  if (ss->targetTime == LOGREADER_TIME_END) {
    /* Seeking to end — keep going, never stop */
    return 0;
  }

  if ((record->fields & LOGREC_HAS_TIMESTAMP) &&
      record->timestamp >= ss->targetTime) {
    ss->found = 1;
    return 1;  /* stop — we've reached the target */
  }

  return 0;  /* keep scanning */
}

int logReaderSeekTime(LogReader *reader, time_t target) {
  if (!reader) return -1;

  LogReaderSAPIState *st = reader->sapiState;
  if (!st) return -1;

  /* If seeking to end, read through everything */
  if (target == LOGREADER_TIME_END) {
    SeekState ss;
    ss.targetTime = LOGREADER_TIME_END;
    ss.found = 0;

    /* Read in large chunks, discarding everything */
    while (!reader->atEnd) {
      logReaderReadForward(reader, 50000, seekScanHandler, &ss);
    }
    return 0;
  }

  /* Scan forward until we find a record at or after target time.
     We read in chunks to avoid one-at-a-time overhead. The scan
     handler stops at the first record >= target. The problem is
     that readForward already delivered that record to the handler.

     For the SAPI backend, seekTime is really a filter hint:
     after seeking, readForward continues from wherever the scan
     stopped. The one record that triggered the stop is lost.
     This is acceptable — we're positioning to an approximate time,
     not to an exact record. */
  SeekState ss;
  ss.targetTime = target;
  ss.found = 0;

  while (!ss.found && !reader->atEnd) {
    logReaderReadForward(reader, 10000, seekScanHandler, &ss);
  }

  return ss.found ? 0 : -1;
}

/* ----------------------------------------------------------------
   Read backward (SAPI: expensive re-read)
   ---------------------------------------------------------------- */

int logReaderReadBackward(LogReader *reader, int maxRecords,
                          LogRecordHandler handler, void *userData) {
  if (!reader || !handler) return -1;

  LogReaderSAPIState *st = reader->sapiState;
  if (!st) return -1;

  /* SAPI has no backward read. To go backward:
     1. Remember where we are (positionTime)
     2. Close the session
     3. Reopen
     4. Seek to (positionTime - estimated_window)
     5. Read forward, collecting into a buffer
     6. Deliver the last maxRecords from that buffer

     This is expensive. The estimated window is a heuristic:
     we guess how many seconds of log correspond to maxRecords
     and seek that far back. If we guess wrong and get too few
     records, we widen and retry.

     For now, return -1 (not supported). The TUI can handle
     this by disabling backward scroll or caching pages itself.
  */

  snprintf(reader->errorMsg, sizeof(reader->errorMsg),
           "Backward read not yet implemented for SAPI backend");
  return -1;
}

/* ----------------------------------------------------------------
   Close
   ---------------------------------------------------------------- */

void logReaderClose(LogReader *reader) {
  if (!reader) return;

  LogReaderSAPIState *st = reader->sapiState;
  if (st) {
    sapiTerminate(st);
  }
}

/* ----------------------------------------------------------------
   Destroy
   ---------------------------------------------------------------- */

void logReaderDestroy(LogReader *reader) {
  if (!reader) return;

  logReaderClose(reader);

  if (reader->sapiState) {
    free(reader->sapiState);
    reader->sapiState = NULL;
  }

  free(reader);
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
