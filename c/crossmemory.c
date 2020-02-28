

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


#ifdef METTLE
#include <metal/metal.h>
#include <metal/limits.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "cellpool.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "lpa.h"
#include "nametoken.h"
#include "zos.h"
#include "printables_for_dump.h"
#include "qsam.h"
#include "resmgr.h"
#include "stcbase.h"
#include "utils.h"
#include "zvt.h"

#define CROSS_MEMORY_SERVER_MIN_NAME_LENGTH   4
#define CROSS_MEMORY_SERVER_MAX_NAME_LENGTH   16

#ifndef CROSS_MEMORY_SERVER_RACF_PROFILE
#define CROSS_MEMORY_SERVER_RACF_PROFILE CMS_PROD_ID".IS"
#endif

#ifdef CROSS_MEMORY_SERVER_DEBUG
  #ifndef CROSS_MEMORY_DEFAULT_SERVER_NAME
  #define CROSS_MEMORY_DEFAULT_SERVER_NAME  CMS_PROD_ID"IS_DEBUG    "
  #endif
#endif

#ifndef CROSS_MEMORY_DEFAULT_SERVER_NAME
#define CROSS_MEMORY_DEFAULT_SERVER_NAME    CMS_PROD_ID"IS_STD      "
#endif

const CrossMemoryServerName CMS_DEFAULT_SERVER_NAME = {CROSS_MEMORY_DEFAULT_SERVER_NAME};

#define CROSS_MEMORY_SERVER_LOG_SERVICE_ID    1
#define CROSS_MEMORY_SERVER_DUMP_SERVICE_ID   2
#define CROSS_MEMORY_SERVER_CONFIG_SERVICE_ID 3
#define CROSS_MEMORY_SERVER_STATUS_SERVICE_ID 4

#define TOSTR(number) #number
#define INT_TO_STR(number) TOSTR(number)

const char *CMS_RC_DESCRIPTION[] = {
  [RC_CMS_OK] = "Ok",
  [RC_CMS_SERVER_NOT_READY] = "Server is not running",
  [RC_CMS_PERMISSION_DENIED] = "Permission denied",
  [RC_CMS_SERVER_ABENDED] = "Cross-memory call ABENDed",
  [RC_CMS_WRONG_SERVER_VERSION] = "Wrong server version",
  [RC_CMS_WRONG_CLIENT_VERSION] = "Wrong client version",
  [RC_CMS_SERVER_NAME_NULL] = "Server name is NULL",
  [RC_CMS_MAX_RC + 1] = NULL
};

#define CMS_DDNAME  "STEPLIB "
#ifndef CMS_MODULE
#define CMS_MODULE  CMS_PROD_ID"IS01"
#endif

static void eightCharStringInit(EightCharString *string, char *cstring, char padChar) {
  memset(string->text, padChar, sizeof(string->text));
  unsigned int cstringLength = strlen(cstring);
  unsigned int copyLength = cstringLength > sizeof(string->text) ? sizeof(string->text) : cstringLength;
  memcpy(string->text, cstring, copyLength);
}

/* This is the QNAME used for all product enqueues. It must NEVER change. */
static const QName PRODUCT_QNAME   = {CMS_PROD_ID"    "};
/* These QNAME and RNAME must NEVER change and be the same across all
 * cross-memory servers. They are used for the ZVTE chain serialization. */
static const QName ZVTE_QNAME  = {CMS_PROD_ID"    "};
static const RName ZVTE_RNAME  = {8, "ISZVTE  "};

#define CMS_MAX_ZVTE_CHAIN_LENGTH   128

#if defined(METTLE) && !defined(CMS_CLIENT)

ZOWE_PRAGMA_PACK

typedef struct MultilineWTOFirstLine_tag {
  unsigned short length;
  unsigned short flags;
  char text[69];
  unsigned char version;
#define MULTILINE_WTO_VERSION 2
  unsigned char miscFlags;
  unsigned char replyLength;
  unsigned char wpxLength;
  unsigned short extendedFlags;
  char reserved2[2];
  char * __ptr32 replyBuffer;
  int  * __ptr32 replyECB;
  unsigned int connectID;
  unsigned short descriptorCodes;
  char reserved3[2];
  char extendedRoutingCodes[16];
  unsigned short messageType;
  unsigned short messagePriority;
  char jobID[8];
  char jobName[8];
  char retrievalKey[8];
  unsigned int tokenForDOM;
  unsigned int consoleID;
  char systemName[8];
  char consoleName[8];
  void * __ptr32 replyConsoleID;
  void * __ptr32 cart;
  void * __ptr32 wsparm;
  unsigned short type;
#define MULTILINE_WTO_LINE_TYPE_E     0x1000
#define MULTILINE_WTO_LINE_TYPE_D     0x2000
#define MULTILINE_WTO_LINE_TYPE_DE    0x3000
  unsigned char areaID;
  unsigned char lineCount;
#define MULTILINE_WTO_LINE_MAX_COUNT  255
} MultilineWTOFirstLine;

typedef struct MultilineWTONLine_tag {
  unsigned short length;
  unsigned short type;
  char text[69];
} MultilineWTONLine;

typedef struct MultilineWTOHandle_tag {
  char eyecatcher[8];
#define MLINE_WTO_HANDLER_EYECATCHER  "RSMLWTOH"
  unsigned int handleSize;
  unsigned int maxLineCount;
  unsigned int lineCount;
  MultilineWTOFirstLine firstLine;
  MultilineWTONLine lines[0];
} MultilineWTOHandle;

ZOWE_PRAGMA_PACK_RESET

static MultilineWTOHandle *wtoGetMultilineHandle(unsigned int maxLineCount) {

  if (maxLineCount > MULTILINE_WTO_LINE_MAX_COUNT) {
    maxLineCount = MULTILINE_WTO_LINE_MAX_COUNT;
  }

  unsigned int handleSize = sizeof(MultilineWTOHandle) + sizeof(MultilineWTONLine) * maxLineCount;
  MultilineWTOHandle *handle =
      (MultilineWTOHandle *)safeMalloc31(handleSize, "Multiline WTO handle");
  if (handle == NULL) {
    return NULL;
  }
  memset(handle, 0, handleSize);
  memcpy(handle->eyecatcher, MLINE_WTO_HANDLER_EYECATCHER, sizeof(handle->eyecatcher));
  handle->handleSize = handleSize;
  handle->maxLineCount = maxLineCount;

  handle->firstLine.length = 0;
  handle->firstLine.flags = WTO_IS_MULTILINE | WTO_EXTENDED_WPL;
  handle->firstLine.version = MULTILINE_WTO_VERSION;
  handle->firstLine.wpxLength = 104; /* from a WTO macro expansion  */
  handle->firstLine.type = MULTILINE_WTO_LINE_TYPE_D;
  handle->firstLine.lineCount = 0;
  handle->lineCount = 0;

  memset(handle->firstLine.jobID, ' ', sizeof(handle->firstLine.jobID));
  memset(handle->firstLine.jobName, ' ', sizeof(handle->firstLine.jobName));
  memset(handle->firstLine.retrievalKey, ' ', sizeof(handle->firstLine.retrievalKey));
  memset(handle->firstLine.systemName, ' ', sizeof(handle->firstLine.systemName));
  memset(handle->firstLine.consoleName, ' ', sizeof(handle->firstLine.consoleName));

  return handle;
}

static void wtoAddLine(MultilineWTOHandle *handle, const char *formatString, ...) {

  if (handle->lineCount + 1 > handle->maxLineCount) {
    return;
  }

  char *msgBuffer = NULL;
  if (handle->lineCount == 0) {
    memset(handle->firstLine.text, ' ', sizeof(handle->firstLine.text));
    handle->firstLine.length = sizeof(handle->firstLine.text) + 4;
    handle->firstLine.type = MULTILINE_WTO_LINE_TYPE_D;
    msgBuffer = handle->firstLine.text;
  } else {
    MultilineWTONLine *nextLine = &handle->lines[handle->lineCount - 1];
    memset(nextLine->text, ' ', sizeof(nextLine->text));
    nextLine->length = sizeof(nextLine->text) + 4;
    nextLine->type = MULTILINE_WTO_LINE_TYPE_D;
    msgBuffer = nextLine->text;
  }
  handle->lineCount++;
  handle->firstLine.lineCount++;

  va_list argPointer;

  va_start(argPointer,formatString);
  unsigned int charsToBeWritten = vsnprintf(msgBuffer, sizeof(handle->firstLine.text) + 1, formatString, argPointer);
  va_end(argPointer);

  if (charsToBeWritten < sizeof(handle->firstLine.text)) {
    msgBuffer[charsToBeWritten] = ' ';
  }

}

static void wtoPrintMultilineMessage(MultilineWTOHandle *handle, int consoleID, CART cart) {

#define WTO_EXT_FLAG_CONSID_SPECIFIED 0x4000

  MultilineWTONLine *finalLine = &handle->lines[handle->lineCount];
  handle->firstLine.consoleID = consoleID;
  if (consoleID != 0) {
    handle->firstLine.extendedFlags |= WTO_EXT_FLAG_CONSID_SPECIFIED;
  } else {
    handle->firstLine.extendedFlags &= ~WTO_EXT_FLAG_CONSID_SPECIFIED;
  }
  handle->firstLine.cart = &cart;
  finalLine->length = 0x0;
  finalLine->type = MULTILINE_WTO_LINE_TYPE_E;
  handle->lineCount++;
  handle->firstLine.lineCount++;

  __asm(
      "         XGR   0,0                                                      \n"
      "         WTO   MF=(E,(%0))                                              \n"
      :
      : "r"(&handle->firstLine)
      : "r0", "r1", "r14", "r15"
  );

}

static void wtoRemoveMultilineHandle(MultilineWTOHandle *handle) {
  safeFree31((char *)handle, handle->handleSize);
  handle = NULL;
}

void wtoPrintf2(int consoleID, CART cart, char *formatString, ...) {

  MultilineWTOHandle *handle = wtoGetMultilineHandle(4);
  if (handle == NULL) {
    return;
  }

  char text[sizeof(handle->firstLine.text)];
  va_list argPointer;
  va_start(argPointer,formatString);
  vsnprintf(text, sizeof(text), formatString, argPointer);
  va_end(argPointer);

  wtoAddLine(handle, text);
  wtoPrintMultilineMessage(handle, consoleID, cart);
  wtoRemoveMultilineHandle(handle);
  handle = NULL;

}

static void printWithPrefix(LoggingContext *context, LoggingComponent *component, char* path, int line, int level, uint64 compID, void *data, char *formatString, va_list argList);
static char *dumpWithEmptyMessageID(char *workBuffer, int workBufferSize, void *data, int dataSize, int lineNumber);

void cmsInitializeLogging() {

  logConfigureDestination2(NULL, LOG_DEST_PRINTF_STDOUT, "printf(stdout)", NULL, printWithPrefix, dumpWithEmptyMessageID);
  logConfigureComponent(NULL, LOG_COMP_STCBASE, "STCBASE", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_CMS, "CIDB", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_CMSPC, "CIDB CM", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  printf("DATESTAMP              JOBNAME  ASCB    (ASID) TCB       MSGID     MSGTEXT\n");

}

#endif /* defined(METTLE) && !defined(CMS_CLIENT) */

ZOWE_PRAGMA_PACK

typedef struct LogTimestamp_tag {
  union {
    struct {
      char year[4];
      char delim00[1];
      char month[2];
      char delim01[1];
      char day[2];
      char delim02[1];
      char hour[2];
      char delim03[1];
      char minute[2];
      char delim04[1];
      char second[2];
      char delim05[1];
      char hundredth[2];
    } dateAndTime;
    char text[22];
  };
} LogTimestamp;

typedef struct STCKCONVOutput_tag {
  unsigned char hour;
  unsigned char minute;
  unsigned char second;
  unsigned int  x10      : 4;
  unsigned int  x100     : 4;
  unsigned int  x1000    : 4;
  unsigned int  x10000   : 4;
  unsigned int  x100000  : 4;
  unsigned int  x1000000 : 4;
  char filler1[2];
  unsigned char year[2];
  unsigned char month;
  unsigned char day;
  char filler2[4];
} STCKCONVOutput;

ZOWE_PRAGMA_PACK_RESET

void stckToLogTimestamp(uint64 stck, LogTimestamp *timestamp) {

  memset(timestamp->text, ' ', sizeof(timestamp->text));

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint64 stck;
      char stckconvParmList[256];
      STCKCONVOutput stckconvResult;
      int stckconvRC;
    )
  );
  parms31->stck = stck;
  memset(parms31->stckconvParmList, 0, sizeof(parms31->stckconvParmList));

  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "STCKBGN  LARL  15,STCKBGN                                               \n"
      "         USING STCKBGN,15                                               \n"
      "         LA    1,0                                                      \n"
      "         SAR   1,1                                                      \n"
      "         STCKCONV STCKVAL=(%1)"
      ",CONVVAL=(%2)"
      ",TIMETYPE=DEC"
      ",DATETYPE=YYYYMMDD"
      ",MF=(E,(%3))                                                            \n"
      "         ST 15,%0                                                       \n"
      "         POP USING                                                      \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      : "=m"(parms31->stckconvRC)
      : "r"(&parms31->stck), "r"(&parms31->stckconvResult), "r"(&parms31->stckconvParmList)
      : "r0", "r1", "r14", "r15"
  );

  if (parms31->stckconvRC == 0) {

    /* packed to zoned */
    timestamp->dateAndTime.year[0]        = (parms31->stckconvResult.year[0]   >> 4) | 0xF0;
    timestamp->dateAndTime.year[1]        = (parms31->stckconvResult.year[0]       ) | 0xF0;
    timestamp->dateAndTime.year[2]        = (parms31->stckconvResult.year[1]   >> 4) | 0xF0;
    timestamp->dateAndTime.year[3]        = (parms31->stckconvResult.year[1]       ) | 0xF0;

    timestamp->dateAndTime.delim00[0]     = '/';

    timestamp->dateAndTime.month[0]       = (parms31->stckconvResult.month     >> 4) | 0xF0;
    timestamp->dateAndTime.month[1]       = (parms31->stckconvResult.month         ) | 0xF0;

    timestamp->dateAndTime.delim01[0]     = '/';

    timestamp->dateAndTime.day[0]         = (parms31->stckconvResult.day       >> 4) | 0xF0;
    timestamp->dateAndTime.day[1]         = (parms31->stckconvResult.day           ) | 0xF0;

    timestamp->dateAndTime.delim02[0]     = '-';

    timestamp->dateAndTime.hour[0]        = (parms31->stckconvResult.hour      >> 4) | 0xF0;
    timestamp->dateAndTime.hour[1]        = (parms31->stckconvResult.hour          ) | 0xF0;

    timestamp->dateAndTime.delim03[0]     = ':';

    timestamp->dateAndTime.minute[0]      = (parms31->stckconvResult.minute    >> 4) | 0xF0;
    timestamp->dateAndTime.minute[1]      = (parms31->stckconvResult.minute        ) | 0xF0;

    timestamp->dateAndTime.delim04[0]     = ':';

    timestamp->dateAndTime.second[0]      = (parms31->stckconvResult.second    >> 4) | 0xF0;
    timestamp->dateAndTime.second[1]      = (parms31->stckconvResult.second        ) | 0xF0;

    timestamp->dateAndTime.delim05[0]     = '.';

    timestamp->dateAndTime.hundredth[0]   = ((char)parms31->stckconvResult.x10     ) | 0xF0;
    timestamp->dateAndTime.hundredth[1]   = ((char)parms31->stckconvResult.x100    ) | 0xF0;

  }

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

}

static void getSTCK(uint64 *stckValue) {
  __asm(" STCK 0(%0)" : : "r"(stckValue));
}

int64 getLocalTimeOffset() {
  CVT * __ptr32 cvt = *(void * __ptr32 * __ptr32)0x10;
  void * __ptr32 cvtext2 = cvt->cvtext2;
  int64 *cvtldto = (int64 * __ptr32)(cvtext2 + 0x38);
  return *cvtldto;
}

static void getCurrentLogTimestamp(LogTimestamp *timestamp) {

  uint64 stck = 0;
  getSTCK(&stck);

  stck += getLocalTimeOffset();

  stckToLogTimestamp(stck, timestamp);

}

#define LOG_MSG_PREFIX_SIZE 57

ZOWE_PRAGMA_PACK
typedef struct LogMessagePrefix_tag {
  char text[LOG_MSG_PREFIX_SIZE];
} LogMessagePrefix;

typedef struct CrossMemoryServerLogServiceParm_tag {
  char eyecatcher[8];
#define CMS_LOG_SERVICE_PARM_EYECATCHER "RSCMSMBL"
  union {
    struct {
      LogMessagePrefix prefix;
      char text[256];
    };
    char message[512];
  };
  int messageLength;
} CrossMemoryServerLogServiceParm;

typedef struct CrossMemoryServerMsgQueueElement_tag {
  QueueElement queueElement;
  CPID cellPool;
  char filler0[4];
  CrossMemoryServerLogServiceParm logServiceParm;
} CrossMemoryServerMsgQueueElement;

typedef struct CrossMemoryServerConfigServiceParm_tag {
  char eyecatcher[8];
#define CMS_CONFIG_SERVICE_PARM_EYECATCHER "RSCMSCSY"
  char nameNullTerm[CMS_CONFIG_PARM_MAX_NAME_LENGTH + 1];
  char padding[7];
  CrossMemoryServerConfigParm result;
} CrossMemoryServerConfigServiceParm;

ZOWE_PRAGMA_PACK_RESET

static void initLogMessagePrefix(LogMessagePrefix *prefix) {
  LogTimestamp currentTime;
  getCurrentLogTimestamp(&currentTime);
  ASCB *ascb = getASCB();
  char *jobName = getASCBJobname(ascb);
  TCB *tcb = getTCB();
  snprintf(prefix->text, sizeof(prefix->text), "%22.22s %8.8s %08X(%04X) %08X  ", currentTime.text, jobName, ascb, ascb->ascbasid, tcb);
  prefix->text[sizeof(prefix->text) - 1] = ' ';
}

#define PREFIXED_LINE_MAX_COUNT         1000
#define PREFIXED_LINE_MAX_MSG_LENGTH    4096

static void printWithPrefix(LoggingContext *context, LoggingComponent *component, char* path, int line, int level, uint64 compID, void *data, char *formatString, va_list argList) {

  char messageBuffer[PREFIXED_LINE_MAX_MSG_LENGTH];
  size_t messageLength = vsnprintf(messageBuffer, sizeof(messageBuffer), formatString, argList);

  if (messageLength > sizeof(messageBuffer) - 1) {
    messageLength = sizeof(messageBuffer) - 1;
  }

  LogMessagePrefix prefix;

  char *nextLine = messageBuffer;
  char *lastLine = messageBuffer + messageLength;
  for (int lineIdx = 0; lineIdx < PREFIXED_LINE_MAX_COUNT; lineIdx++) {
    if (nextLine >= lastLine) {
      break;
    }
    char *endOfLine = strchr(nextLine, '\n');
    size_t nextLineLength = endOfLine ? (endOfLine - nextLine) : (lastLine - nextLine);
    memset(prefix.text, ' ', sizeof(prefix.text));
    if (lineIdx == 0) {
      initLogMessagePrefix(&prefix);
    }
    printf("%.*s%.*s\n", sizeof(prefix.text), prefix.text, nextLineLength, nextLine);
    nextLine += (nextLineLength + 1);
  }

}

#define MESSAGE_ID_SHIFT  10

/* this is a copy of the dumper function from logging.c, it's been modified to occupy less space and print an empty message ID */
static char *dumpWithEmptyMessageID(char *workBuffer, int workBufferSize, void *data, int dataSize, int lineNumber) {

  int formatWidth = 16;
  int index = lineNumber * formatWidth;
  int length = dataSize;
  int lastIndex = length - 1;
  const unsigned char *tranlationTable = printableEBCDIC;

  memset(workBuffer, ' ', MESSAGE_ID_SHIFT);
  memcpy(workBuffer, CMS_LOG_DUMP_MSG_ID, strlen(CMS_LOG_DUMP_MSG_ID));
  workBuffer += MESSAGE_ID_SHIFT;
  if (index <= lastIndex){
    int pos;
    int linePos = 0;
    linePos = hexFill(workBuffer, linePos, 0, 8, 2, index);
    for (pos = 0; pos < formatWidth; pos++){
      if (((index + pos) % 4 == 0) && ((index + pos) % formatWidth != 0)){
        if ((index + pos) % 16 == 0){
          workBuffer[linePos++] = ' ';
        }
        workBuffer[linePos++] = ' ';
      }
      if ((index + pos) < length){
        linePos = hexFill(workBuffer, linePos, 0, 2, 0, (0xFF & ((char *) data)[index + pos]));
      } else {
        linePos += sprintf(workBuffer + linePos, "  ");
      }
    }
    linePos += sprintf(workBuffer + linePos, " |");
    for (pos = 0; pos < formatWidth && (index + pos) < length; pos++){
      int ch = tranlationTable[0xFF & ((char *) data)[index + pos]];
      workBuffer[linePos++] = ch;
    }
    workBuffer[linePos++] = '|';
    workBuffer[linePos++] = 0;
    return workBuffer - MESSAGE_ID_SHIFT;
  }

  return NULL;
}

static int callPCRoutine(int pcNumber, int sequenceNumber, void *parmBlock) {

  int returnCode = 0;
  __asm(
      ASM_PREFIX
      "         LMH   15,15,%2                                                 \n"
      "         L     14,%1                                                    \n"
      "         XGR   1,1                                                      \n"
      "         LA    1,0(%3)                                                  \n"
      "         PC    0(14)                                                    \n"
      "         ST    15,%0                                                    \n"
      : "=m"(returnCode)
      : "m"(pcNumber), "m"(sequenceNumber), "r"(parmBlock)
      : "r14"
  );

  return returnCode;
}

#if defined(METTLE) && !defined(CMS_CLIENT)

static int axset() {

  int axsetRC = 0;
  __asm(
      ASM_PREFIX
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         LA    1,1                                                      \n"
      "         AXSET AX=(1)                                                   \n"
      "         ST    15,%0                                                    \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      : "=m"(axsetRC)
      :
      : "r0", "r1", "r14", "r15"
  );

  return axsetRC;
}

__asm("LXRESGLB LXRES MF=L" : "DS"(LXRESGLB));

static int lxres(ELXLIST *elxList) {

  __asm("LXRESGLB LXRES MF=L" : "DS"(lxresParmList));
  lxresParmList = LXRESGLB;

  int lxresRC = 0;
  __asm(
      ASM_PREFIX
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         LXRES ELXLIST=(%1)"
      ",SYSTEM=YES"
      ",REUSABLE=YES"
      ",MF=(E,(%2))                                                            \n"
      "         ST    15,%0                                                    \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      : "=m"(lxresRC)
      : "r"(elxList), "r"(&lxresParmList)
      : "r0", "r1", "r14", "r15"
  );

  return lxresRC;
}

static int buildPCEntryTable(void *__ptr32 pcssRoutine, void *__ptr32 pccpRoutine, const void * __ptr32 parm1, const void * __ptr32 parm2, int *ettoken) {

  char * __ptr32 etdescGlobal = NULL;
  char * __ptr32 etlistGlobal = NULL;

  __asm(
      ASM_PREFIX
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "ETDBGN   LARL  15,ETDBGN                                                \n"
      "         USING ETDBGN,15                                                \n"
      "         J     ETDWORK                                                  \n"

      "ETDESC   ETDEF TYPE=INITIAL                                             \n"

      "ETLIST   DS    0H                                                       \n"

      /* Entry #1 */
      "         ETDEF TYPE=ENTRY"
      ",ROUTINE=0"
      ",SASN=OLD"
#ifdef _LP64
      ",RAMODE=64"
#else
      ",RAMODE=31"
#endif
      ",STATE=SUPERVISOR"
      ",AKM=(0:15)"
      ",EK="INT_TO_STR(CROSS_MEMORY_SERVER_KEY)"                               \n"

      /* Entry #2 */
      "         ETDEF TYPE=ENTRY"
      ",ROUTINE=0"
      ",SASN=OLD"
#ifdef _LP64
      ",RAMODE=64"
#else
      ",RAMODE=31"
#endif
      ",STATE=SUPERVISOR"
      ",AKM=(0:15)"
      ",EK="INT_TO_STR(CROSS_MEMORY_SERVER_KEY)"                               \n"

      "         ETDEF TYPE=FINAL                                               \n"

      "ETDWORK  LA    1,ETDESC                                                 \n"
      "         ST    1,%0                                                     \n"
      "         LA    1,ETLIST                                                 \n"
      "         ST    1,%1                                                     \n"

      "         POP   USING                                                    \n"

      : "=m"(etdescGlobal), "=m"(etlistGlobal)
      :
      : "r0", "r15"
  );

  ZOWE_PRAGMA_PACK

  typedef struct ETD_tag {
    char format;
    char hflag;
    short entryNumber;
  } ETD;

  typedef struct ETDELE_tag {
    char index;
    char flag;
#define ETDELE_FLAG_SUP       0x80
#define ETDELE_FLAG_SSWITCH   0x40
    char reserved[2];
    union {
      char pro[8];
      struct {
        int zeros;
        int address;
      } routine;
    };
    char akm[2];
    char ekm[2];
    char par[4];
    char optb1;
    char ek;
    char eax[2];
    char arr[8];
    char par2[4];
    char lpafl[4];
  } ETDELE;

  typedef struct PCEntryTable_tag {
    ETD header;
    ETDELE pcssEntry;
    ETDELE pccpEntry;
  } PCEntryTable;

  ZOWE_PRAGMA_PACK_RESET

  PCEntryTable entryTable;
  memcpy(&entryTable, etdescGlobal, sizeof(PCEntryTable));

  entryTable.pcssEntry.routine.zeros = 0;
  entryTable.pcssEntry.routine.address = (int)pcssRoutine;
#ifndef _LP64
  entryTable.pcssEntry.routine.address |= 0x80000000;
#endif
  memcpy(&entryTable.pcssEntry.par, &parm1, sizeof(void * __ptr32));
  memcpy(&entryTable.pcssEntry.par2, &parm2, sizeof(void * __ptr32));
  entryTable.pcssEntry.flag |= ETDELE_FLAG_SSWITCH;

  entryTable.pccpEntry.routine.zeros = 0;
  entryTable.pccpEntry.routine.address = (int)pccpRoutine;
#ifndef _LP64
  entryTable.pccpEntry.routine.address |= 0x80000000;
#endif
  memcpy(&entryTable.pccpEntry.par, &parm1, sizeof(void * __ptr32));
  memcpy(&entryTable.pccpEntry.par2, &parm2, sizeof(void * __ptr32));

  int etcreRC = 0;
  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         ETCRE ENTRIES=(%1)                                             \n"
      "         ST    15,%0                                                    \n"
      "         ST    0,0(%2)                                                  \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif

      : "=m"(etcreRC)
      : "r"(&entryTable), "r"(ettoken)
      : "r0", "r1", "r14", "r15"
  );

  return etcreRC;
}

ZOWE_PRAGMA_PACK

typedef struct TokenList_tag {
  int tokenCount;
  int tokenValue[32];
} TokenList;

ZOWE_PRAGMA_PACK_RESET

__asm("ETCONGLB ETCON MF=L" : "DS"(ETCONGLB));

static int etcon(ELXLIST *elxList, TokenList *tokenList) {

  __asm("ETCONGLB ETCON MF=L" : "DS"(etconParmList));
  etconParmList = ETCONGLB;

  int etconRC = 0;
  __asm(
      ASM_PREFIX
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "ETCBGN   LARL  15,ETCBGN                                                \n"
      "         USING ETCBGN,15                                                \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         ETCON TKLIST=(%1)"
      ",ELXLIST=(%2)"
      ",MF=(E,(%3))                                                            \n"
      "         ST    15,%0                                                    \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif

      "         POP   USING                                                    \n"
      : "=m"(etconRC)
      : "r"(tokenList), "r"(elxList), "r"(&etconParmList)
      : "r0", "r1", "r14", "r15"
  );

  return etconRC;
}

typedef int ECB;

static void wait(ECB * __ptr32 ecb) {
  __asm(
      ASM_PREFIX
      "       WAIT  1,ECB=(%0)                                                 \n"
      :
      : "r"(ecb)
      : "r0", "r1", "r14", "r15"
  );
}

int cmsAddConfigParm(CrossMemoryServer *server,
                     const char *name, const void *value,
                     CrossMemoryServerParmType type) {

  ShortLivedHeap *slh = server->slh;

  size_t keyLength = strlen(name);
  if (keyLength > CMS_CONFIG_PARM_MAX_NAME_LENGTH) {
    return RC_CMS_CONFIG_PARM_NAME_TOO_LONG;
  }

  char *keyNullTerm = SLHAlloc(slh, keyLength + 1);
  if (keyNullTerm == NULL) {
    return RC_CMS_SLH_ALLOC_FAILED;
  }

  strcpy(keyNullTerm, name);

  CrossMemoryServerConfigParm *parm =
      (CrossMemoryServerConfigParm *)SLHAlloc(
          slh,
          sizeof(CrossMemoryServerConfigParm)
      );
  if (parm == NULL) {
    return RC_CMS_SLH_ALLOC_FAILED;
  }

  memcpy(parm->eyecatcher, CMS_PARM_EYECATCHER, sizeof(parm->eyecatcher));
  parm->type = type;

  switch (type) {
  case CMS_CONFIG_PARM_TYPE_CHAR:
  {
    const char *charValueNullTerm = value;
    size_t valueLength = strlen(charValueNullTerm);
    if (valueLength > sizeof(parm->charValueNullTerm) - 1) {
      return RC_CMS_CHAR_PARM_TOO_LONG;
    }
    parm->valueLength = valueLength;
    strcpy(parm->charValueNullTerm, charValueNullTerm);
  }
  break;
  default:
    return RC_CMS_UNKNOWN_PARM_TYPE;
  }

  htPut(server->configParms, keyNullTerm, parm);

  return RC_CMS_OK;
}

ZOWE_PRAGMA_PACK

typedef struct RACFEnityName_tag {
  short bufferLength;
  short entityLength;
  char entityName[255];
  char padding[7];
} RACFEnityName;

typedef enum RACFAccessAttribute_tag {
  RACF_ACCESS_ATTRIBUTE_READ = 0x02,
  RACF_ACCESS_ATTRIBUTE_UPDATE = 0x04,
  RACF_ACCESS_ATTRIBUTE_CONTROL = 0x08,
  RACF_ACCESS_ATTRIBUTE_ALTER = 0x08,
} RACFAccessAttribute;

ZOWE_PRAGMA_PACK_RESET

static void racfEntityNameInit(RACFEnityName *string, char *cstring, char padChar) {
  memset(string->entityName, padChar, sizeof(string->entityName));
  unsigned int cstringLength = strlen(cstring);
  unsigned int copyLength = cstringLength > sizeof(string->entityName) ? sizeof(string->entityName) : cstringLength;
  memcpy(string->entityName, cstring, copyLength);
  string->bufferLength = sizeof(string->entityName);
  string->entityLength = copyLength;
}

static bool isSRBCaller() {
  PSA callerPSA;
  cmCopyWithSourceKeyAndALET(&callerPSA, 0x00, 0, CROSS_MEMORY_ALET_HASN,
                             sizeof(PSA));
  return callerPSA.psatold != NULL ? FALSE : TRUE;
}

static unsigned short getMyPASID() {

  unsigned short pasid = 0;
  __asm(
      ASM_PREFIX
      "         LA    %0,0                                                     \n"
      "         EPAR  %0                                                       \n"
      : "=r"(pasid)
      :
      :
  );

  return pasid;
}

static unsigned short getMySASID() {

  unsigned short pasid = 0;
  __asm(
      ASM_PREFIX
      "         LA    %0,0                                                     \n"
      "         ESAR  %0                                                       \n"
      : "=r"(pasid)
      :
      :
  );

  return pasid;
}

static unsigned short getCallersPASID() {

  unsigned short pasid = 0;
  __asm(
      ASM_PREFIX
      "         LA    7,0                                                      \n"
      "         ESTA  6,7                                                      \n"
      "         STH   7,%0                                                     \n"
      : "=m"(pasid)
      :
      : "r6", "r7"
  );

  return pasid;
}

static unsigned short getCallersSASID() {

  unsigned short sasid = 0;
  __asm(
      ASM_PREFIX
      "         LA    7,0                                                      \n"
      "         ESTA  6,7                                                      \n"
      "         STH   6,%0                                                     \n"
      : "=m"(sasid)
      :
      : "r6", "r7"
  );

  return sasid;
}

static bool isSpaceSwitched() {
  return getCallersPASID() != getMyPASID();
}

static int getCurrentKey() {
  int psw = extractPSW();
  int key = (psw >> 20) & 0x0000000F;
  return key;
}

static int getTCBKey() {
  TCB *tcb = getTCB();
  int key = ((int)tcb->tcbpkf >> 4) & 0x0000000F;
  return key;
}

static uint64_t getCallersPSW(void) {

  uint64_t psw;

  __asm(
      ASM_PREFIX
      "         ESTA  6,7                                                      \n"
      "         ST    6,%[psw]                                                 \n"
      "         ST    7,4+%[psw]                                               \n"
      : [psw]"=m"(psw)
      : "NR:r7"(0x01) /* ESTA extraction code #1 - 8 byte PSW */
      : "r6", "r7"
  );

  return psw;
}

static bool isCallerPrivileged(void) {

static const uint64_t pswUserKeyMask        = 0x0080000000000000LLU;
static const uint64_t pswProblemStateMask   = 0x0001000000000000LLU;

  uint64_t psw = getCallersPSW();
  bool isSystemKey        = (psw & pswUserKeyMask)      ? false : true;
  bool isSupervisorState  = (psw & pswProblemStateMask) ? false : true;

  return isSystemKey || isSupervisorState;
}

__asm("GLBRLSTP RACROUTE REQUEST=LIST,MF=L" : "DS"(GLBRLSTP));

static int racrouteLIST(char *className, int *racfRC, int *racfRSN) {

  __asm("GLBRLSTP RACROUTE REQUEST=LIST,MF=L" : "DS"(listParmList));
  listParmList = GLBRLSTP;

  EightCharString class;
  eightCharStringInit(&class, className, ' ');
  char workArea[512];
  memset(workArea, 0, sizeof(workArea));

  int safRC = 0;

  __asm(
      ASM_PREFIX
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         RACROUTE REQUEST=LIST"
      ",ENVIR=CREATE"
      ",WORKA=(%1)"
      ",CLASS=(%2)"
      ",GLOBAL=YES"
      ",RELEASE=2.1"
      ",MF=(E,(%3))                                                            \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ST    15,%0                                                    \n"
      : "=m"(safRC)
      : "r"(&workArea), "r"(&class), "r"(&listParmList)
      : "r0", "r1", "r14", "r15"
  );

  *racfRC = *(int *)&listParmList;
  *racfRSN = *((int *)&listParmList + 1);

  return safRC;
}

__asm("GLBFSTAP RACROUTE REQUEST=FASTAUTH,MF=L" : "DS"(GLBFSTAP));

static int racroutFASTAUTH(ACEE *acee, char *className, char *profileName, RACFAccessAttribute accessLevel,
                           int *racfRC, int *racfRSN, int traceLevel) {

  __asm("GLBFSTAP RACROUTE REQUEST=FASTAUTH,MF=L" : "DS"(fastauthParmList));
  fastauthParmList = GLBFSTAP;

  char fastauthWorkArea[64];
  memset(fastauthWorkArea, 0, sizeof(fastauthWorkArea));
  char workArea[512];
  memset(workArea, 0, sizeof(workArea));

  EightCharString class;
  eightCharStringInit(&class, className, ' ');
  RACFEnityName profile;
  racfEntityNameInit(&profile, profileName, ' ');

  int safRC = 0;

  __asm(
      ASM_PREFIX
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         RACROUTE REQUEST=FASTAUTH"
      ",ACEE=(%1)"
      ",WKAREA=(%2)"
      ",WORKA=(%3)"
      ",CLASS=(%4)"
      ",ENTITYX=(%5)"
      ",ATTR=(%6)"
      ",RELEASE=2.6"
      ",MF=(E,(%7))                                                            \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ST    15,%0                                                    \n"
      : "=m"(safRC)
      : "r"(acee), "r"(&fastauthWorkArea), "r"(&workArea), "r"(&class), "r"(&profile), "r"(accessLevel), "r"(&fastauthParmList)
      : "r0", "r1", "r14", "r15"
  );

  *racfRC = *(int *)&fastauthWorkArea;
  *racfRSN = *((int *)&fastauthWorkArea + 1);

  return safRC;
}

static bool isInternalPCSSCall(CrossMemoryServerGlobalArea *globalArea) {
  return globalArea->serverASID == getMyPASID() && globalArea->serverASID == getMySASID();
}

static bool isAuthCheckRequired(const CrossMemoryServerGlobalArea *globalArea) {
  return globalArea->serverFlags & CROSS_MEMORY_SERVER_FLAG_CHECKAUTH;
}

static bool isCallerAuthorized(CrossMemoryServerGlobalArea *globalArea,
                               bool noSAFRequested) {

  if (noSAFRequested) {
    if (isCallerPrivileged() && !isAuthCheckRequired(globalArea)) {
      return TRUE;
    }
  }

  if (isSRBCaller()) {
    return TRUE;
  }

  if (isInternalPCSSCall(globalArea)) {
    return TRUE;
  }

  ACEE callerACEE;
  ACEE *callerACEEAddr = NULL;
  cmGetCallerAddressSpaceACEE(&callerACEE, &callerACEEAddr);
  if (callerACEEAddr == NULL) {
    return FALSE;
  }
  if (memcmp(callerACEE.aceeacee, "ACEE", sizeof(callerACEE.aceeacee))) {
    return FALSE;
  }

  int racfRC = 0, racfRSN = 0;
  int safRC = racroutFASTAUTH(callerACEEAddr, "FACILITY", CROSS_MEMORY_SERVER_RACF_PROFILE, RACF_ACCESS_ATTRIBUTE_READ, &racfRC, &racfRSN, 0);
  if (safRC != 0) {
    return FALSE;
  }

  return TRUE;
}

ZOWE_PRAGMA_PACK

typedef struct LatentParmList_tag {
  void * __ptr32 parm1;
  void * __ptr32 parm2;
} LatentParmList;

typedef struct PCHandlerParmList_tag {
  char eyecatcher[8];
#define PC_HANDLER_PARM_EYECATCHER  "RSPCHEYE"
  LatentParmList *latentParmList;
  CrossMemoryServerParmList *userParmList;
} PCHandlerParmList;

ZOWE_PRAGMA_PACK_RESET

#define CMS_MAIN_PCSS_STACK_POOL_SIZE       1024
#define CMS_MAIN_PCSS_RECOVERY_POOL_SIZE    8192

#define CMS_STACK_SIZE                      65536
#define CMS_STACK_SUBPOOL                   132

#define CMS_MSG_QUEUE_MAIN_POOL_PSIZE       32768
#define CMS_MSG_QUEUE_MAIN_POOL_SSIZE       4096
#define CMS_MSG_QUEUE_FALLBACK_POOL_PSIZE   16384
#define CMS_MSG_QUEUE_SUBPOOL               132

#ifdef _LP64

#pragma prolog(handlePCSS,\
"        PUSH  USING                                                    \n\
LSSPRLG  LARL  10,LSSPRLG          establish addressability using r10   \n\
         USING LSSPRLG,10                                               \n\
         LGR   5,4                 save latent parm as CPOOL uses r4    \n\
         USING PCLPARM,5                                                \n\
         LLGT  2,PCLPPRM1          load global area from latent parm    \n\
         USING GLA,2                                                    \n\
         SAM31                                                          \n\
         SYSSTATE AMODE64=NO                                            \n\
         CPOOL GET,C,CPID=GLASTCP,REGS=USE   allocate a stack cell      \n\
         SAM64                                                          \n\
         SYSSTATE AMODE64=YES                                           \n\
         DROP  2                   drop, r2 is changed by CPOOL         \n\
         LTGFR 1,1                 check if we have a cell              \n\
         BNZ   LSSINIT             go init, if we have it               \n\
         LA    15,52               if not, leave with a bad RC (52)     \n\
         EREGG 0,1                 restore r0 and r1 before leaving     \n\
         PR                        return to caller                     \n\
LSSINIT  DS    0H                                                       \n\
         LGR   13,1                use new storage as stack in r13      \n\
         USING PCHSTACK,13                                              \n\
         EREGG 1,1                 restore r1 (CMS parm) for PC routine \n\
         MVC   PCHSAEYE,=C'F1SA'   init stack eyecatcher                \n\
         MVC   PCHPLEYE,=C'RSPCHEYE'  init PC parm eyecatcher           \n\
         STG   5,PCHPLLPL          save latent parm in PC parm          \n\
         STG   1,PCHPLUPL          save CMS parm in PC parm             \n\
         LA    1,PCHPARM           make PC parm available to PC routine \n\
         LA    13,PCHSA            adjust stack to start after PC parm  \n\
         DROP  13                  drop, it's no longer PCHSTACK        \n\
         J     LSSRET              go to metal PC routine               \n\
         LTORG                                                          \n\
         DROP  5,10                drop, not needed any more            \n\
LSSRET   DS    0H                                                       \n\
         DROP                                                           \n\
         POP   USING                               ")

#pragma epilog(handlePCSS,\
"        PUSH  USING                                                    \n\
         SLGFI 13,PCHPARML         restore original stack address       \n\
         LGR   3,13                put stack to r3 for CPOOL            \n\
         LGR   4,15                save RC as CPOOL uses r15            \n\
         LARL  10,&CCN_BEGIN       establish addressability             \n\
         USING &CCN_BEGIN,10                                            \n\
         USING PCHSTACK,13                                              \n\
         LG    5,PCHPLLPL          load latent parm from PC parm        \n\
         USING PCLPARM,5                                                \n\
         LLGT  2,PCLPPRM1          load global area from latent parm    \n\
         USING GLA,2                                                    \n\
         SAM31                                                          \n\
         SYSSTATE AMODE64=NO                                            \n\
         CPOOL FREE,CPID=GLASTCP,CELL=(3),REGS=USE  free stack cell     \n\
         SAM64                                                          \n\
         SYSSTATE AMODE64=YES                                           \n\
         DROP  13                  drop, it's no longer valid           \n\
LSSTERM  DS    0H                                                       \n\
         DROP  5,10                drop, not needed any more            \n\
         LGR   15,4                restore RC for caller                \n\
         EREGG 0,1                 restore r0 and r1 for caller         \n\
         PR                        return to caller                     \n\
         DROP                                                           \n\
         POP   USING                               ")

#else

#pragma prolog(handlePCSS,\
"        PUSH  USING                                                    \n\
LSSPRLG  LARL  10,LSSPRLG          establish addressability using r10   \n\
         USING LSSPRLG,10                                               \n\
         LR    5,4                 save latent parm as CPOOL uses r4    \n\
         USING PCLPARM,5                                                \n\
         L     2,PCLPPRM1          load global area from latent parm    \n\
         USING GLA,2                                                    \n\
         CPOOL GET,C,CPID=GLASTCP,REGS=USE   allocate a stack cell      \n\
         DROP  2                   drop, r2 is changed by CPOOL         \n\
         LTR   1,1                 check if we have a cell              \n\
         BNZ   LSSINIT             go init, if we have it               \n\
         LA    15,52               if not, leave with a bad RC (52)     \n\
         EREG  0,1                 restore r0 and r1 before leaving     \n\
         PR                        return to caller                     \n\
LSSINIT  DS    0H                                                       \n\
         LR    13,1                use new storage as stack in r13      \n\
         USING PCHSTACK,13                                              \n\
         EREG  1,1                 restore r1 (CMS parm) for PC routine \n\
         MVC   PCHPLEYE,=C'RSPCHEYE'  init PC parm eyecatcher           \n\
         ST    5,PCHPLLPL          save latent parm in PC parm          \n\
         ST    1,PCHPLUPL          save CMS parm in PC parm             \n\
         LA    1,PCHPARM           make PC parm available to PC routine \n\
         LA    13,PCHSA            adjust stack to start after PC parm  \n\
         DROP  13                  drop, it's no longer PCHSTACK        \n\
         J     LSSRET              go to metal PC routine               \n\
         LTORG                                                          \n\
         DROP  5,10                drop, not needed any more            \n\
LSSRET   DS    0H                                                       \n\
         DROP                                                           \n\
         POP   USING                               ")

#pragma epilog(handlePCSS,\
"        PUSH  USING                                                    \n\
         SLFI  13,PCHPARML         restore original stack address       \n\
         LR    3,13                put stack to r3 for CPOOL            \n\
         LR    4,15                save RC as CPOOL uses r15            \n\
         LARL  10,&CCN_BEGIN       establish addressability             \n\
         USING &CCN_BEGIN,10                                            \n\
         USING PCHSTACK,13                                              \n\
         L     5,PCHPLLPL          load latent parm from PC parm        \n\
         USING PCLPARM,5                                                \n\
         L     2,PCLPPRM1          load global area from latent parm    \n\
         USING GLA,2                                                    \n\
         SAM31                                                          \n\
         CPOOL FREE,CPID=GLASTCP,CELL=(3),REGS=USE  free stack cell     \n\
         SAM64                                                          \n\
         DROP  13                  drop, it's no longer valid           \n\
LSSTERM  DS    0H                                                       \n\
         DROP  5,10                drop, not needed any more            \n\
         LR    15,4                restore RC for caller                \n\
         EREG  0,1                 restore r0 and r1 for caller         \n\
         PR                        return to caller                     \n\
         DROP                                                           \n\
         POP   USING                               ")

#endif

#ifdef _LP64

#pragma prolog(handlePCCP,\
"        PUSH  USING                                                    \n\
LCPPRLG  LARL  10,LCPPRLG          establish addressability using r10   \n\
         USING LCPPRLG,10                                               \n\
* allocate stack storage                                                \n\
         STORAGE OBTAIN,COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,LOC=31,BNDRY=PAGE \n\
         LTR   15,15               check RC                             \n\
         BZ    LCPINIT             go init if RC=0                      \n\
         LA    15,52               if not, leave with a bad RC (52)     \n\
         EREGG 0,1                 restore r0 and r1 before leaving     \n\
         PR                        return to caller                     \n\
LCPINIT  DS    0H                                                       \n\
         LGR   13,1                use new storage as stack in r13      \n\
         USING PCHSTACK,13                                              \n\
         EREGG 1,1                 restore r1 (CMS parm) for PC routine \n\
         MVC   PCHSAEYE,=C'F1SA'   init stack eyecatcher                \n\
         MVC   PCHPLEYE,=C'RSPCHEYE'  init PC parm eyecatcher           \n\
         STG   4,PCHPLLPL          save latent parm in PC parm          \n\
         STG   1,PCHPLUPL          save CMS parm in PC parm             \n\
         LA    1,PCHPARM           make PC parm available to PC routine \n\
         LA    13,PCHSA            adjust stack to start after PC parm  \n\
         DROP  13                  drop, it's no longer PCHSTACK        \n\
         J     LCPRET              go to metal PC routine               \n\
         LTORG                                                          \n\
         DROP  10                  drop, not needed any more            \n\
LCPRET   DS    0H                                                       \n\
         DROP                                                           \n\
         POP   USING                               ")

#pragma epilog(handlePCCP,\
"        PUSH  USING                                                    \n\
         SLGFI 13,PCHPARML         restore original stack address       \n\
         LGR   1,13                put stack to r3 for CPOOL            \n\
         LGR   2,15                save RC as STORAGE RELEASE uses r15  \n\
         LARL  10,&CCN_BEGIN       establish addressability             \n\
         USING &CCN_BEGIN,10                                            \n\
* free stack storage                                                    \n\
         STORAGE RELEASE,COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,ADDR=(1) \n\
         LTR   15,15               check RC                             \n\
         BZ    LCPTERM             continue normal termination if RC=0  \n\
         LTR   2,2                 already have a bad RC?               \n\
         BNZ   LCPTERM             yes, continue normal termination     \n\
         LA    2,53                if not, indicate release failure (53)\n\
LCPTERM  DS    0H                                                       \n\
         DROP  10                  drop, not needed any more            \n\
         LGR   15,2                restore RC for caller                \n\
         EREGG 0,1                 restore r0 and r1 for caller         \n\
         PR                        return to caller                     \n\
         DROP                                                           \n\
         POP   USING                               ")

#else

#pragma prolog(handlePCCP,\
"        PUSH  USING                                                    \n\
LCPPRLG  LARL  10,LCPPRLG          establish addressability using r10   \n\
         USING LCPPRLG,10                                               \n\
* allocate stack storage                                                \n\
         STORAGE OBTAIN,COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,LOC=31,BNDRY=PAGE \n\
         LTR   15,15               check RC                             \n\
         BZ    LCPINIT             go init if RC=0                      \n\
         LA    15,52               if not, leave with a bad RC (52)     \n\
         EREG  0,1                 restore r0 and r1 before leaving     \n\
         PR                        return to caller                     \n\
LCPINIT  DS    0H                                                       \n\
         LR    13,1                use new storage as stack in r13      \n\
         USING PCHSTACK,13                                              \n\
         EREG  1,1                 restore r1 (CMS parm) for PC routine \n\
         MVC   PCHPLEYE,=C'RSPCHEYE'  init PC parm eyecatcher           \n\
         ST    4,PCHPLLPL          save latent parm in PC parm          \n\
         ST    1,PCHPLUPL          save CMS parm in PC parm             \n\
         LA    1,PCHPARM           make PC parm available to PC routine \n\
         LA    13,PCHSA            adjust stack to start after PC parm  \n\
         DROP  13                  drop, it's no longer PCHSTACK        \n\
         J     LCPRET              go to metal PC routine               \n\
         LTORG                                                          \n\
         DROP  10                  drop, not needed any more            \n\
LCPRET   DS    0H                                                       \n\
         DROP                                                           \n\
         POP   USING                               ")

#pragma epilog(handlePCCP,\
"        PUSH  USING                                                    \n\
         SLFI  13,PCHPARML         restore original stack address       \n\
         LR    1,13                put stack to r3 for CPOOL            \n\
         LR    2,15                save RC as STORAGE RELEASE uses r15  \n\
         LARL  10,&CCN_BEGIN       establish addressability             \n\
         USING &CCN_BEGIN,10                                            \n\
* free stack storage                                                    \n\
         STORAGE RELEASE,COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,ADDR=(1) \n\
         LTR   15,15               check RC                             \n\
         BZ    LCPTERM             continue normal termination if RC=0  \n\
         LTR   2,2                 already have a bad RC?               \n\
         BNZ   LCPTERM             yes, continue normal termination     \n\
         LA    2,53                if not, indicate release failure (53)\n\
LCPTERM  DS    0H                                                       \n\
         DROP  10                  drop, not needed any more            \n\
         LR    15,2                restore RC for caller                \n\
         EREG  0,1                 restore r0 and r1 for caller         \n\
         PR                        return to caller                     \n\
         DROP                                                           \n\
         POP   USING                               ")

#endif

#ifdef _LP64
#define GET_R1()\
({ \
  void *data = NULL;\
  __asm(\
      ASM_PREFIX\
      "         STG   1,%0                                                     \n"\
      : "=m"(data)\
  );\
  data;\
})
#else
#define GET_R1()\
({ \
  void *data = NULL;\
  __asm(\
      ASM_PREFIX\
      "         ST    1,%0                                                     \n"\
      : "=m"(data)\
  );\
  data;\
})
#endif

ZOWE_PRAGMA_PACK
typedef struct PCRoutineEnvironment_tag {
  char eyecatcher[8];
#define PC_ROUTINE_ENV_EYECATCHER  "RSPCEEYE"
  CAA dummyCAA;
  RLETask dummyRLETask;
  char filler0[4];
  RecoveryContext recoveryContext;
} PCRoutineEnvironment;
ZOWE_PRAGMA_PACK_RESET

#define INIT_PC_ENVIRONMENT(cmsGlobalAreaAddr, isSpaceSwitch, envAddr) \
  ({ \
    memset((envAddr), 0, sizeof(PCRoutineEnvironment)); \
    memcpy((envAddr)->eyecatcher, PC_ROUTINE_ENV_EYECATCHER, sizeof((envAddr)->eyecatcher)); \
    (envAddr)->dummyCAA.rleTask = &(envAddr)->dummyRLETask; \
    int returnCode = RC_CMS_OK; \
    __asm(" LA    12,0(,%0) " : : "r"(&(envAddr)->dummyCAA) : ); \
    int recoveryRC = RC_RCV_OK; \
    if (isSpaceSwitch) { \
      recoveryRC = recoveryEstablishRouter2(&(envAddr)->recoveryContext, \
                                            (cmsGlobalAreaAddr)->pcssRecoveryPool, \
                                            RCVR_ROUTER_FLAG_PC_CAPABLE | \
                                            RCVR_ROUTER_FLAG_RUN_ON_TERM); \
    } else { \
      recoveryRC = recoveryEstablishRouter(RCVR_ROUTER_FLAG_PC_CAPABLE | \
                                           RCVR_ROUTER_FLAG_RUN_ON_TERM); \
    } \
    if (recoveryRC != RC_RCV_OK) { \
      returnCode = RC_CMS_ERROR; \
    } \
    returnCode; \
  })

#define TERMINATE_PC_ENVIRONMENT(envAddr) \
  ({ \
    int returnCode = RC_CMS_OK; \
    int recoveryRC = recoveryRemoveRouter(); \
    if (recoveryRC != RC_RCV_OK) { \
      returnCode = RC_CMS_ERROR; \
    } \
    __asm(" LA    12,0 "); \
    memset((envAddr), 0, sizeof(PCRoutineEnvironment)); \
    returnCode; \
  })

void post(ECB * __ptr32 ecb, int code) {
  __asm(
      ASM_PREFIX
      "         POST (%0),(%1),LINKAGE=SYSTEM                                  \n"
      :
      : "r"(ecb), "r"(code)
      : "r0", "r1", "r14", "r15"
  );
}

static int allocateMsgQueueElement(CrossMemoryServer *server,
                                   CrossMemoryServerMsgQueueElement **result) {

  CPID cpid = CPID_NULL;
  CrossMemoryServerMsgQueueElement *element = NULL;
  bool isConditional = isCallerLocked();

  int recoveryRC = recoveryPush(
      "allocateMsgQueueElement()",
      RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
      "ABEND in CPOOL GET for msg queue element",
      NULL, NULL,
      NULL, NULL
  );

  if (recoveryRC == RC_RCV_OK) {

    cpid = server->messageQueueMainPool;
    element = cellpoolGet(cpid, isConditional);

    if (element == NULL) {
      cpid = server->messageQueueFallbackPool;
      element = cellpoolGet(cpid, true);
    }

  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  if (element) {
    memset(element, 0, sizeof(*element));
    element->cellPool = cpid;
  } else {
    return RC_CMS_NO_STORAGE_FOR_MSG;
  }

  *result = element;

  return RC_CMS_OK;
}

static void freeMsgQueueElement(CrossMemoryServerMsgQueueElement *element) {
  cellpoolFree(element->cellPool, element);
}

static int handleLogService(CrossMemoryServer *server, CrossMemoryServerLogServiceParm *callerParm) {

  if (callerParm == NULL) {
    return RC_CMS_STDSVC_PARM_NULL;
  }

  CrossMemoryServerLogServiceParm localParm;
  cmCopyFromSecondaryWithCallerKey(&localParm, callerParm,
                                   sizeof(CrossMemoryServerLogServiceParm));

  if (memcmp(localParm.eyecatcher, CMS_LOG_SERVICE_PARM_EYECATCHER,
             sizeof(localParm.eyecatcher))) {
    return RC_CMS_STDSVC_PARM_BAD_EYECATCHER;
  }

  CrossMemoryServerMsgQueueElement *newElement = NULL;
  int allocRC = allocateMsgQueueElement(server, &newElement);
  if (allocRC != RC_CMS_OK) {
    return allocRC;
  }

  newElement->logServiceParm = localParm;

  /* log service is always space switch */
  qEnqueue(server->messageQueue, &newElement->queueElement);

  return RC_CMS_OK;
}

static int handleConfigService(CrossMemoryServer *server,
                               CrossMemoryServerConfigServiceParm *callerParm) {

  if (callerParm == NULL) {
    return RC_CMS_STDSVC_PARM_NULL;
  }

  CrossMemoryServerConfigServiceParm localParm;
  cmCopyFromSecondaryWithCallerKey(&localParm, callerParm,
                                   sizeof(CrossMemoryServerConfigServiceParm));

  if (memcmp(localParm.eyecatcher, CMS_CONFIG_SERVICE_PARM_EYECATCHER,
             sizeof(localParm.eyecatcher))) {
    return RC_CMS_STDSVC_PARM_BAD_EYECATCHER;
  }

  localParm.nameNullTerm[sizeof(localParm.nameNullTerm) - 1] = '\0';

  CrossMemoryServerConfigParm *configParm =
      htGet(server->configParms, localParm.nameNullTerm);
  if (configParm == NULL) {
    return RC_CMS_CONFIG_PARM_NOT_FOUND;
  }

  cmCopyToSecondaryWithCallerKey(&callerParm->result, configParm,
                                 sizeof(callerParm->result));

  return RC_CMS_OK;
}

static bool isStandardService(int serviceID) {

  if (0 < serviceID && serviceID < CROSS_MEMORY_SERVER_MIN_SERVICE_ID) {
    return TRUE;
  }

  return FALSE;
}

static int handleStandardService(CrossMemoryServer *server, CrossMemoryServerParmList *localParmList) {

  int status = RC_CMS_OK;

  switch (localParmList->serviceID) {
  case CROSS_MEMORY_SERVER_LOG_SERVICE_ID:
    status = handleLogService(server, localParmList->callerData);
    break;
  case CROSS_MEMORY_SERVER_DUMP_SERVICE_ID:
    status = RC_CMS_PC_NOT_IMPLEMENTED;
    break;
  case CROSS_MEMORY_SERVER_CONFIG_SERVICE_ID:
    status = handleConfigService(server, localParmList->callerData);
    break;
  case CROSS_MEMORY_SERVER_STATUS_SERVICE_ID:
    status = RC_CMS_OK;
    break;
  }

  return status;
}

static int handleUnsafeProgramCall(PCHandlerParmList *parmList,
                                   bool isSpaceSwitchPC) {

  LatentParmList *latentParmList = parmList->latentParmList;
  CrossMemoryServerGlobalArea *globalArea = latentParmList->parm1;

  CrossMemoryServerParmList *userParmList = parmList->userParmList;
  if (userParmList == NULL) {
    return RC_CMS_USER_PARM_NULL;
  }

  CrossMemoryServerParmList localParmList;
  if (isSpaceSwitchPC) {
    cmCopyFromSecondaryWithCallerKey(&localParmList, userParmList, sizeof(CrossMemoryServerParmList));
  } else {
    cmCopyFromPrimaryWithCallerKey(&localParmList, userParmList, sizeof(CrossMemoryServerParmList));
  }

  if (memcmp(localParmList.eyecatcher, CMS_PARMLIST_EYECATCHER, sizeof(localParmList.eyecatcher))) {
    return RC_CMS_PARM_BAD_EYECATCHER;
  }

  if (localParmList.version != CROSS_MEMORY_SERVER_VERSION) {
    return RC_CMS_WRONG_CLIENT_VERSION;
  }

  if (localParmList.serviceID <= 0 || CROSS_MEMORY_SERVER_MAX_SERVICE_ID < localParmList.serviceID) {
    return RC_CMS_FUNCTION_ID_OUT_OF_RANGE;
  }

  if (!isCallerAuthorized(globalArea,
                          localParmList.flags & CMS_PARMLIST_FLAG_NO_SAF_CHECK))
  {
    return RC_CMS_PERMISSION_DENIED;
  }

  CrossMemoryServer *server = globalArea->localServerAddress;
  if (server == NULL) {
    return RC_CMS_SERVER_NULL;
  }

  CrossMemoryService *service = &globalArea->serviceTable[localParmList.serviceID];
  if (!(service->flags & CROSS_MEMORY_SERVICE_FLAG_INITIALIZED)) {
    return RC_CMS_SERVICE_NOT_INITIALIZED;
  }

  bool isServiceSpaceSwitch = (service->flags & CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH) ? true : false;
  if (isSpaceSwitchPC != isServiceSpaceSwitch) {
    return RC_CMS_IMPROPER_SERVICE_AS;
  }

  if (isStandardService(localParmList.serviceID)) {
    return handleStandardService(globalArea->localServerAddress, &localParmList);
  }

  CrossMemoryServiceFunction *function = service->function;
  if (function == NULL) {
    return RC_CMS_FUNCTION_NULL;
  }

  int returnCode = RC_CMS_OK;

  int pushRC = recoveryPush("CMS service function call",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY | RCVR_FLAG_PRODUCE_DUMP,
                            "RCMS", NULL, NULL, NULL, NULL);

  if (pushRC == RC_RCV_OK) {

    int serviceRC = function(globalArea, service, localParmList.callerData);
    if (isSpaceSwitchPC) {
      cmCopyToSecondaryWithCallerKey(&userParmList->serviceRC, &serviceRC, sizeof(userParmList->serviceRC));
    } else {
      cmCopyToPrimaryWithCallerKey(&userParmList->serviceRC, &serviceRC, sizeof(userParmList->serviceRC));
    }

    recoveryPop();

  } else if (pushRC == RC_RCV_ABENDED) {
    returnCode = RC_CMS_PC_SERVICE_ABEND_DETECTED;
  } else {
    returnCode = RC_CMS_PC_RECOVERY_ENV_FAILED;
  }

  return returnCode;
}

static int handleProgramCall(PCHandlerParmList *parmList, bool isSpaceSwitchPC) {

  if (parmList == NULL) {
    return RC_CMS_PARM_NULL;
  }
  if (memcmp(parmList->eyecatcher, PC_HANDLER_PARM_EYECATCHER,
             sizeof(parmList->eyecatcher))) {
    return RC_CMS_PC_HDLR_PARM_BAD_EYECATCHER;
  }

  LatentParmList *latentParmList = parmList->latentParmList;
  if (latentParmList == NULL) {
    return RC_CMS_LATENT_PARM_NULL;
  }

  CrossMemoryServerGlobalArea *globalArea = latentParmList->parm1;
  if (globalArea == NULL) {
    return RC_CMS_GLOBAL_AREA_NULL;
  }
  if (memcmp(globalArea->eyecatcher, CMS_GLOBAL_AREA_EYECATCHER,
             sizeof(globalArea->eyecatcher))) {
    return RC_CMS_GLOBAL_AREA_BAD_EYECATCHER;
  }

  PCRoutineEnvironment env;
  int envRC = INIT_PC_ENVIRONMENT(globalArea, isSpaceSwitchPC, &env);
  if (envRC != RC_CMS_OK) {
    return RC_CMS_PC_ENV_NOT_ESTABLISHED;
  }

  int returnCode = RC_CMS_OK;

  int pushRC = recoveryPush("CMS PC handler",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "RCMS", NULL, NULL, NULL, NULL);

  if (pushRC == RC_RCV_OK) {

    returnCode = handleUnsafeProgramCall(parmList, isSpaceSwitchPC);

    recoveryPop();

  } else if (pushRC == RC_RCV_ABENDED) {
    returnCode = RC_CMS_PC_SERVICE_ABEND_DETECTED;
  } else {
    returnCode = RC_CMS_PC_RECOVERY_ENV_FAILED;
  }

  envRC = TERMINATE_PC_ENVIRONMENT(&env);
  if (envRC != RC_CMS_OK) {
    returnCode = (returnCode != RC_CMS_OK) ? returnCode : RC_CMS_PC_ENV_NOT_TERMINATED;
  }

  return returnCode;
}

static int handlePCSS() {
  PCHandlerParmList *parmList = GET_R1();
  return handleProgramCall(parmList, true);
}

static int handlePCCP() {
  PCHandlerParmList *parmList = GET_R1();
  return handleProgramCall(parmList, false);
}

static void *allocateECSAStorage(unsigned int size) {
  return cmAlloc(size, CROSS_MEMORY_SERVER_SUBPOOL, CROSS_MEMORY_SERVER_KEY);
}

static void freeECSAStorage(void *data, unsigned int size) {
  cmFree(data, size, CROSS_MEMORY_SERVER_SUBPOOL, CROSS_MEMORY_SERVER_KEY);
}

static void allocateECSAStorage2(unsigned int size,
                                 void **resultPtr) {

  cmAlloc2(size, CROSS_MEMORY_SERVER_SUBPOOL, CROSS_MEMORY_SERVER_KEY,
           resultPtr);

}

static void freeECSAStorage2(void **dataPtr, unsigned int size) {

  cmFree2(dataPtr, size, CROSS_MEMORY_SERVER_SUBPOOL, CROSS_MEMORY_SERVER_KEY);

}

void *cmsAllocateECSAStorage(CrossMemoryServerGlobalArea *globalArea, unsigned int size) {

  if (globalArea->ecsaBlockCount >= CMS_MAX_ECSA_BLOCK_NUMBER) {
    return NULL;
  }

  if (size > CMS_MAX_ECSA_BLOCK_SIZE) {
    return NULL;
  }

  atomicIncrement(&globalArea->ecsaBlockCount, 1);

  return allocateECSAStorage(size);
}

void cmsAllocateECSAStorage2(CrossMemoryServerGlobalArea *globalArea,
                             unsigned int size, void **resultPtr)
{

  if (globalArea->ecsaBlockCount >= CMS_MAX_ECSA_BLOCK_NUMBER) {
    return;
  }

  if (size > CMS_MAX_ECSA_BLOCK_SIZE) {
    return;
  }

  atomicIncrement(&globalArea->ecsaBlockCount, 1);

  allocateECSAStorage2(size, resultPtr);

}

void cmsFreeECSAStorage(CrossMemoryServerGlobalArea *globalArea, void *data, unsigned int size) {

  freeECSAStorage(data, size);

  atomicIncrement(&globalArea->ecsaBlockCount, -1);
}

void cmsFreeECSAStorage2(CrossMemoryServerGlobalArea *globalArea,
                         void **dataPtr, unsigned int size) {

  freeECSAStorage2(dataPtr, size);

  atomicIncrement(&globalArea->ecsaBlockCount, -1);

}

static void *getMyModuleAddressAndSize(unsigned int *size) {

  TCB *tcb = getTCB();

  void *rb = tcb->tcbrbp;

  IHACDE *cde = (IHACDE *)(*(int *)((char *)rb + 0x0C) & 0x00FFFFFF);

  void *xtlst = cde->cdxlmjp;

  void *moduleAddress = *(void * __ptr32 *)((char *)xtlst + 0x0C);
  size_t moduleSize = *(int *)((char *)xtlst + 0x08) & 0x00FFFFFF;

  if (size != NULL) {
    *size = moduleSize;
  }
  return moduleAddress;
}

#define SERVER_HEAP_BLOCK_SIZE        0x10000
#define SERVER_HEAP_MAX_BLOCK_COUNT   10
#define PARM_HT_BACKBONE_SIZE         257

static int allocServerResources(CrossMemoryServer *server) {

  /* TODO move service resources to the corresponding service cleanup functions
     * in the future */

  int status = RC_CMS_OK;

  do {

    server->slh = makeShortLivedHeap(SERVER_HEAP_BLOCK_SIZE,
                                     SERVER_HEAP_MAX_BLOCK_COUNT);
    if (server->slh == NULL) {
      status = RC_CMS_SLH_NOT_CREATED;
      break;
    }

    server->messageQueue = makeQueue(0);
    if (server->messageQueue == NULL) {
      status = RC_CMS_MSG_QUEUE_NOT_CREATED;
      break;
    }

    unsigned msgQueueCellSize =
        cellpoolGetDWordAlignedSize(sizeof(CrossMemoryServerMsgQueueElement));
    server->messageQueueMainPool =
        cellpoolBuild(CMS_MSG_QUEUE_MAIN_POOL_PSIZE,
                      CMS_MSG_QUEUE_MAIN_POOL_SSIZE,
                      msgQueueCellSize,
                      CMS_MSG_QUEUE_SUBPOOL,
                      getCurrentKey(),
                      &(CPHeader){"ZWESCMSMSGMCELLPOOL     "});
    if (server->messageQueueMainPool == CPID_NULL) {
      status = RC_CMS_MSG_QUEUE_NOT_CREATED;
      break;
    }

    server->messageQueueFallbackPool =
        cellpoolBuild(CMS_MSG_QUEUE_FALLBACK_POOL_PSIZE,
                      0,
                      msgQueueCellSize,
                      CMS_MSG_QUEUE_SUBPOOL,
                      getCurrentKey(),
                      &(CPHeader){"ZWESCMSMSGFCELLPOOL     "});
    if (server->messageQueueFallbackPool == CPID_NULL) {
      status = RC_CMS_MSG_QUEUE_NOT_CREATED;
      break;
    }

    server->configParms = htCreate(PARM_HT_BACKBONE_SIZE,
                                   stringHash, stringCompare,
                                   NULL, NULL);
    if (server->configParms == NULL) {
      status = RC_CMS_CONFIG_HT_NOT_CREATED;
      break;
    }

  } while (0);

  if (status != RC_CMS_OK) {

    if (server->configParms != NULL) {
      htDestroy(server->configParms);
      server->configParms = NULL;
    }

    if (server->messageQueueFallbackPool != CPID_NULL) {
      cellpoolDelete(server->messageQueueFallbackPool);
      server->messageQueueFallbackPool = CPID_NULL;
    }

    if (server->messageQueueMainPool != CPID_NULL) {
      cellpoolDelete(server->messageQueueMainPool);
      server->messageQueueMainPool = CPID_NULL;
    }

    if (server->messageQueue != NULL) {
      qRemove(server->messageQueue);
      server->messageQueue = NULL;
    }

    if (server->slh != NULL) {
      SLHFree(server->slh);
      server->slh = NULL;
    }

  }

  return status;
}

static void releaseServerResources(CrossMemoryServer *server) {

  if (server->configParms != NULL) {
     htDestroy(server->configParms);
     server->configParms = NULL;
   }

  if (server->messageQueueFallbackPool != CPID_NULL) {
    cellpoolDelete(server->messageQueueFallbackPool);
    server->messageQueueFallbackPool = CPID_NULL;
  }

  if (server->messageQueueMainPool != CPID_NULL) {
    cellpoolDelete(server->messageQueueMainPool);
    server->messageQueueMainPool = CPID_NULL;
  }

   if (server->messageQueue != NULL) {
     qRemove(server->messageQueue);
     server->messageQueue = NULL;
   }

   if (server->slh != NULL) {
     SLHFree(server->slh);
     server->slh = NULL;
   }

}

static void initStandardServices(CrossMemoryServer *server) {

  /* print service */
  server->serviceTable[CROSS_MEMORY_SERVER_LOG_SERVICE_ID].function = NULL;
  server->serviceTable[CROSS_MEMORY_SERVER_LOG_SERVICE_ID].flags |= CROSS_MEMORY_SERVICE_FLAG_INITIALIZED;
  server->serviceTable[CROSS_MEMORY_SERVER_LOG_SERVICE_ID].flags |= CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH;

  /* dump service */
  server->serviceTable[CROSS_MEMORY_SERVER_DUMP_SERVICE_ID].function = NULL;
  server->serviceTable[CROSS_MEMORY_SERVER_DUMP_SERVICE_ID].flags |= CROSS_MEMORY_SERVICE_FLAG_INITIALIZED;
  server->serviceTable[CROSS_MEMORY_SERVER_DUMP_SERVICE_ID].flags |= CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH;

  /* config service */
  CrossMemoryService *configService =
      &server->serviceTable[CROSS_MEMORY_SERVER_CONFIG_SERVICE_ID];
  configService->function = NULL;
  configService->flags |= CROSS_MEMORY_SERVICE_FLAG_INITIALIZED;
  configService->flags |= CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH;

  /* status service */
  CrossMemoryService *statusService =
      &server->serviceTable[CROSS_MEMORY_SERVER_STATUS_SERVICE_ID];
  statusService->function = NULL;
  statusService->flags |= CROSS_MEMORY_SERVICE_FLAG_INITIALIZED;
  statusService->flags |= CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH;

}

static void termStandardServices(CrossMemoryServer *server) {

  /* TODO implement service cleanup/term functionality */

}

CrossMemoryServer *makeCrossMemoryServer(STCBase *base,
                                         const CrossMemoryServerName *name,
                                         unsigned int flags, int *reasonCode) {

  return makeCrossMemoryServer2(base, name, flags, NULL, NULL, NULL, NULL,
                                reasonCode);

}

CrossMemoryServer *makeCrossMemoryServer2(
    STCBase *base,
    const CrossMemoryServerName *name,
    unsigned int flags,
    CMSStarCallback *startCallback,
    CMSStopCallback *stopCallback,
    CMSModifyCommandCallback *commandCallback,
    void *callbackData,
    int *reasonCode
) {


  int envCheckRC = isEnvironmentReady();
  if (envCheckRC != RC_CMS_OK) {
    *reasonCode = envCheckRC;
    return NULL;
  }

  CrossMemoryServer *server = (CrossMemoryServer *)safeMalloc31(sizeof(CrossMemoryServer), "CrossMemoryServer");
  memset(server, 0, sizeof(CrossMemoryServer));
  memcpy(server->eyecatcher, CMS_EYECATCHER, sizeof(server->eyecatcher));
  server->base = base;
  server->moduleAddressLocal = getMyModuleAddressAndSize(&server->moduleSize);
  server->name = name ? *name : CMS_DEFAULT_SERVER_NAME;
  memcpy(server->ddname.text, CMS_DDNAME, sizeof(server->ddname.text));
  memcpy(server->dsname.text, CMS_MODULE, sizeof(server->dsname.text));

  server->startCallback = startCallback;
  server->stopCallback = stopCallback;
  server->commandCallback = commandCallback;
  server->callbackData = callbackData;

  server->pcssStackPoolSize = CMS_MAIN_PCSS_STACK_POOL_SIZE;
  server->pcssRecoveryPoolSize = CMS_MAIN_PCSS_RECOVERY_POOL_SIZE;

  if (flags & CMS_SERVER_FLAG_DEBUG) {
    logSetLevel(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG);
    logSetLevel(NULL, LOG_COMP_ID_CMSPC, ZOWE_LOG_DEBUG);
  }
  if (flags & CMS_SERVER_FLAG_COLD_START) {
    server->flags |= CROSS_MEMORY_SERVER_FLAG_COLD_START;
  }
  if (flags & CMS_SERVER_FLAG_CHECKAUTH) {
    server->flags |= CROSS_MEMORY_SERVER_FLAG_CHECKAUTH;
  }

  int allocResourcesRC = allocServerResources(server);
  if (allocResourcesRC != RC_CMS_OK) {
    safeFree31((char *)server, sizeof(CrossMemoryServer));
    *reasonCode = allocResourcesRC;
    return NULL;
  }

  initStandardServices(server);

  return server;
}

void removeCrossMemoryServer(CrossMemoryServer *server) {

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" server is about to be removed @ 0x%p\n", server);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, (char *)server, sizeof(CrossMemoryServer));

  termStandardServices(server);

  releaseServerResources(server);

  safeFree31((char *)server, sizeof(CrossMemoryServer));
  server = NULL;

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" server removed @ 0x%p\n", server);

}

void cmsSetPoolParameters(CrossMemoryServer *server,
                          unsigned int pcssStackPoolSize,
                          unsigned int pcssRecoveryPoolSize) {

  server->pcssStackPoolSize = pcssStackPoolSize;
  server->pcssRecoveryPoolSize = pcssRecoveryPoolSize;

}

static bool isServiceEntryAvailable(const CrossMemoryService *entry) {
  if (entry->function == NULL) {
    return true;
  }
  return false;
}

int cmsRegisterService(CrossMemoryServer *server, int id, CrossMemoryServiceFunction *serviceFunction, void *serviceData, int flags) {

  if (id < CROSS_MEMORY_SERVER_MIN_SERVICE_ID || CROSS_MEMORY_SERVER_MAX_SERVICE_ID < id) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_BAD_SERVICE_ID_MSG, id);
    return RC_CMS_FUNCTION_ID_OUT_OF_RANGE;
  }

  if (!isServiceEntryAvailable(&server->serviceTable[id])) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
            CMS_LOG_SRVC_ENTRY_OCCUPIED_MSG, id);
    return RC_CMS_SERVICE_ENTRY_OCCUPIED;
  }

  bool isSpaceSwitch = flags & CMS_SERVICE_FLAG_SPACE_SWITCH ? true : false;
  bool isCurrentPrimary = !isSpaceSwitch;
  bool codeInCommon = flags & CMS_SERVICE_FLAG_CODE_IN_COMMON ? true : false;
  bool relocateToCommon = flags & CMS_SERVICE_FLAG_RELOCATE_TO_COMMON ? true : false;

  bool relocationRequired = (isSpaceSwitch && relocateToCommon && !codeInCommon) || (isCurrentPrimary && !codeInCommon);

  if (relocationRequired) {
    char *moduleAddressLocalStart = server->moduleAddressLocal;
    char *moduleAddressLocalEnd = moduleAddressLocalStart + server->moduleSize;
    char *serviceFunctionLocal = (char *)serviceFunction;
    if (serviceFunctionLocal < moduleAddressLocalStart || moduleAddressLocalEnd < serviceFunctionLocal) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
              CMS_LOG_BAD_SERVICE_ADDR_ID_MSG, id, serviceFunction,
              moduleAddressLocalStart, moduleAddressLocalEnd);
      return RC_CMS_SERVICE_NOT_RELOCATABLE;
    }
  }

  memset(&server->serviceTable[id], 0, sizeof(server->serviceTable[id]));
  server->serviceTable[id].function = serviceFunction;
  server->serviceTable[id].serviceData = serviceData;
  if (relocationRequired) {
    server->serviceTable[id].flags |= CROSS_MEMORY_SERVICE_FLAG_LPA;
  }
  if (isSpaceSwitch) {
    server->serviceTable[id].flags |= CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH;
  }

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" function %d - 0x%p\n", id, serviceFunction);

  return RC_CMS_OK;
}

static int loadServer(CrossMemoryServer *server) {

  int isgenqRC = 0, isgenqRSN = 0;

  QName serverQName = PRODUCT_QNAME;
  RName serverRName;
  int bytesToBeWritten = snprintf(serverRName.value, sizeof(serverRName.value), "IS.%16.16s", server->name.nameSpacePadded);
  if (bytesToBeWritten < 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RNAME_FAILURE_MSG, "Server", "snprintf failed");
    return RC_CMS_SNPRINTF_FAILED;
  }
  if (bytesToBeWritten >= sizeof(serverRName.value)) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RNAME_FAILURE_MSG, "Server", "too long");
    return RC_CMS_SNPRINTF_FAILED;
  }
  serverRName.length = bytesToBeWritten;

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" Server QNAME:\n");
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, &serverQName, sizeof(QName));
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" Server RNAME:\n");
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, &serverRName, sizeof(RName));

  isgenqRC = isgenqGetExclusiveLockOrFail(&serverQName, &serverRName,
                                          ISGENQ_SCOPE_SYSTEM, &server->serverENQToken, &isgenqRSN);
  if (isgenqRC == 4 && (isgenqRSN & 0xFFFF) == 0x0404) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_DUP_SERVER_MSG);
    return RC_CMS_DUPLICATE_SERVER;
  }
  if (isgenqRC > 4) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_SERVER_NOT_LOCKED_MSG, isgenqRC, isgenqRSN);
    return RC_CMS_ISGENQ_FAILED;
  }

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" ISGENQ RC = %d, RSN = 0x%08X\n", isgenqRC, isgenqRSN);

  return RC_CMS_OK;
}

static int startServer(CrossMemoryServer *server) {

  CrossMemoryServerGlobalArea *globalArea = server->globalArea;
  if (globalArea == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_GLB_AREA_NULL_MSG);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, (char *)&server, sizeof(CrossMemoryServer));
    return RC_CMS_GLOBAL_AREA_NULL;
  }

  globalArea->pcLogLevel = logGetLevel(NULL, LOG_COMP_ID_CMSPC);
  globalArea->serverFlags |= CROSS_MEMORY_SERVER_FLAG_READY;
  server->flags |= CROSS_MEMORY_SERVER_FLAG_READY;

  return RC_CMS_OK;
}

static int stopServer(CrossMemoryServer *server) {

  CrossMemoryServerGlobalArea *globalArea = server->globalArea;
  if (globalArea == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_GLB_AREA_NULL_MSG);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, (char *)&server, sizeof(CrossMemoryServer));
    return RC_CMS_GLOBAL_AREA_NULL;
  }

  server->flags &= ~CROSS_MEMORY_SERVER_FLAG_READY;
  globalArea->serverFlags &= ~CROSS_MEMORY_SERVER_FLAG_READY;
  globalArea->localServerAddress = NULL;
  globalArea->serverASID = 0;

  globalArea->pccpHandler = NULL;
  globalArea->pcInfo.pcssPCNumber = 0;
  globalArea->pcInfo.pcssSequenceNumber = 0;
  globalArea->pcInfo.pccpPCNumber = 0;
  globalArea->pcInfo.pccpSequenceNumber = 0;

  globalArea->pcLogLevel = 0;

  memset(globalArea->serviceTable, 0, sizeof(globalArea->serviceTable));

  server->flags |= CROSS_MEMORY_SERVER_FLAG_TERM_ENDED;
  globalArea->serverFlags |= CROSS_MEMORY_SERVER_FLAG_TERM_ENDED;

  return RC_CMS_OK;
}

static void initializeServiceTable(CrossMemoryServer *server) {
  for (int i = CROSS_MEMORY_SERVER_MIN_SERVICE_ID; i < sizeof(server->serviceTable) / sizeof(server->serviceTable[0]); i++) {

    bool relocationRequired = false;
    if (!(server->serviceTable[i].flags & CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH)) {
      relocationRequired = true;
    }
    if (server->serviceTable[i].flags & CROSS_MEMORY_SERVICE_FLAG_LPA) {
      relocationRequired = true;
    }

    if (server->serviceTable[i].function != NULL) {

      server->serviceTable[i].flags |= CROSS_MEMORY_SERVICE_FLAG_INITIALIZED;

      if (relocationRequired) {
        char *serviceFunctionLocal = (char *)server->serviceTable[i].function;
        char *moduleAddressLocal = server->moduleAddressLocal;
        char *moduleAddressLPA = server->moduleAddressLPA;

        if (serviceFunctionLocal < moduleAddressLocal ||
            moduleAddressLocal + server->moduleSize < serviceFunctionLocal) {
          zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RELOC_FAILURE_MSG,
              i, serviceFunctionLocal, moduleAddressLocal, moduleAddressLocal + server->moduleSize);
          server->serviceTable[i].function = NULL;
          server->serviceTable[i].flags = 0;
        }

        server->serviceTable[i].function =
            (CrossMemoryServiceFunction * __ptr32)(serviceFunctionLocal - moduleAddressLocal + moduleAddressLPA);
      }

    }

  }
}

static int discardGlobalResources(CrossMemoryServer *server) {

#ifdef CROSS_MEMORY_SERVER_DEBUG

  NameTokenUserName ntName;
  memcpy(ntName.name, &server->name, sizeof(ntName.name));

  CrossMemoryServerToken token = {"        ", NULL, 0};
  if (!nameTokenRetrieve(NAME_TOKEN_LEVEL_SYSTEM, &ntName, &token.ntToken)) {

    int deleteTokenRC = nameTokenDelete(NAME_TOKEN_LEVEL_SYSTEM, &ntName);
    if (deleteTokenRC != 0) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_NTP_DELETE_FAILURE_MSG, deleteTokenRC);
      return RC_CMS_NAME_TOKEN_DELETE_FAILED;
    }

  }

  return RC_CMS_OK;

#else

  ZVT *zvt = zvtGet();
  if (zvt == NULL) {
    return RC_CMS_ZVT_NULL;
  }

  ENQToken lockToken;
  int lockRC = 0, lockRSN = 0;
  lockRC = isgenqGetExclusiveLock(&ZVTE_QNAME, &ZVTE_RNAME, ISGENQ_SCOPE_SYSTEM, &lockToken, &lockRSN);
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" ZVTE ISGENQ LOCK RC = %d, RSN = 0x%08X:\n", lockRC, lockRSN);
  if (lockRC > 4) {
    return RC_CMS_ZVTE_CHAIN_NOT_LOCKED;
  }

  int status = RC_CMS_OK;

  CrossMemoryServerGlobalArea *globalArea = NULL;
  int getRC = cmsGetGlobalArea(&server->name, &globalArea);
  if (getRC != RC_CMS_OK) {
    status = getRC;
  }

  if (status == RC_CMS_OK) {
    globalArea->version = CROSS_MEMORY_SERVER_DISCARDED_VERSION;
  }


  int unlockRC = 0, unlockRSN = 0;
  unlockRC = isgenqReleaseLock(&lockToken, &unlockRSN);
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" ZVTE ISGENQ RELEASE RC = %d, RSN = 0x%08X:\n", unlockRC, unlockRSN);
  if (unlockRC > 4) {
    status = (status != RC_CMS_OK) ? status : RC_CMS_ZVTE_CHAIN_NOT_RELEASED;
  }

  return status;

#endif /* CROSS_MEMORY_SERVER_DEBUG */

}

static int initSecuritySubsystem(CrossMemoryServer *server) {

  /* We must require the FACILITY class to be RACLISTed and not do that
   * ourselves.
  int racfRC = 0, racfRSN = 0;
  int safRC = racrouteLIST("FACILITY", &racfRC, &racfRSN);

  if (safRC != 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RACROUTE_LIST_FAILURE_MSG, safRC, racfRC, racfRSN);
    return RC_CMS_RACF_ROUTE_LIST_FAILED;
  }
  */

  return RC_CMS_OK;
}

static int addGlobalArea(const CrossMemoryServerName *serverName, CrossMemoryServerGlobalArea *globalArea) {

#ifdef CROSS_MEMORY_SERVER_DEBUG

  NameTokenUserName ntName;
  memcpy(ntName.name, serverName, sizeof(ntName.name));

  CrossMemoryServerToken token = {"        ", NULL, 0};
  if (!nameTokenRetrieve(NAME_TOKEN_LEVEL_SYSTEM, &ntName, &token.ntToken)) {

    int deleteTokenRC = nameTokenDelete(NAME_TOKEN_LEVEL_SYSTEM, &ntName);
    if (deleteTokenRC != 0) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_NTP_DELETE_FAILURE_MSG, deleteTokenRC);
      return RC_CMS_NAME_TOKEN_DELETE_FAILED;
    }

  }

  if (globalArea != NULL) {
    memset(&token, 0, sizeof(CrossMemoryServerToken));
    memcpy(token.eyecatcher, CMS_TOKEN_EYECATCHER, sizeof(token.eyecatcher));
    token.globalArea = globalArea;

    int createTokenRC = nameTokenCreatePersistent(NAME_TOKEN_LEVEL_SYSTEM, &ntName, &token.ntToken);
    if (createTokenRC != 0) {
      return RC_CMS_NAME_TOKEN_CREATE_FAILED;
    }
  }

  return RC_CMS_OK;

#else

  ZVT *zvt = zvtGet();
  if (zvt == NULL) {
    return RC_CMS_ZVT_NULL;
  }

  int status = RC_CMS_OK;

  ENQToken lockToken;
  int lockRC = 0, lockRSN = 0;
  lockRC = isgenqGetExclusiveLock(&ZVTE_QNAME, &ZVTE_RNAME, ISGENQ_SCOPE_SYSTEM, &lockToken, &lockRSN);
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" ZVTE ISGENQ LOCK RC = %d, RSN = 0x%08X:\n", lockRC, lockRSN);
  if (lockRC > 4) {
    return RC_CMS_ZVTE_CHAIN_NOT_LOCKED;
  }

  ZVTEntry *firstZVTE = zvt->zvteSlots.zis;
  ZVTEntry *newZVTE = zvtAllocEntry();
  if (newZVTE == NULL) {
    status = RC_CMS_ZVTE_NOT_ALLOCATED;
  }

  if (status == RC_CMS_OK) {
    int wasProblemState = supervisorMode(TRUE);
    int oldKey = setKey(0);
    newZVTE->next = firstZVTE;
    if (firstZVTE != NULL) {
      firstZVTE->prev = newZVTE;
    }
    newZVTE->productAnchor = globalArea;
    zvt->zvteSlots.zis = newZVTE;
    setKey(oldKey);
    if (wasProblemState) {
      supervisorMode(FALSE);
    }
  }

  int unlockRC = 0, unlockRSN = 0;
  unlockRC = isgenqReleaseLock(&lockToken, &unlockRSN);
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" ZVTE ISGENQ RELEASE RC = %d, RSN = 0x%08X:\n", unlockRC, unlockRSN);
  if (unlockRC > 4) {
    status = (status != RC_CMS_OK) ? status : RC_CMS_ZVTE_CHAIN_NOT_RELEASED;
  }

  return status;
#endif
}

/* This timestamp value will be updated every time this file gets re-assembled */
static CMSBuildTimestamp getServerBuildTimestamp() {

  CMSBuildTimestamp timestamp = {0};

  __asm(
      ASM_PREFIX

      "         MACRO                                                          \n"
      "&LABEL   GENTIMST                                                       \n"
      "MODTIME  DS    0D                                                       \n"
      "         DC    CL26'&SYSCLOCK'                                          \n"
      "         DC    CL6' '                                                   \n"
      "         MEND  ,                                                        \n"

      "L$MDTBGN DS    0H                                                       \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         LARL  10,L$MDTBGN                                              \n"
      "         USING L$MDTBGN,10                                              \n"

      "         MVC   0(32,%[timestamp]),MODTIME                               \n"
      "         J     L$MDTEXT                                                 \n"
      "         GENTIMST                                                       \n"

      "L$MDTEXT DS    0H                                                       \n"
      "         POP   USING                                                    \n"
      :
      : [timestamp]"r"(&timestamp)
      : "r10"
  );

  return timestamp;
}

static int allocateGlobalResources(CrossMemoryServer *server) {

#ifndef CROSS_MEMORY_SERVER_DEBUG
  zvtInit();
#endif

  CrossMemoryServerGlobalArea *globalArea = NULL;
  int getRC = cmsGetGlobalArea(&server->name, &globalArea);
  if (getRC != RC_CMS_OK && getRC != RC_CMS_GLOBAL_AREA_NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_GLB_AREA_NOT_SET_MSG, getRC);
    return getRC;
  }
  char * __ptr32 moduleAddressLPA = NULL;

  if (globalArea == NULL) {

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" global area not found\n");

    globalArea = allocateECSAStorage(sizeof(CrossMemoryServerGlobalArea));
    if (globalArea == NULL) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_ALLOC_FAILURE_MSG, "global area", sizeof(CrossMemoryServerGlobalArea));
      isgenqReleaseLock(&server->serverENQToken, NULL);
      return RC_CMS_ECSA_ALLOC_FAILED;
    }
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" global area allocated @ 0x%p\n", globalArea);

    memset(globalArea, 0, sizeof(CrossMemoryServerGlobalArea));
    memcpy(globalArea->eyecatcher, CMS_GLOBAL_AREA_EYECATCHER, sizeof(globalArea->eyecatcher));
    globalArea->version = CROSS_MEMORY_SERVER_VERSION;
    globalArea->key = CROSS_MEMORY_SERVER_KEY;
    globalArea->subpool = CROSS_MEMORY_SERVER_SUBPOOL;
    globalArea->size = sizeof(CrossMemoryServerGlobalArea);

    int setRC = addGlobalArea(&server->name, globalArea);
    if (setRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_GLB_AREA_NOT_SET_MSG, setRC);
      freeECSAStorage(globalArea, sizeof(CrossMemoryServerGlobalArea));
      globalArea = NULL;
      return setRC;
    }

  } else {

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" global area @ 0x%p\n", globalArea);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, (char *)globalArea, globalArea->size);

    globalArea->serverFlags = 0;

    moduleAddressLPA =
        globalArea->lpaModuleInfo.outputInfo.stuff.successInfo.loadPointAddr;

    if (moduleAddressLPA != NULL) {

      /* Does the module in LPA match our private storage module? */
      CMSBuildTimestamp privateModuleTimestamp = getServerBuildTimestamp();
      if (memcmp(&privateModuleTimestamp, &globalArea->lpaModuleTimestamp,
                 sizeof(CMSBuildTimestamp))) {

        /* No, discard the LPA module. */
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
                CMS_LOG_BUILD_TIME_MISMATCH_MSG, moduleAddressLPA,
                globalArea->lpaModuleTimestamp.value,
                privateModuleTimestamp.value);
        zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
                 (char *)&globalArea->lpaModuleInfo, sizeof(LPMEA));
        wtoPrintf(CMS_LOG_BUILD_TIME_MISMATCH_MSG, moduleAddressLPA,
                  globalArea->lpaModuleTimestamp.value,
                  privateModuleTimestamp.value);

#ifdef CMS_LPA_DEV_MODE
        /* Compiling with CMS_LPA_DEV_MODE will force the server to remove the
         * existing LPA module if the private module doesn't match it.
         * This will help avoid abandoning too many module in LPA during
         * development. */
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_DEBUG_MSG_ID
                " LPA dev mode enabled, issuing CSVDYLPA DELETE\n");
        int lpaDeleteRSN = 0;
        int lpaDeleteRC = lpaDelete(&globalArea->lpaModuleInfo,
                                    &lpaDeleteRSN);
        if (lpaDeleteRC != 0) {
          zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
                  CMS_LOG_LPA_DELETE_FAILURE_MSG, lpaDeleteRC, lpaDeleteRSN);
          return RC_CMS_LPA_DELETE_FAILED;
        }
#endif /* CMS_LPA_DEV_MODE */

        moduleAddressLPA = NULL;
        memset(&globalArea->lpaModuleTimestamp, 0, sizeof(CMSBuildTimestamp));
        memset(&globalArea->lpaModuleInfo, 0, sizeof(LPMEA));

      }

    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_LPMEA_INVALID_MSG);
      zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, (char *)&globalArea->lpaModuleInfo, sizeof(LPMEA));
    }

  }

  /* Update the server information in the global area. */
  server->globalArea = globalArea;
  globalArea->serverName = server->name;
  globalArea->localServerAddress = server;
  globalArea->serverFlags |= server->flags;
  globalArea->serverASID = getMyPASID();

  /* Load the module to LPA if needed, otherwise re-use the existing module. */
  if (moduleAddressLPA == NULL) {

    int lpaAddRSN = 0;
    int lpaAddRC = lpaAdd(&server->lpaCodeInfo, &server->ddname, &server->dsname,
                          &lpaAddRSN);
    if (lpaAddRC != 0) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_LPA_LOAD_FAILURE_MSG, lpaAddRC, lpaAddRSN);
      return RC_CMS_LPA_ADD_FAILED;
    }
    globalArea->lpaModuleInfo = server->lpaCodeInfo;

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID
            " module successfully loaded into LPA @ 0x%p, LPMEA:\n",
            moduleAddressLPA);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG,
             (char *)&server->lpaCodeInfo, sizeof(LPMEA));

  } else {

    server->lpaCodeInfo = globalArea->lpaModuleInfo;

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID
            " LPA module will be re-used @ 0x%p, LPMEA:\n", moduleAddressLPA);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG,
             (char *)&server->lpaCodeInfo, sizeof(LPMEA));

  }

  /* The required module is in LPA, update the corresponding fields. */
  globalArea->lpaModuleTimestamp = getServerBuildTimestamp();
  moduleAddressLPA = server->lpaCodeInfo.outputInfo.stuff.successInfo.loadPointAddr;
  server->moduleAddressLPA = moduleAddressLPA;

  /* Prepare the service table */
  initializeServiceTable(server);
  memcpy(globalArea->serviceTable, server->serviceTable, sizeof(globalArea->serviceTable));

  /* Prepare the PC-cp handler. */
  char *handlerAddressLocal = (char *)handlePCCP;
  char *moduleAddressLocal = (char *)getMyModuleAddressAndSize(NULL);
  char *handlerAddressLPA = handlerAddressLocal - moduleAddressLocal + moduleAddressLPA;
  globalArea->pccpHandler = (int (*)())handlerAddressLPA;

  globalArea->pcssStackPool =
      cellpoolBuild(server->pcssStackPoolSize,
                    0,
                    CMS_STACK_SIZE,
                    CMS_STACK_SUBPOOL,
                    CROSS_MEMORY_SERVER_KEY,
                    &(CPHeader){"ZWESPCSSMCELLPOOL       "});
  if (globalArea->pcssStackPool == CPID_NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RES_NOT_CREATED_MSG,
            "PC-ss state pool", 0);
    return RC_CMS_ALLOC_FAILED;
  }

  globalArea->pcssRecoveryPool =
      recoveryMakeStatePool(server->pcssRecoveryPoolSize);
  if (globalArea->pcssRecoveryPool == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RES_NOT_CREATED_MSG,
            "PC-ss recovery pool", 0);
    return RC_CMS_ALLOC_FAILED;
  }

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" localModuleAddress=0x%08X, handlerAddressLocal=0x%08X, moduleAddressLPA=0x%08X, handlerAddressLPA=0x%08X\n",
      moduleAddressLocal, handlerAddressLocal, moduleAddressLPA, handlerAddressLPA);

  return RC_CMS_OK;
}

static int establishPCRoutines(CrossMemoryServer *server) {

  int axsetRC = axset();
  if (axsetRC != 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_PC_SET_FAILURE_MSG, "ss", "AXSET", axsetRC, 0);
    return RC_CMS_AXSET_FAILED;
  }

  ELXLIST elxList;
  memset(&elxList, 0, sizeof(elxList));
  elxList.elxCount = 1;

  int lxresRC = lxres(&elxList);
  if (lxresRC != 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_PC_SET_FAILURE_MSG, "ss/cp", "LXREC", lxresRC, 0);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, (char *)&elxList, sizeof(ELXLIST));
    return RC_CMS_LXRES_FAILED;
  }

  int ettoken = 0;
  int buildEntryTable = buildPCEntryTable((void * __ptr32)handlePCSS, (void * __ptr32)server->globalArea->pccpHandler, server->globalArea, NULL, &ettoken);
  if (buildEntryTable != 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_PC_SET_FAILURE_MSG, "ss/cp", "Build ET", ettoken, buildEntryTable);
    return RC_CMS_ETCRE_FAILED;
  }

  TokenList tokenList;
  memset(&tokenList, 0, sizeof(tokenList));
  tokenList.tokenCount = 1;
  tokenList.tokenValue[0] = ettoken;

  int etconRC = etcon(&elxList, &tokenList);
  if (etconRC != 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_PC_SET_FAILURE_MSG, "ss/cp", "ETCON", etconRC, 0);
    return RC_CMS_ETCON_FAILED;
  }

  memcpy(&server->pcELXList, &elxList, sizeof(ELXLIST));

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" ELXLIST @ 0x%p\n", &server->pcELXList);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, (char *)&server->pcELXList, sizeof(ELXLIST));

  server->globalArea->pcInfo.pcssPCNumber = elxList.entries[0].pcNumber;
  server->globalArea->pcInfo.pcssSequenceNumber = elxList.entries[0].sequenceNumber;
  server->globalArea->pcInfo.pccpPCNumber = elxList.entries[0].pcNumber + 1;
  server->globalArea->pcInfo.pccpSequenceNumber = elxList.entries[0].sequenceNumber;

  return RC_CMS_OK;
}

static void flushDebugMessages(CrossMemoryServer *server) {

  CrossMemoryServerMsgQueueElement *element =
      (CrossMemoryServerMsgQueueElement *)qDequeue(server->messageQueue);

  while (element != NULL) {

    CrossMemoryServerLogServiceParm *msg = &element->logServiceParm;

    if (memcmp(msg->eyecatcher, CMS_LOG_SERVICE_PARM_EYECATCHER, sizeof(msg->eyecatcher))) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INVALID_EYECATCHER_MSG, "log parm", msg);
      zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, (char *)msg, sizeof(CrossMemoryServerLogServiceParm));
    }

    if (logShouldTraceInternal(NULL, LOG_COMP_ID_CMSPC, ZOWE_LOG_INFO)) {
      printf("%.*s", msg->messageLength > sizeof(msg->message) ? sizeof(msg->message) : msg->messageLength, msg->message);
    }

    freeMsgQueueElement(element);
    element = (CrossMemoryServerMsgQueueElement *)qDequeue(server->messageQueue);

    if (server->flags & CROSS_MEMORY_SERVER_FLAG_TERM_STARTED) {
      break;
    }

  }

}

static CMSModifyCommandStatus handleCommandVerbFlush(
    CrossMemoryServer *server,
    char **args,
    unsigned int argCount,
    CMSWTORouteInfo *routeInfo
) {

  CART cart = routeInfo->cart;
  int consoleID = routeInfo->consoleID;

  unsigned int expectedArgCount = 0;
  if (argCount != expectedArgCount) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_INVALID_CMD_ARGS_MSG, "FLUSH", expectedArgCount, argCount);
    wtoPrintf2(consoleID, cart, CMS_LOG_INVALID_CMD_ARGS_MSG, "FLUSH", expectedArgCount, argCount);
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  if ((server->flags & CROSS_MEMORY_SERVER_FLAG_READY) == 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_NOT_READY_FOR_CMD_MSG, "FLUSH");
    wtoPrintf2(consoleID, cart, CMS_LOG_NOT_READY_FOR_CMD_MSG, "FLUSH");
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  flushDebugMessages(server);

  return CMS_MODIFY_COMMAND_STATUS_CONSUMED;
}

static void printDisplayConfigCommandResponse(CrossMemoryServer *server, CMSWTORouteInfo *routeInfo) {

  CART cart = routeInfo->cart;
  int consoleID = routeInfo->consoleID;
  CrossMemoryServerGlobalArea *globalArea = server->globalArea;

#ifdef CROSS_MEMORY_SERVER_DEBUG
  char* formatStringPrefix = CMS_LOG_DISP_CMD_RESULT_MSG"Server name - \'%16.16s\' (debug mode)\n";
#else
  char* formatStringPrefix = CMS_LOG_DISP_CMD_RESULT_MSG"Server name - \'%16.16s\'\n";
#endif

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, strcat(formatStringPrefix,
      "         Global area address = 0x%p\n"
      "           Version          = %d\n"
      "           Key              = %d\n"
      "           Subpool          = %d\n"
      "           Size             = %d\n"
      "           Flags            = 0x%08X\n"
      "           Server address   = 0x%08X\n"
      "           Server ASID      = 0x%04X\n"
      "           Server flags     = 0x%08X\n"
      "           ECSA block count = %d\n"
      "           PC-ss PC number  = 0x%08X\n"
      "           PC-ss seq number = 0x%08X\n"
      "           PC-cp PC number  = 0x%08X\n"
      "           PC-cp seq number = 0x%08X\n"
      "           PC log level     = %d\n"),
      server->name.nameSpacePadded,
      globalArea,
      globalArea->version,
      globalArea->key,
      globalArea->subpool,
      globalArea->size,
      globalArea->flags,
      globalArea->localServerAddress,
      globalArea->serverASID,
      globalArea->serverFlags,
      globalArea->ecsaBlockCount,
      globalArea->pcInfo.pcssPCNumber,
      globalArea->pcInfo.pcssSequenceNumber,
      globalArea->pcInfo.pccpPCNumber,
      globalArea->pcInfo.pccpSequenceNumber,
      globalArea->pcLogLevel
  );

  MultilineWTOHandle *handle = wtoGetMultilineHandle(32);
#ifdef CROSS_MEMORY_SERVER_DEBUG
  wtoAddLine(handle, CMS_LOG_DISP_CMD_RESULT_MSG"Server name - \'%16.16s\' (debug mode)\n", server->name.nameSpacePadded);
#else
  wtoAddLine(handle, CMS_LOG_DISP_CMD_RESULT_MSG"Server name - \'%16.16s\'\n", server->name.nameSpacePadded);
#endif
  wtoAddLine(handle, "         Global area address = 0x%p",   globalArea);
  wtoAddLine(handle, "           Version          = %d",      globalArea->version);
  wtoAddLine(handle, "           Key              = %d",      globalArea->key);
  wtoAddLine(handle, "           Subpool          = %d",      globalArea->subpool);
  wtoAddLine(handle, "           Size             = %d",      globalArea->size);
  wtoAddLine(handle, "           Flags            = 0x%08X",  globalArea->flags);
  wtoAddLine(handle, "           Server address   = 0x%08X",  globalArea->localServerAddress);
  wtoAddLine(handle, "           Server ASID      = 0x%04X",  globalArea->serverASID);
  wtoAddLine(handle, "           Server flags     = 0x%08X",  globalArea->serverFlags);
  wtoAddLine(handle, "           ECSA block count = %d",      globalArea->ecsaBlockCount);
  wtoAddLine(handle, "           PC-ss PC number  = 0x%08X",  globalArea->pcInfo.pcssPCNumber);
  wtoAddLine(handle, "           PC-ss seq number = 0x%08X",  globalArea->pcInfo.pcssSequenceNumber);
  wtoAddLine(handle, "           PC-cp PC number  = 0x%08X",  globalArea->pcInfo.pccpPCNumber);
  wtoAddLine(handle, "           PC-cp seq number = 0x%08X",  globalArea->pcInfo.pccpSequenceNumber);
  wtoAddLine(handle, "           PC log level     = %d",      globalArea->pcLogLevel);
  wtoPrintMultilineMessage(handle, consoleID, cart);
  wtoRemoveMultilineHandle(handle);
  handle = NULL;

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_DISP_CMD_RESULT_MSG"Global area dump at 0x%p\n", server->globalArea);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, (char *)server->globalArea, sizeof(CrossMemoryServerGlobalArea));
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_DISP_CMD_RESULT_MSG"Server control block dump at 0x%p\n", server);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, (char *)server, sizeof(CrossMemoryServer));

}

#define CMS_COMMAND_VERB_DISAPLY_DEFAULT_OPTION "CONFIG"

static CMSModifyCommandStatus handleCommandVerbDisplay(
    CrossMemoryServer *server,
    char **args,
    unsigned int argCount,
    CMSWTORouteInfo *routeInfo
) {

  CART cart = routeInfo->cart;
  int consoleID = routeInfo->consoleID;

  char *option = NULL;
  if (argCount == 0) {
    argCount = 1;
    option = CMS_COMMAND_VERB_DISAPLY_DEFAULT_OPTION;
  } else {
    option = args[0];
  }

  unsigned int expectedArgCount = 1;
  if (argCount != expectedArgCount) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_INVALID_CMD_ARGS_MSG, "DISPLAY", expectedArgCount, argCount);
    wtoPrintf2(consoleID, cart, CMS_LOG_INVALID_CMD_ARGS_MSG, "DISPLAY", expectedArgCount, argCount);
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  if ((server->flags & CROSS_MEMORY_SERVER_FLAG_READY) == 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_NOT_READY_FOR_CMD_MSG, "DISPLAY");
    wtoPrintf2(consoleID, cart, CMS_LOG_NOT_READY_FOR_CMD_MSG, "DISPLAY");
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  if (strcmp(option, "CONFIG") == 0) {
    printDisplayConfigCommandResponse(server, routeInfo);
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_BAD_DISPLAY_OPTION_MSG, option);
    wtoPrintf2(consoleID, cart, CMS_LOG_BAD_DISPLAY_OPTION_MSG, option);
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  return CMS_MODIFY_COMMAND_STATUS_CONSUMED;
}

static CMSModifyCommandStatus handleCommandVerbLog(
    CrossMemoryServer *server,
    char **args,
    unsigned int argCount,
    CMSWTORouteInfo *routeInfo
) {

  CART cart = routeInfo->cart;
  int consoleID = routeInfo->consoleID;

  unsigned int expectedArgCount = 2;
  if (argCount != expectedArgCount) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
            CMS_LOG_INVALID_CMD_ARGS_MSG, "LOG", expectedArgCount, argCount);
    wtoPrintf2(consoleID, cart, CMS_LOG_INVALID_CMD_ARGS_MSG, "LOG",
               expectedArgCount, argCount);
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  if ((server->flags & CROSS_MEMORY_SERVER_FLAG_READY) == 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
            CMS_LOG_NOT_READY_FOR_CMD_MSG, "LOG");
    wtoPrintf2(consoleID, cart, CMS_LOG_NOT_READY_FOR_CMD_MSG, "LOG");
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  char *component = args[0];
  uint64 logCompID = 0;
  if (strcmp(component, "STC") == 0) {
    logCompID = LOG_COMP_STCBASE;
  } else if (strcmp(component, "CMS") == 0) {
    logCompID = LOG_COMP_ID_CMS;
  } else if (strcmp(component, "CMSPC") == 0) {
    logCompID = LOG_COMP_ID_CMSPC;
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_BAD_LOG_COMP_MSG,
            component);
    wtoPrintf2(consoleID, cart, CMS_LOG_BAD_LOG_COMP_MSG, component);
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  char *level = args[1];
  int logLevel = ZOWE_LOG_NA;
  if (strcmp(level, "SEVERE") == 0) {
    logLevel = ZOWE_LOG_SEVERE;
  } else if (strcmp(level, "WARNING") == 0) {
    logLevel = ZOWE_LOG_WARNING;
  } else if (strcmp(level, "INFO") == 0) {
    logLevel = ZOWE_LOG_INFO;
  } else if (strcmp(level, "DEBUG") == 0) {
    logLevel = ZOWE_LOG_DEBUG;
  } else if (strcmp(level, "DEBUG2") == 0) {
    logLevel = ZOWE_LOG_DEBUG2;
  } else if (strcmp(level, "DEBUG3") == 0) {
    logLevel = ZOWE_LOG_DEBUG3;
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_BAD_LOG_LEVEL_MSG,
            level);
    wtoPrintf2(consoleID, cart, CMS_LOG_BAD_LOG_LEVEL_MSG, level);
    return CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  logSetLevel(NULL, logCompID, logLevel);
  if (logCompID == LOG_COMP_ID_CMSPC) {
    server->globalArea->pcLogLevel = logLevel;
  }

  return CMS_MODIFY_COMMAND_STATUS_CONSUMED;
}

static int workElementHandler(STCBase *base, STCModule *module, WorkElementPrefix *prefix) {
  return 0;
}

int backgroundHandler(STCBase *base, STCModule *module, int selectStatus) {

  CrossMemoryServer *server = module->data;
  flushDebugMessages(server);

  return 0;
}

#define CMS_MAX_COMMNAD_LENGTH      64
#define CMS_MAX_COMMAND_TOKEN_COUNT (CMS_MAX_COMMNAD_LENGTH / 2)

#define CMS_COMMAND_VERB_LOG            "LOG"
#define CMS_COMMAND_VERB_FLUSH          "FLUSH"
#define CMS_COMMAND_VERB_DISPLAY        "DISPLAY"
#define CMS_COMMAND_VERB_DISPLAY_SHORT  "DIS"
#define CMS_COMMAND_VERB_DISPLAY_ABBRV  "D"
#define CMS_COMMAND_VERB_COLD           "COLD"

static char **tokenizeModifyCommand(ShortLivedHeap *slh, const char *command, unsigned short commandLength, unsigned int *tokenCount) {

  char **tokens = (char **)SLHAlloc(slh, sizeof(char *) * CMS_MAX_COMMAND_TOKEN_COUNT);
  if (tokens == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_ALLOC_FAILURE_MSG,
            "token array", sizeof(char *) * CMS_MAX_COMMAND_TOKEN_COUNT);
    return NULL;
  }

  unsigned int currTokenCount = 0;
  for (int charIdx = 0; charIdx < commandLength; charIdx++) {

    unsigned int tokenStartIdx = 0;
    for (; charIdx < commandLength; charIdx++) {
      if (command[charIdx] != ' ') {
        tokenStartIdx = charIdx;
        break;
      }
    }

    unsigned int tokenLength = 0;

    for (; charIdx < commandLength; charIdx++) {
      if (command[charIdx] == ' ') {
        break;
      }
      tokenLength++;
    }

    if (tokenLength > 0) {

      if (currTokenCount >= CMS_MAX_COMMAND_TOKEN_COUNT) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_TOO_MANY_CMD_TOKENS_MSG);
        return NULL;
      }

      tokens[currTokenCount] = SLHAlloc(slh, tokenLength + 1);
      if (tokens[currTokenCount] == NULL) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_ALLOC_FAILURE_MSG, "command token", tokenLength + 1);
        return NULL;
      }

      memcpy(tokens[currTokenCount], &command[tokenStartIdx], tokenLength);
      tokens[currTokenCount][tokenLength] = '\0';
      currTokenCount++;

    }

  }

  *tokenCount = currTokenCount;
  return tokens;
}

typedef struct CommandTaskContext_tag {
  char eyecatcher[8];
#define COMMAND_TASK_CONTEXT_EYECATCHER "CMSCTEYE"
  CrossMemoryServer *server;
  CMSWTORouteInfo routeInfo;
  char *command;
  unsigned short commandLength;
} CommandTaskContext;

static CommandTaskContext *makeCommandTaskContext(
    CrossMemoryServer *server,
    const char *command,
    unsigned short commandLength,
    const CMSWTORouteInfo *routeInfo
) {

  CommandTaskContext *context =
      (CommandTaskContext *)safeMalloc(sizeof(CommandTaskContext),
                                       "CommandTaskContext");
  if (context == NULL) {
    return NULL;
  }
  memset(context, 0, sizeof(CommandTaskContext));
  memcpy(context->eyecatcher, COMMAND_TASK_CONTEXT_EYECATCHER,
         sizeof(context->eyecatcher));
  context->server = server;
  context->routeInfo = *routeInfo;

  context->command = safeMalloc(commandLength, "async modify command");
  if (context->command != NULL) {
    memcpy(context->command, command, commandLength);
    context->commandLength = commandLength;
  } else {
    safeFree((char *)context, sizeof(CommandTaskContext));
    context = NULL;
  }

  return context;
}

static void destroyCommandTaskContext(CommandTaskContext *context) {
  safeFree(context->command, context->commandLength);
  context->command = NULL;
  safeFree((char *)context, sizeof(CommandTaskContext));
  context = NULL;
}

static void reportCommandRetrieval(char *commandVerb,
                                   char *target,
                                   char **args,
                                   unsigned int argCount,
                                   CMSWTORouteInfo *routeInfo) {

  unsigned int consoleID = routeInfo->consoleID;
  CART cart = routeInfo->cart;

  char *message;
  if (target != NULL) {
    message = CMS_LOG_CMD_RECEIVED_MSG" (target = %s)";
  } else {
    message = CMS_LOG_CMD_RECEIVED_MSG"%s";
    target = "";
  }

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, message, commandVerb, target);
  wtoPrintf2(consoleID, cart, message, commandVerb, target);

}

static void reportCommandStatus(char *commandVerb,
                                char *target,
                                char **args,
                                unsigned int argCount,
                                CMSWTORouteInfo *routeInfo,
                                CMSModifyCommandStatus status) {

  unsigned int consoleID = routeInfo->consoleID;
  CART cart = routeInfo->cart;

  char *badCmdMsg;
  char *rejectMsg;
  if (target != NULL) {
    badCmdMsg = CMS_LOG_BAD_CMD_MSG" (target = %s)";
    rejectMsg = CMS_LOG_CMD_REJECTED_MSG" (target = %s)";
  } else {
    badCmdMsg = CMS_LOG_BAD_CMD_MSG"%s";
    rejectMsg = CMS_LOG_CMD_REJECTED_MSG"%s";
    target = "";
  }

  if (status == CMS_MODIFY_COMMAND_STATUS_UNKNOWN) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, badCmdMsg, commandVerb,
            target);
    wtoPrintf2(consoleID, cart, badCmdMsg, commandVerb, target);
  } else if (status != CMS_MODIFY_COMMAND_STATUS_CONSUMED &&
             status != CMS_MODIFY_COMMAND_STATUS_PROCESSED) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, rejectMsg, commandVerb, target);
    wtoPrintf2(consoleID, cart, rejectMsg, commandVerb, target);
  }

}

typedef struct ABENDInfo_tag {
  char eyecatcher[8];
#define ABEND_INFO_EYECATCHER "CMSABEDI"
  int completionCode;
  int reasonCode;
} ABENDInfo;

static void extractABENDInfo(RecoveryContext * __ptr32 context,
                             SDWA * __ptr32 sdwa,
                             void * __ptr32 userData) {
  ABENDInfo *info = (ABENDInfo *)userData;
  recoveryGetABENDCode(sdwa, &info->completionCode, &info->reasonCode);
}

static void passCommandToUserCallback(
    CrossMemoryServer *server,
    char *commandVerb,
    char *target,
    char **args, unsigned int argCount,
    CMSWTORouteInfo *routeInfo
) {

  const CMSModifyCommand command = {
       .routeInfo = *routeInfo,
       .commandVerb = commandVerb,
       .target = target,
       .args = (const char* const*)args,
       .argCount = argCount,
   };


  reportCommandRetrieval(commandVerb, target, args, argCount, routeInfo);

  if (server->commandTaskCount >= CROSS_MEMORY_SERVER_MAX_CMD_TASK_NUM) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_CMD_REJECTED_BUSY_MSG,
            commandVerb, target);
    wtoPrintf2(routeInfo->consoleID, routeInfo->cart,
               CMS_LOG_CMD_REJECTED_BUSY_MSG, commandVerb, target);
    return;
  }

  CMSModifyCommandStatus status = CMS_MODIFY_COMMAND_STATUS_UNKNOWN;
  CMSModifyCommandCallback *userCallback = server->commandCallback;
  if (userCallback != NULL) {

    ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};
    int recoveryRC = recoveryPush(
        "plugin command handler",
        RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY | RCVR_FLAG_PRODUCE_DUMP,
        "plugin command handler",
        extractABENDInfo, &abendInfo,
        NULL, NULL
    );

    if (recoveryRC == RC_RCV_OK) {

      userCallback(server->globalArea, &command, &status, server->callbackData);

    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_STEP_ABEND_MSG,
              abendInfo.completionCode, abendInfo.reasonCode,
              "user command handler", recoveryRC);
      status = CMS_MODIFY_COMMAND_STATUS_REJECTED;
    }

    if (recoveryRC == RC_RCV_OK) {
      recoveryPop();
    }

  }

  reportCommandStatus(commandVerb, target, args, argCount, routeInfo, status);

}

static void passCommandToCMSCommandHandlers(
    CrossMemoryServer *server,
    char *commandVerb,
    char *target,
    char **args, unsigned int argCount,
    CMSWTORouteInfo *routeInfo
) {

  reportCommandRetrieval(commandVerb, target, args, argCount, routeInfo);

  CMSModifyCommandStatus status = CMS_MODIFY_COMMAND_STATUS_UNKNOWN;

  if (strcmp(commandVerb, CMS_COMMAND_VERB_LOG) == 0) {
    status = handleCommandVerbLog(server, args, argCount, routeInfo);
  } else if (strcmp(commandVerb, CMS_COMMAND_VERB_FLUSH) == 0) {
    status = handleCommandVerbFlush(server, args, argCount, routeInfo);
  } else if (strcmp(commandVerb, CMS_COMMAND_VERB_DISPLAY_ABBRV) == 0 ||
             strcmp(commandVerb, CMS_COMMAND_VERB_DISPLAY_SHORT) == 0 ||
             strcmp(commandVerb, CMS_COMMAND_VERB_DISPLAY) == 0) {
    status = handleCommandVerbDisplay(server, args, argCount, routeInfo);
  } else {
    status = CMS_MODIFY_COMMAND_STATUS_UNKNOWN;
  }

  reportCommandStatus(commandVerb, target, args, argCount, routeInfo, status);

}

static void splitVerbAndTarget(char **verb, char **target) {

  char *v = *verb;

  size_t verbLength = strlen(v);

  char *openParen = strchr(v, '(');
  char *closeParen = strchr(v, ')');

  if (openParen == NULL || closeParen == NULL) {
    return;
  }

  if (closeParen != (v + verbLength - 1)) {
    return;
  }

  *openParen = '\0';
  *closeParen = '\0';

  *verb = v;
  *target = openParen + 1;

}

static void handleAsyncModifyCommand(CrossMemoryServer *server,
                                     const char *command,
                                     unsigned short commandLength,
                                     CMSWTORouteInfo routeInfo) {

  int consoleID = routeInfo.consoleID;
  CART cart = routeInfo.cart;

  int slhBlockSize = 4096;
  int slhMaxBlockNumber = 4;
  ShortLivedHeap *slh = makeShortLivedHeap(slhBlockSize, slhMaxBlockNumber);
  if (slh == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_ALLOC_FAILURE_MSG,
            "SLH", slhBlockSize);
    wtoPrintf2(consoleID, cart, CMS_LOG_ALLOC_FAILURE_MSG,
               "SLH", slhBlockSize);
    return;
  }

  unsigned int commandTokenCount = 0;
  char **commandTokens = tokenizeModifyCommand(slh, command, commandLength,
                                               &commandTokenCount);

   if (commandTokens != NULL) {

     if (commandTokenCount > 0) {

       char *commandVerb = commandTokens[0];
       char **args = commandTokens + 1;
       unsigned int argCount = commandTokenCount - 1;
       char *target = NULL;
       splitVerbAndTarget(&commandVerb, &target);

       if (target == NULL) {
         passCommandToCMSCommandHandlers(server, commandVerb,
                                         target,
                                         args, argCount,
                                         &routeInfo);
       } else {
         passCommandToUserCallback(server, commandVerb,
                                   target,
                                   args, argCount,
                                   &routeInfo);
       }

     } else {
       zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_EMPTY_CMD_MSG);
       wtoPrintf2(consoleID, cart, CMS_LOG_EMPTY_CMD_MSG);
     }

   } else {
     zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
             CMS_LOG_CMD_TKNZ_FAILURE_MSG);
     zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
              (void *)command, commandLength);
     wtoPrintf2(consoleID, cart, CMS_LOG_CMD_TKNZ_FAILURE_MSG);
   }
   SLHFree(slh);
}

static int commandTaskHandler(RLETask *rleTask) {

  CommandTaskContext *context = rleTask->userPointer;
  CrossMemoryServer *server = context->server;
  atomicIncrement(&server->commandTaskCount, 1);

  char *command = context->command;
  unsigned short commandLength = context->commandLength;
  CMSWTORouteInfo routeInfo = context->routeInfo;

  CMSModifyCommandStatus status = CMS_MODIFY_COMMAND_STATUS_UNKNOWN;
  CMSModifyCommandCallback *userCallback = server->commandCallback;
  if (userCallback != NULL) {

    ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};
    int recoveryRC = recoveryPush(
        "async command handler",
        RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY | RCVR_FLAG_PRODUCE_DUMP,
        "async command handler",
        extractABENDInfo, &abendInfo,
        NULL, NULL
    );

    if (recoveryRC == RC_RCV_OK) {

      handleAsyncModifyCommand(server, command, commandLength, routeInfo);

    } else {

      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_STEP_ABEND_MSG,
              abendInfo.completionCode, abendInfo.reasonCode,
              "async command handler", recoveryRC);
      status = CMS_MODIFY_COMMAND_STATUS_REJECTED;

    }

    if (recoveryRC == RC_RCV_OK) {
      recoveryPop();
    }

  }

  destroyCommandTaskContext(context);
  context = NULL;

  atomicIncrement(&server->commandTaskCount, -1);
  return 0;
}

static void handleCommandAsynchronously(CrossMemoryServer *server,
                                        const char *command,
                                        unsigned short commandLength,
                                        const CMSWTORouteInfo *routeInfo) {

  if (server->commandTaskCount >= CROSS_MEMORY_SERVER_MAX_CMD_TASK_NUM) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
            CMS_LOG_CMD_REJECTED_BUSY_MSG);
    wtoPrintf2(routeInfo->consoleID, routeInfo->cart,
               CMS_LOG_CMD_REJECTED_BUSY_MSG);
    return;
  }

  CommandTaskContext *taskContext = makeCommandTaskContext(server,
                                                           command,
                                                           commandLength,
                                                           routeInfo);
  if (taskContext == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RES_NOT_CREATED_MSG,
            "command task context", 0);
    return;
  }

  RLETask *task = makeRLETask(
      server->base->rleAnchor,
      RLE_TASK_TCB_CAPABLE | RLE_TASK_RECOVERABLE | RLE_TASK_DISPOSABLE,
      commandTaskHandler
  );

  if (task == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RES_NOT_CREATED_MSG,
            "RLE task", 0);
    destroyCommandTaskContext(taskContext);
    taskContext = NULL;
    return;
  }

  task->userPointer = taskContext;

  startRLETask(task, NULL);

  task = NULL;
  taskContext = NULL;

}

static int handleModifyCommand(STCBase *base, CIB *cib, STCConsoleCommandType commandType, const char *command, unsigned short commandLength, void *userData) {

  CrossMemoryServer *server = userData;
  CrossMemoryServerGlobalArea *globalArea = server->globalArea;

  CIBX *cibx = (CIBX *)((char *)cib + cib->cibxoff);
  unsigned int consoleID = cibx->cibxcnid;
  CART cart = *(CART *)&cibx->cibxcart;

  CMSWTORouteInfo routeInfo = {cart, consoleID};

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG,
      CMS_LOG_DEBUG_MSG_ID" command handler called, type=%d, command=0x%p, commandLength=%hu, userData=0x%p\n",
      commandType, command, commandLength, userData);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, (void *)command, commandLength);

  if (commandLength > CMS_MAX_COMMNAD_LENGTH) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_CMD_TOO_LONG_MSG, commandLength);
    wtoPrintf2(consoleID, cart, CMS_LOG_CMD_TOO_LONG_MSG, commandLength);
    return 8;
  }

  int slhBlockSize = 4096;
  int slhMaxBlockNumber = 4;
  ShortLivedHeap *slh = makeShortLivedHeap(slhBlockSize, slhMaxBlockNumber);
  if (slh == NULL) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_ALLOC_FAILURE_MSG, "SLH", slhBlockSize);
    wtoPrintf2(consoleID, cart, CMS_LOG_ALLOC_FAILURE_MSG, "SLH", slhBlockSize);
    return 8;
  }

  if (commandType == STC_COMMAND_MODIFY) {

    handleCommandAsynchronously(server, command, commandLength, &routeInfo);

  } else if (commandType == STC_COMMAND_START) {

    server->startCommandInfo = routeInfo;

    unsigned int commandTokenCount = 0;
    char **commandTokens = tokenizeModifyCommand(slh, command, commandLength, &commandTokenCount);
    if (commandTokens != NULL) {

      if (commandTokenCount > 0) {

        char *commandVerb = commandTokens[0];
        char **args = commandTokens + 1;
        unsigned int argCount = commandTokenCount - 1;

        if (strcmp(commandVerb, CMS_COMMAND_VERB_COLD) == 0) {
          server->flags |= CROSS_MEMORY_SERVER_FLAG_COLD_START;
          if (globalArea != NULL) {
            globalArea->serverFlags |= CROSS_MEMORY_SERVER_FLAG_COLD_START;
          }
        } else {
          zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_BAD_CMD_MSG, commandVerb);
          wtoPrintf2(consoleID, cart, CMS_LOG_BAD_CMD_MSG, commandVerb);
        }

      }

    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_CMD_TKNZ_FAILURE_MSG);
      zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, (void *)command, commandLength);
      wtoPrintf2(consoleID, cart, CMS_LOG_CMD_TKNZ_FAILURE_MSG);
    }

  } else if (commandType == STC_COMMAND_STOP) {

    server->termCommandInfo = routeInfo;

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_TERM_CMD_MSG);
    wtoPrintf2(consoleID, cart, CMS_LOG_TERM_CMD_MSG);
    server->flags |= CROSS_MEMORY_SERVER_FLAG_TERM_STARTED;
    if (globalArea != NULL) {
      globalArea->serverFlags |= CROSS_MEMORY_SERVER_FLAG_TERM_STARTED;
    }

  }

  SLHFree(slh);
  slh = NULL;

  return 0;
}

static void sleep(int seconds){
  int waitValue = seconds * 100;
  __asm(" STIMER WAIT,BINTVL=%0\n" : : "m"(waitValue));
}

static int isEnvironmentReady() {

  if (getLoggingContext() == NULL) {
    return RC_CMS_LOGGING_CNTX_NOT_FOUND;
  }

  if (recoveryIsRouterEstablished() == FALSE) {
    return RC_CMS_RECOVERY_CNTX_NOT_FOUND;
  }

  return RC_CMS_OK;
}

static void printServerInitStartedMessage(CrossMemoryServer *srv) {

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STARTED_MSG);

  wtoPrintf2(srv->startCommandInfo.consoleID, srv->startCommandInfo.cart, CMS_LOG_INIT_STARTED_MSG);

}

static void printServerInitFailedMessage(CrossMemoryServer *srv, int rc) {

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_FAILED_MSG, rc);

  wtoPrintf2(srv->startCommandInfo.consoleID, srv->startCommandInfo.cart, CMS_LOG_INIT_FAILED_MSG, rc);

}

static void printServerReadyMessage(CrossMemoryServer *srv) {

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_SERVER_READY_MSG);

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
      CMS_LOG_MODIFY_CMD_INFO_MSG"Modify commands:\n"
      "           DISPLAY [OPTION_NAME]- Print service information\n"
      "             OPTION_NAME:\n"
      "               CONFIG - Print server configuration information (default)\n"
      "           FLUSH - Print all pending log messages\n"
      "           LOG <COMP_ID> <LOG_LEVEL> - Set log level\n"
      "             COMP_ID:\n"
      "               CMS   - Cross memory server\n"
      "               CMSPC - PC routines\n"
      "               STC   - STC base\n"
      "             LOG_LEVEL:\n"
      "               SEVERE\n"
      "               WARNING\n"
      "               INFO\n"
      "               DEBUG\n"
      "               DEBUG2\n"
      "               DEBUG3\n"
  );

  int consoleID = srv->startCommandInfo.consoleID;
  CART cart = srv->startCommandInfo.cart;

  wtoPrintf2(consoleID, cart, CMS_LOG_SERVER_READY_MSG);

  MultilineWTOHandle *handle = wtoGetMultilineHandle(32);
  wtoAddLine(handle, CMS_LOG_MODIFY_CMD_INFO_MSG"Modify commands:");
  wtoAddLine(handle, "           DISPLAY [OPTION_NAME] - Print service information");
  wtoAddLine(handle, "             OPTION_NAME:");
  wtoAddLine(handle, "               CONFIG - Print server configuration information (default)");
  wtoAddLine(handle, "           FLUSH   - Print all pending log messages");
  wtoAddLine(handle, "           LOG <COMP_ID> <LOG_LEVEL> - Set log level");
  wtoAddLine(handle, "             COMP_ID:");
  wtoAddLine(handle, "               CMS   - Cross memory server");
  wtoAddLine(handle, "               CMSPC - PC routines");
  wtoAddLine(handle, "               STC   - STC base");
  wtoAddLine(handle, "             LOG_LEVEL:");
  wtoAddLine(handle, "               SEVERE");
  wtoAddLine(handle, "               WARNING");
  wtoAddLine(handle, "               INFO");
  wtoAddLine(handle, "               DEBUG");
  wtoAddLine(handle, "               DEBUG2");
  wtoAddLine(handle, "               DEBUG3");
  wtoPrintMultilineMessage(handle, consoleID, cart);
  wtoRemoveMultilineHandle(handle);
  handle = NULL;

}

static void printServerStopped(CrossMemoryServer *srv, int rc) {

  CART cart = 0;
  int consoleID = 0;
  if (srv->startCommandInfo.cart != 0) {
    cart = srv->startCommandInfo.cart;
    consoleID = srv->startCommandInfo.consoleID;
  } else {
    cart = srv->termCommandInfo.cart;
    consoleID = srv->termCommandInfo.consoleID;
  }

  if (rc == RC_CMS_OK) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_SERVER_STOPPED_MSG);
    wtoPrintf2(consoleID, cart, CMS_LOG_SERVER_STOPPED_MSG);
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_SERVER_STOP_FAILURE_MSG, rc);
    wtoPrintf2(consoleID, cart, CMS_LOG_SERVER_STOP_FAILURE_MSG, rc);
  }

}

static Addr31 getResourceManagerRoutine(unsigned int *size) {

  Addr31 routineStart = NULL;
  Addr31 routineEnd = NULL;

  __asm(
      ASM_PREFIX
      "         LARL  1,L$RMRBGN                                               \n"
      "         ST    1,%0                                                     \n"
      "         LARL  1,L$RMREND                                               \n"
      "         ST    1,%1                                                     \n"
      "         J     L$RMREND                                                 \n"
      "L$RMRBGN DS    0H                                                       \n"

      /* establish addressability */
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  10,L$RMRBGN                                              \n"
      "         USING L$RMRBGN,10                                              \n"
      /* save input */
      "         LTR   1,1                 are parms from system zero?          \n"
      "         JZ    L$RMRRET            yes, leave                           \n"
      /* handle input parms */
      "         L     3,4(,1)             user provided field address          \n"
      "         LTR   3,3                 is it zero?                          \n"
      "         JZ    L$RMRRET            yes, leave                           \n"
      "         L     4,0(,3)             user parms (global area)             \n"
      "         LTR   4,4                 is it zero?                          \n"
      "         JZ    L$RMRRET            yes, leave                           \n"
      "         USING GLA,4                                                    \n"
      "         CLC   GLAEYE,=C'"CMS_GLOBAL_AREA_EYECATCHER"'                  \n"
      "         JNE   L$RMRRET            bad eyecatcher, leave                \n"
      /* zero out PC info in global area */
      "         LA    2,GLAPCINF          destination address                  \n"
      "         LA    1,"INT_TO_STR(CROSS_MEMORY_SERVER_KEY)"  destination key \n"
      "         SLL   1,4                 shift key to bits 24-27              \n"
      "         LA    0,GLAPCINL          size of PC info                      \n"
      "         BCTR  0,0                 bytes to copy - 1                    \n"
      "         MVCDK 0(2),=XL(GLAPCINL)'00'  zero out                         \n" /* make sure to keep it below 256 bytes */
           /* return to caller */
      "L$RMRRET DS    0H                                                       \n"
      "         LA    0,0                 reason code for system               \n"
      "         EREG  1,1                 R1 must be restored                  \n"
      "         PR    ,                   return                               \n"
      /* non executable code */
      "         LTORG                                                          \n"
      "         POP   USING                                                    \n"

      "L$RMREND DS    0H                                                       \n"
      : "=m"(routineStart), "=m"(routineEnd)
      :
      : "r1"
  );

  *size = (unsigned int)routineEnd - (unsigned int)routineStart;
  return routineStart;
}

#define RESMGR_RNAME_PREFIX   "ISRMGR.V"
#define RESMGR_NT_NAME_PREFIX CMS_PROD_ID".IRM"

static int installResourceManager(CrossMemoryServerGlobalArea *globalArea, ResourceManagerHandle *resmgrHandle) {

  QName resmgrQName = PRODUCT_QNAME;
  RName resmgrRName;
  {
    int bytesToBeWritten = snprintf(resmgrRName.value, sizeof(resmgrRName.value), "%s%08X", RESMGR_RNAME_PREFIX, globalArea->version);
    if (bytesToBeWritten < 0) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RNAME_FAILURE_MSG, "RESMGR", "snprintf failed");
      return RC_CMS_SNPRINTF_FAILED;
    }
    if (bytesToBeWritten >= sizeof(resmgrRName.value)) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RNAME_FAILURE_MSG, "RESMGR", "too long");
      return RC_CMS_SNPRINTF_FAILED;
    }
    resmgrRName.length = bytesToBeWritten;
  }
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR QNAME:\n");
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, &resmgrQName, sizeof(QName));
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR RNAME:\n");
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, &resmgrRName, sizeof(RName));

  NameTokenUserName resmgrRoutineNTName;
  {
    char nameTokenString[sizeof(resmgrRoutineNTName.name) + 1];
    int bytesToBeWritten = snprintf(nameTokenString, sizeof(nameTokenString), "%s%08X", RESMGR_NT_NAME_PREFIX, globalArea->version);
    if (bytesToBeWritten < 0) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_NT_NAME_FAILURE_MSG, "RESMGR", "snprintf failed");
      return RC_CMS_SNPRINTF_FAILED;
    }
    if (bytesToBeWritten >= sizeof(nameTokenString)) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_NT_NAME_FAILURE_MSG, "RESMGR", "too long");
      return RC_CMS_SNPRINTF_FAILED;
    }
    memcpy(resmgrRoutineNTName.name, nameTokenString, sizeof(resmgrRoutineNTName.name));
  }
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR name/token name:\n");
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, &resmgrRoutineNTName, sizeof(resmgrRoutineNTName));

  ENQToken lockToken;
  int lockRC = 0, lockRSN = 0;
  lockRC = isgenqGetExclusiveLock(&resmgrQName, &resmgrRName, ISGENQ_SCOPE_SYSTEM, &lockToken, &lockRSN);
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR ISGENQ LOCK RC = %d, RSN = 0x%08X:\n", lockRC, lockRSN);
  if (lockRC > 4) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RESMGR_NOT_LOCKED_MSG, globalArea->version, lockRC, lockRSN);
    return RC_CMS_RESMGR_NOT_LOCKED;
  }

  int status = RC_CMS_OK;
  Addr31 resmgrRoutineInECSA = NULL;
  unsigned int resmgrRoutineSize = 0;

  union {
    struct {
      Addr31 routine;
      unsigned int routineSize;
    };
    NameTokenUserToken token;
  } resmgrRoutineNTToken = {0, 0};

  int retrieveRC = nameTokenRetrieve(NAME_TOKEN_LEVEL_SYSTEM, &resmgrRoutineNTName, &resmgrRoutineNTToken.token);

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR name/token RC = %d, token:\n", retrieveRC);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, &resmgrRoutineNTToken, sizeof(resmgrRoutineNTToken));

  if (retrieveRC == RC_NAMETOKEN_NOT_FOUND) {

    Addr31 resmgrRoutine = getResourceManagerRoutine(&resmgrRoutineSize);

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR routine @ 0x%p with size = %u\n",
        resmgrRoutine, resmgrRoutineSize);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, resmgrRoutine, resmgrRoutineSize);

    resmgrRoutineInECSA = allocateECSAStorage(resmgrRoutineSize);
    if (resmgrRoutineInECSA == NULL) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RESMGR_ECSA_FAILURE_MSG, resmgrRoutineSize);
      status = RC_CMS_ECSA_ALLOC_FAILED;
    }

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR ECSA allocated @ 0x%p, size = %u\n",
        resmgrRoutineInECSA, resmgrRoutineSize);

    if (status == RC_CMS_OK) {
      memcpy(resmgrRoutineInECSA, resmgrRoutine, resmgrRoutineSize);
      resmgrRoutineNTToken.routine = resmgrRoutineInECSA;
      resmgrRoutineNTToken.routineSize = resmgrRoutineSize;
      int createTokenRC = nameTokenCreatePersistent(NAME_TOKEN_LEVEL_SYSTEM, &resmgrRoutineNTName, &resmgrRoutineNTToken.token);
      if (createTokenRC != 0) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RESMGR_NT_NOT_CREATED_MSG, createTokenRC);
        freeECSAStorage(resmgrRoutineInECSA, resmgrRoutineSize);
        resmgrRoutineInECSA = NULL;
        status = RC_CMS_NAME_TOKEN_CREATE_FAILED;
      }
    }

  } else if (retrieveRC == RC_NAMETOKEN_OK) {
    resmgrRoutineInECSA = resmgrRoutineNTToken.routine;
    resmgrRoutineSize = resmgrRoutineNTToken.routineSize;
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR ECSA routine @ 0x%p\n", resmgrRoutineInECSA);
    zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, resmgrRoutineInECSA, resmgrRoutineSize);
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RESMGR_NT_NOT_RETR_MSG, retrieveRC);
    status = RC_CMS_RESMGR_NAMETOKEN_FAILED;
  }

  if (status == RC_CMS_OK) {
    ASCB *ascb = getASCB();
    unsigned short asid = ascb->ascbasid;
    int resourceManagerRC = 0;
    int resmgrAddRC = resmgrAddAddressSpaceResourceManager(asid, resmgrRoutineInECSA, globalArea, resmgrHandle, &resourceManagerRC);
    if (resmgrAddRC != 0) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RESMGR_NOT_ADDED_MSG, resmgrAddRC, resourceManagerRC);
      status = RC_CMS_RESMGR_NOT_ADDED;
    }
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR ADD ASID = %04X, RC = %d, manager RC = 0x%08X\n",
        asid, resmgrAddRC, resourceManagerRC);
  }

  int unlockRC = 0, unlockRSN = 0;
  unlockRC = isgenqReleaseLock(&lockToken, &unlockRSN);
  if (unlockRC > 4) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RESMGR_NOT_RELEASED_MSG, globalArea->version, unlockRC, unlockRSN);
  }
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR ISGENQ UNLOCK RC = %d, RSN = 0x%08X:\n", unlockRC, unlockRSN);

  return status;
}

static int removeResourceManager(ResourceManagerHandle *resmgrHandle) {

  unsigned short asid = resmgrHandle->asid;
  int resourceManagerRC = 0;
  int resmgrDeleteRC = resmgrDeleteAddressSpaceResourceManager(resmgrHandle, &resourceManagerRC);
  if (resmgrDeleteRC != RC_RESMGR_OK) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_RESMGR_NOT_REMOVED_MSG, asid, resmgrDeleteRC, resourceManagerRC);
    return RC_CMS_RESMGR_NOT_REMOVED;
  }
  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, CMS_LOG_DEBUG_MSG_ID" RESMGR DELETE ASID = %04X, RC = %d, manager RC = 0x%08X\n",
      asid, resmgrDeleteRC, resourceManagerRC);

  return RC_CMS_OK;
}

#pragma pack(packed)

typedef struct BLDLListEntry_tag {
  char name[8];
  int ttr : 24;
  int concatenationNumber : 8;
  int dirEntryLocation : 8;
#define BLDL_LIST_LOC_PRIVATE         0x00
#define BLDL_LIST_LOC_LINKLIB         0x01
#define BLDL_LIST_LOC_STEPLIB         0x02 /* Job, task, or step library */
/* 3-16 Job, task, or step library of parent task n, where n = Z-2 */
  int nameType : 8;
#define BLDL_LIST_NAME_TYPE_MEMBER    0x00
  char userData[62];
} BLDLListEntry;

typedef struct BLDLList_tag {
  unsigned short entryCount;
  unsigned short entryLength;
  BLDLListEntry entry;
} BLDLList;

#pragma pack(reset)

static int verifySTEPLIB(CrossMemoryServer *srv) {

  TCB *tcb = getTCB();
  /* This DCB will point to either JOBLIB or STEPLIB in our case. */
  char *dcb = (char *)tcb->tcbjlb;

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG,
          CMS_LOG_DEBUG_MSG_ID" TCB=0x%p, TCBJLB=0x%p\n", tcb, dcb);

  if (dcb == 0) {
    return RC_CMS_NO_STEPLIB;
  }

  BLDLList bldlList = {
      .entryCount = 1,
      .entryLength = sizeof(BLDLListEntry),
      .entry.name = CMS_MODULE,
      .entry.ttr = 0,
      .entry.concatenationNumber = 0,
      .entry.dirEntryLocation = 0,
      .entry.nameType = BLDL_LIST_NAME_TYPE_MEMBER,
      .entry.userData = {0},
  };

  int bldlRC = 0, bldlRSN = 0;
  __asm(
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         BLDL  (%2),(%3)                                                \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ST    15,%0                                                    \n"
      "         ST    0,%1                                                     \n"
      : "=m"(bldlRC), "=m"(bldlRSN)
      : "r"(dcb), "r"(&bldlList)
      : "r0", "r1", "r14", "r15"
  );

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG,
          CMS_LOG_DEBUG_MSG_ID" BLDL RC=%d, RSN=%d, bldlList=0x%p\n",
          bldlRC, bldlRSN, &bldlList);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG, &bldlList, sizeof(bldlList));

  if (bldlRC != 0) {
    return RC_CMS_MODULE_NOT_IN_STEPLIB;
  }

  return RC_CMS_OK;
}

#define MAIN_WAIT_MILLIS 10000
#define START_COMMAND_HANDLING_DELAY_IN_SEC 5
#define STCBASE_SHUTDOWN_DELAY_IN_SEC       5

int cmsStartMainLoop(CrossMemoryServer *srv) {

  int authStatus = testAuth();
  if (authStatus != 0) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
            CMS_LOG_NOT_APF_AUTHORIZED_MSG, authStatus);
    return RC_CMS_NOT_APF_AUTHORIZED;
  }

  int tcbKey = getTCBKey();
  if ((tcbKey != 2 && tcbKey != 4) || (tcbKey != CROSS_MEMORY_SERVER_KEY)) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_BAD_SERVER_KEY_MSG, tcbKey);
    return RC_CMS_BAD_SERVER_KEY;
  }

  int rcvrPushRC = recoveryPush(
      "cmsStartMainLoop",
      RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY | RCVR_FLAG_PRODUCE_DUMP,
      "cmsStartMainLoop",
      NULL, NULL,
      NULL, NULL
  );
  if (rcvrPushRC == RC_RCV_ABENDED) {
    printServerStopped(srv, RC_CMS_SERVER_ABENDED);
    return RC_CMS_SERVER_ABENDED;
  } else if (rcvrPushRC == RC_RCV_CONTEXT_NOT_FOUND) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INIT_STEP_FAILURE_MSG, "push recovery state", rcvrPushRC);
    return RC_CMS_RECOVERY_CNTX_NOT_FOUND;
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STEP_SUCCESS_MSG, "push recovery state");
  }

  int status = RC_CMS_OK;
  bool serverStarted = false;
  bool startCallbackSuccess = true;
  bool resourceManagerInstalled = false;

  if (status == RC_CMS_OK) {
    int verifyRC = verifySTEPLIB(srv);
    if (verifyRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
              CMS_LOG_INIT_STEP_FAILURE_MSG, "verify STEPLIB", verifyRC);
      status = verifyRC;
    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
              CMS_LOG_INIT_STEP_SUCCESS_MSG, "verify STEPLIB");
    }
  }

  if (status == RC_CMS_OK) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_STARTING_CONSOLE_TASK_MSG);
    stcLaunchConsoleTask2(srv->base, handleModifyCommand, srv);
    sleep(START_COMMAND_HANDLING_DELAY_IN_SEC);
  }

  if (status == RC_CMS_OK) {
    int loadRC = loadServer(srv);
    if (loadRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INIT_STEP_FAILURE_MSG, "load server", loadRC);
      status = loadRC;
    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STEP_SUCCESS_MSG, "load server");
    }
  }

  if (status == RC_CMS_OK) {
    printServerInitStartedMessage(srv);
  }

  if (status == RC_CMS_OK) {
    if (srv->flags & CROSS_MEMORY_SERVER_FLAG_COLD_START) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_COLD_START_MSG);
      int cleanRC = discardGlobalResources(srv);
      if (cleanRC != RC_CMS_OK) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, CMS_LOG_GLB_CLEANUP_WARN_MSG, cleanRC);
      }
    }
  }

  if (status == RC_CMS_OK) {
    int securityRC = initSecuritySubsystem(srv);
    if (securityRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INIT_STEP_FAILURE_MSG, "init security", securityRC);
      status = securityRC;
    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STEP_SUCCESS_MSG, "init security");
    }
  }

  if (status == RC_CMS_OK) {
    int allocRC = allocateGlobalResources(srv);
    if (allocRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INIT_STEP_FAILURE_MSG, "allocate global resources", allocRC);
      status = allocRC;
    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STEP_SUCCESS_MSG, "allocate global resources");
    }
  }

  if (status == RC_CMS_OK) {
    int pcRC = establishPCRoutines(srv);
    if (pcRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INIT_STEP_FAILURE_MSG, "established PC routines", pcRC);
      status = pcRC;
    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STEP_SUCCESS_MSG, "established PC routines");
    }
  }

  ResourceManagerHandle rmHandle;
  if (status == RC_CMS_OK) {
    int rmAddRC = installResourceManager(srv->globalArea, &rmHandle);
    if (rmAddRC != RC_RESMGR_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INIT_STEP_FAILURE_MSG, "add resource manager", rmAddRC);
      status = RC_CMS_RESMGR_NOT_ADDED;
    } else {
      resourceManagerInstalled = true;
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STEP_SUCCESS_MSG, "add resource manager");
    }
  }

  if (status == RC_CMS_OK) {
    if (srv->startCallback != NULL) {
      int callbackRC = srv->startCallback(srv->globalArea, srv->callbackData);
      if (callbackRC != 0) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
                CMS_LOG_INIT_STEP_FAILURE_MSG, "start callback", callbackRC);
        status = RC_CMS_ERROR;
        startCallbackSuccess = false;
      } else {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
                CMS_LOG_INIT_STEP_SUCCESS_MSG, "start callback");
      }
    }
  }

  if (status == RC_CMS_OK) {
    int startRC = startServer(srv);
    if (startRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_INIT_STEP_FAILURE_MSG, "start server", startRC);
      status = startRC;
    } else {
      serverStarted = true;
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_INIT_STEP_SUCCESS_MSG, "start server");
    }
  }

  if (status != RC_CMS_OK) {
    printServerInitFailedMessage(srv, status);
  }

  if (status == RC_CMS_OK) {

    stcRegisterModule(
        srv->base,
        STC_MODULE_GENERIC,
        srv,
        NULL,
        NULL,
        workElementHandler,
        backgroundHandler
    );

    printServerReadyMessage(srv);

    srv->startCommandInfo.cart = 0;
    srv->startCommandInfo.consoleID = 0;

    stcBaseMainLoop(srv->base, MAIN_WAIT_MILLIS);

    if ((srv->flags & CROSS_MEMORY_SERVER_FLAG_TERM_STARTED) == 0) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_MAIN_LOOP_FAILURE_MSG);
      status = RC_CMS_MAIN_LOOP_FAILED;
    } else  {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_MAIN_LOOP_TERM_MSG);
    }

  }

  stcBaseShutdown(srv->base);
  sleep(STCBASE_SHUTDOWN_DELAY_IN_SEC);

  if (serverStarted) {
    int stopRC = stopServer(srv);
    if (stopRC != RC_CMS_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_TERM_STEP_FAILURE_MSG, "stop server", stopRC);
      status = (status != RC_CMS_OK) ? status : stopRC;
    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_TERM_STEP_SUCCESS_MSG, "stop server");
    }
  }

  if (startCallbackSuccess) {
    if (srv->stopCallback != NULL) {
      int callbackRC = srv->stopCallback(srv->globalArea, srv->callbackData);
      if (callbackRC != 0) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
                CMS_LOG_TERM_STEP_FAILURE_MSG, "stop callback", callbackRC);
        status = RC_CMS_ERROR;
      } else {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
                CMS_LOG_TERM_STEP_SUCCESS_MSG, "stop callback");
      }
    }
  }

  if (resourceManagerInstalled) {
    int rmDeleteRC = removeResourceManager(&rmHandle);
    if (rmDeleteRC != RC_RESMGR_OK) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, CMS_LOG_TERM_STEP_FAILURE_MSG, "delete resource manager", rmDeleteRC);
      status = (status != RC_CMS_OK) ? status : rmDeleteRC;
    } else {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, CMS_LOG_TERM_STEP_SUCCESS_MSG, "delete resource manager");
    }
  }

  recoveryPop();

  printServerStopped(srv, status);

  return status;
}

#endif /* defined(METTLE) && !defined(CMS_CLIENT) */

int cmsGetGlobalArea(const CrossMemoryServerName *serverName, CrossMemoryServerGlobalArea **globalAreaAddress) {

#ifdef CROSS_MEMORY_SERVER_DEBUG

  NameTokenUserName ntName;
  memcpy(ntName.name, serverName->nameSpacePadded, sizeof(ntName.name));
  CrossMemoryServerGlobalArea *globalArea = NULL;
  CrossMemoryServerToken token = {"        ", NULL, 0};
  if (!nameTokenRetrieve(NAME_TOKEN_LEVEL_SYSTEM, &ntName, &token.ntToken)) {

    if (memcmp(token.eyecatcher, CMS_TOKEN_EYECATCHER, sizeof(token.eyecatcher))) {
      return RC_CMS_NAME_TOKEN_BAD_EYECATCHER;
    }

    globalArea = token.globalArea;
    if (globalArea == NULL) {
      return RC_CMS_GLOBAL_AREA_NULL;
    }

  } else {
    return RC_CMS_GLOBAL_AREA_NULL;
  }

  *globalAreaAddress = globalArea;
  return RC_CMS_OK;

#else

  ZVT *zvt = zvtGet();
  if (zvt == NULL) {
    return RC_CMS_ZVT_NULL;
  }

  CrossMemoryServerGlobalArea *globalArea = NULL;
  ZVTEntry *currZVTE = zvt->zvteSlots.zis;
  int entryIdx = 0;
  for (entryIdx = 0; entryIdx < CMS_MAX_ZVTE_CHAIN_LENGTH; entryIdx++) {

    if (currZVTE == NULL) {
      break;
    }

    CrossMemoryServerGlobalArea *globalAreaCandidate = (CrossMemoryServerGlobalArea *)currZVTE->productAnchor;
    if (globalAreaCandidate != NULL) {
      if (memcmp(&globalAreaCandidate->serverName, serverName, sizeof(CrossMemoryServerName)) == 0 &&
          globalAreaCandidate->version == CROSS_MEMORY_SERVER_VERSION) {
        /* TODO: make a new function that finds a running server by name
         * The version check should be done when we've found a running instance,
         * otherwise it is not clear whether global are is NULL because it's
         * not been allocated or the caller's and server's versions don't
         * match. */
        globalArea = globalAreaCandidate;
        break;
      }
    }

    currZVTE = (ZVTEntry *)currZVTE->next;
  }

  if (entryIdx == CMS_MAX_ZVTE_CHAIN_LENGTH) {
    return RC_CMS_ZVTE_CHAIN_LOOP;
  }

  if (globalArea == NULL) {
    return RC_CMS_GLOBAL_AREA_NULL;
  }

  *globalAreaAddress = globalArea;
  return RC_CMS_OK;

#endif
}

static int callServiceInternal(CrossMemoryServerGlobalArea *globalArea,
                               int serviceID, void *parmList,
                               int flags,
                               int *serviceRC) {

  if (!(globalArea->serverFlags & CROSS_MEMORY_SERVER_FLAG_READY)) {
    return RC_CMS_SERVER_NOT_READY;
  }

  CrossMemoryService *service = &globalArea->serviceTable[serviceID];

  int pcNumber = 0;
  int sequenceNumber = 0;

  if (service->flags & CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH) {
    pcNumber = globalArea->pcInfo.pcssPCNumber;
    sequenceNumber = globalArea->pcInfo.pcssSequenceNumber;
  } else {
    pcNumber = globalArea->pcInfo.pccpPCNumber;
    sequenceNumber = globalArea->pcInfo.pccpSequenceNumber;
  }

  if (pcNumber == 0) {
    return RC_CMS_ZERO_PC_NUMBER;
  }

  CrossMemoryServerParmList cmsParmList;
  memset(&cmsParmList, 0, sizeof(cmsParmList));
  memcpy(cmsParmList.eyecatcher, CMS_PARMLIST_EYECATCHER, sizeof(cmsParmList.eyecatcher));
  cmsParmList.version = CROSS_MEMORY_SERVER_VERSION;
  cmsParmList.serverName = globalArea->serverName;
  cmsParmList.serviceID = serviceID;
  cmsParmList.serviceRC = 0;
  cmsParmList.callerData = parmList;

  if (flags & CMS_CALL_FLAG_NO_SAF_CHECK) {
    cmsParmList.flags |= CMS_PARMLIST_FLAG_NO_SAF_CHECK;
  }

  int callRC = callPCRoutine(pcNumber, sequenceNumber, &cmsParmList);
  if (callRC != RC_CMS_OK) {
    return callRC;
  }

  if (serviceRC != NULL) {
    *serviceRC = cmsParmList.serviceRC;
  }

  return RC_CMS_OK;
}

int cmsCallService(const CrossMemoryServerName *serverName, int serviceID, void *parmList, int *serviceRC) {

  if (serverName == NULL) {
    return RC_CMS_SERVER_NAME_NULL;
  }

  if (serviceID <= 0 || CROSS_MEMORY_SERVER_MAX_SERVICE_ID < serviceID) {
    return RC_CMS_FUNCTION_ID_OUT_OF_RANGE;
  }

  CrossMemoryServerGlobalArea *globalArea = NULL;
  int getRC = cmsGetGlobalArea(serverName, &globalArea);
  if (getRC != RC_CMS_OK) {
    return getRC;
  }

  if (globalArea->version != CROSS_MEMORY_SERVER_VERSION) {
    return RC_CMS_WRONG_SERVER_VERSION;
  }

  return callServiceInternal(globalArea, serviceID, parmList,
                             CMS_CALL_FLAG_NONE, serviceRC);
}

int cmsCallService2(CrossMemoryServerGlobalArea *cmsGlobalArea,
                    int serviceID, void *parmList, int *serviceRC) {

  if (serviceID <= 0 || CROSS_MEMORY_SERVER_MAX_SERVICE_ID < serviceID) {
    return RC_CMS_FUNCTION_ID_OUT_OF_RANGE;
  }

  if (cmsGlobalArea->version != CROSS_MEMORY_SERVER_VERSION) {
    return RC_CMS_WRONG_SERVER_VERSION;
  }

  return callServiceInternal(cmsGlobalArea, serviceID, parmList,
                             CMS_CALL_FLAG_NONE, serviceRC);
}

int cmsCallService3(CrossMemoryServerGlobalArea *cmsGlobalArea,
                    int serviceID, void *parmList, int flags, int *serviceRC) {

  if (serviceID <= 0 || CROSS_MEMORY_SERVER_MAX_SERVICE_ID < serviceID) {
    return RC_CMS_FUNCTION_ID_OUT_OF_RANGE;
  }

  if (cmsGlobalArea->version != CROSS_MEMORY_SERVER_VERSION) {
    return RC_CMS_WRONG_SERVER_VERSION;
  }

  return callServiceInternal(cmsGlobalArea, serviceID, parmList, flags,
                             serviceRC);
}

int cmsPrintf(const CrossMemoryServerName *serverName, const char *formatString, ...) {

  CrossMemoryServerLogServiceParm msgParmList;
  memcpy(&msgParmList.eyecatcher, CMS_LOG_SERVICE_PARM_EYECATCHER, sizeof(msgParmList.eyecatcher));

  LogMessagePrefix *prefix = &msgParmList.prefix;
  initLogMessagePrefix(prefix);

  va_list argPointer;
  va_start(argPointer, formatString);
  int charactersToBeWritten = vsnprintf(msgParmList.text, sizeof(msgParmList.text), formatString, argPointer);
  va_end(argPointer);

  if (charactersToBeWritten < 0) {
    return RC_CMS_VSNPRINTF_FAILED;
  }
  if (charactersToBeWritten >= sizeof(msgParmList.text)) {
    return RC_CMS_MESSAGE_TOO_LONG;
  }

  msgParmList.messageLength = sizeof(prefix->text) + charactersToBeWritten;

  return cmsCallService(serverName, CROSS_MEMORY_SERVER_LOG_SERVICE_ID, &msgParmList, NULL);
}

int cmsGetConfigParm(const CrossMemoryServerName *serverName, const char *name,
                     CrossMemoryServerConfigParm *parm) {

  size_t nameLength = strlen(name);
  if (nameLength > CMS_CONFIG_PARM_MAX_NAME_LENGTH) {
    return RC_CMS_CONFIG_PARM_NAME_TOO_LONG;
  }

  CrossMemoryServerConfigServiceParm parmList = {0};
  memcpy(parmList.eyecatcher, CMS_CONFIG_SERVICE_PARM_EYECATCHER,
         sizeof(parmList.eyecatcher));
  memcpy(parmList.nameNullTerm, name, nameLength);

  int serviceRC = cmsCallService(
      serverName,
      CROSS_MEMORY_SERVER_CONFIG_SERVICE_ID,
      &parmList,
      NULL
  );
  if (serviceRC != RC_CMS_OK) {
    return serviceRC;
  }

  *parm = parmList.result;

  return RC_CMS_OK;
}

int cmsGetPCLogLevel(const CrossMemoryServerName *serverName) {

  int logLevel = ZOWE_LOG_NA;
  CrossMemoryServerGlobalArea *globalArea = NULL;
  if (cmsGetGlobalArea(serverName, &globalArea) == RC_CMS_OK) {
    logLevel = globalArea->pcLogLevel;
  }

  return logLevel;
}

CrossMemoryServerName cmsMakeServerName(const char *nameNullTerm) {

  CrossMemoryServerName name;
  int nameLength = strlen(nameNullTerm);
  int copySize = nameLength > sizeof(name.nameSpacePadded) ? sizeof(name.nameSpacePadded) : nameLength;
  memset(name.nameSpacePadded, ' ', sizeof(name.nameSpacePadded));
  memcpy(name.nameSpacePadded, nameNullTerm, copySize);

  return name;
}

static void fillServerStatus(int cmsRC, CrossMemoryServerStatus *status) {

  memset(status, 0, sizeof(CrossMemoryServerStatus));

  const char *description = NULL;
  if (0 <= cmsRC && cmsRC <= RC_CMS_MAX_RC) {
    description = CMS_RC_DESCRIPTION[cmsRC];
  }

  if (description == NULL) {
    description = "N/A";
  }

  status->cmsRC = cmsRC;
  strncpy(status->descriptionNullTerm, description,
          sizeof(status->descriptionNullTerm) - 1);

}

CrossMemoryServerStatus cmsGetStatus(const CrossMemoryServerName *serverName) {

  CrossMemoryServerStatus status = {0};

  int cmsRC = cmsCallService(serverName, CROSS_MEMORY_SERVER_STATUS_SERVICE_ID,
                             NULL, NULL);

  fillServerStatus(cmsRC, &status);

  return status;
}

#if defined(METTLE) && !defined(CMS_CLIENT)

#define crossmemoryServerDESCTs CMSDSECT
void crossmemoryServerDESCTs(){

  __asm(

#ifndef _LP64
      "         DS    0D                                                       \n"
      "PCHSTACK DSECT ,                                                        \n"
      /* parameters on stack */
      "PCHPARM  DS    0F                   PC handle parm list                 \n"
      "PCHPLEYE DS    CL8                  eyecatcher (RSPCHEYE)               \n"
      "PCHPLLPL DS    F                    latent parm list                    \n"
      "PCHPLUPL DS    F                    user parm list                      \n"
      "PCHPLPAD DS    0F                   padding                             \n"
      "PCHPARML EQU   *-PCHPARM                                                \n"
      /* 31-bit save area */
      "PCHSA    DS    0F                                                       \n"
      "PCHSARSV DS    CL4                                                      \n"
      "PCHSAPRV DS    A                    previous save area                  \n"
      "PCHSANXT DS    A                    next save area                      \n"
      "PCHSAGPR DS    15F                  GPRs                                \n"
      "PCHSTCKL EQU   *-PCHSTACK                                               \n"
      "         EJECT ,                                                        \n"
#else
      "         DS    0D                                                       \n"
      "PCHSTACK DSECT ,                                                        \n"
      /* parameters on stack */
      "PCHPARM  DS    0D                   PC handle parm list                 \n"
      "PCHPLEYE DS    CL8                  eyecatcher (RSPCHEYE)               \n"
      "PCHPLLPL DS    D                    latent parm list                    \n"
      "PCHPLUPL DS    D                    user parm list                      \n"
      "PCHPLPAD DS    0D                   padding                             \n"
      "PCHPARML EQU   *-PCHPARM                                                \n"
      /* 64-bit save area */
      "PCHSA    DS    0D                                                       \n"
      "PCHSARSV DS    CL4                                                      \n"
      "PCHSAEYE DS    CL4                  eyecatcher F1SA                     \n"
      "PCHSAGPR DS    15D                  GPRs                                \n"
      "PCHSAPRV DS    AD                   previous save area                  \n"
      "PCHSANXT DS    AD                   next save area                      \n"
      "PCHSTCKL EQU   *-PCHSTACK                                               \n"
      "         EJECT ,                                                        \n"
#endif

      "         DS    0D                                                       \n"
      "GLA      DSECT ,                                                        \n"
      "GLAEYE   DS    CL8                                                      \n"
      "GLAVER   DS    F                                                        \n"
      "GLAKEY   DS    X                                                        \n"
      "GLASBP   DS    X                                                        \n"
      "GLASIZE  DS    H                                                        \n"
      "GLAFLAG  DS    F                                                        \n"
      "GLARSV1  DS    CL60                                                     \n"
      "GLASNAM  DS    CL16                                                     \n"
      "GLALSAD  DS    A                                                        \n"
      "GLAASID  DS    H                                                        \n"
      "GLAPAD1  DS    2X                                                       \n"
      "GLASFLG  DS    F                                                        \n"
      "GLAECSN  DS    F                                                        \n"
      "GLARSV2  DS    CL48                                                     \n"
      "GLAPCCP  DS    A                                                        \n"
      "GLALPAM  DS    CL40                                                     \n"
      "GLAPCINF DS    0F                                                       \n"
      "GLAPCSSN DS    F                                                        \n"
      "GLAPCSSS DS    F                                                        \n"
      "GLAPCCPN DS    F                                                        \n"
      "GLAPCCPS DS    F                                                        \n"
      "GLAPCINL EQU   *-GLAPCINF                                               \n"
      "GLAPCLOG DS    F                                                        \n"
      "GLARCVP  DS    AD                                                       \n"
      "GLASTCP  DS    F                                                        \n"
      "GLARSV3  DS    CL484                                                    \n"
      "GLASRVT  DS    CL2048                                                   \n"
      "GLARSV4  DS    CL1320                                                   \n"
      "GLALEN   EQU   *-GLA                                                    \n"
      "         EJECT ,                                                        \n"

      "         DS    0D                                                       \n"
      "PCLPARM  DSECT ,                                                        \n"
      "PCLPPRM1 DS    F                                                        \n"
      "PCLPPRM2 DS    F                                                        \n"
      "         EJECT ,                                                        \n"

      "         DS    0H                                                       \n"
      "         CSVLPRET                                                       \n"
      "         EJECT ,                                                        \n"

      "         CSECT ,                                                        \n"
      "         RMODE ANY                                                      \n"
  );

}

#endif /* defined(METTLE) && !defined(CMS_CLIENT) */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
