

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
   tsoservice.c — Background TSO/E command execution from USS.

   Calls IKJTSOEV to set up a background TSO/E environment, then
   IKJEFTSR to run commands.  Output captured via SYSTSPRT DD
   allocated to a USS temp file.

   See research/ikjtsoev-ikjeftsr-64to31-handoff.md for design notes.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdio.h>
#include <metal/string.h>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "dynalloc.h"
#include "tsoservice.h"

/* ----------------------------------------------------------------
   AMODE switching macros (same pattern as ssi.c)
   ---------------------------------------------------------------- */

#ifdef _LP64
#define SAM31_IF_NEEDED " SAM31 \n"
#else
#define SAM31_IF_NEEDED ""
#endif

#ifdef _LP64
#define SAM64_IF_NEEDED " SAM64 \n"
#else
#define SAM64_IF_NEEDED ""
#endif

/* ----------------------------------------------------------------
   Constants
   ---------------------------------------------------------------- */

#define TSO_SESSION_EYECATCHER "TSOSESSN"
#define TSO_RESULT_EYECATCHER  "TSORESLT"

#define SYSTSPRT_DDNAME  "SYSTSPRT"
#define SYSTSIN_DDNAME   "SYSTSIN "

#define TSO_CMD_MAX_LEN  1024
#define TSO_OUTPUT_PATH  "/tmp/zowe_tso_systsprt"

/* IKJEFTSR flag byte positions (big-endian fullword):
     byte 1 (bits 0-7):  reserved (0)
     byte 2 (bits 8-15): 0x00=isolated, 0x01=unisolated
     byte 3 (bits 16-23): 0x00=dump, 0x01=no dump
     byte 4 (bits 24-31): 0x01=command/CLIST/REXX, 0x02=program
*/
#define TSR_FLAG_UNISOLATED  0x00010000
#define TSR_FLAG_NODUMP      0x00000100
#define TSR_FLAG_COMMAND     0x00000001
#define TSR_FLAG_PROGRAM     0x00000002

/* Default flags: unisolated, no dump, command */
#define TSR_FLAGS_CMD_DEFAULT (TSR_FLAG_UNISOLATED | TSR_FLAG_NODUMP | TSR_FLAG_COMMAND)

/* ----------------------------------------------------------------
   Internal session structure
   ---------------------------------------------------------------- */

struct TsoSession_tag {
  char      eyecatcher[8];        /* "TSOSESSN" */
  uint32_t  flags;                /* TSO_FLAG_xxx */
  uint32_t  lastServiceRC;
  uint32_t  lastFunctionRC;
  uint32_t  lastReasonCode;
  uint32_t  lastAbendCode;

  /* Saved from IKJTSOEV — 31-bit addresses stored as uint32_t */
  uint32_t  cpplAddr;             /* CPPL address returned by IKJTSOEV */

  /* IKJEFTSI platform token (4 fullwords) */
  uint32_t  platformToken[4];

  /* Output capture */
  char      systsprtPath[256];    /* USS temp file path */
  long      systsprtOffset;       /* Read offset into temp file */

  /* Cached entry points (31-bit addresses) */
  uint32_t  epTsoev;              /* IKJTSOEV entry point */
  uint32_t  epEftsr;              /* IKJEFTSR entry point */
  uint32_t  epEftsi;              /* IKJEFTSI entry point */
  uint32_t  epEftst;              /* IKJEFTST entry point */
};

/* ----------------------------------------------------------------
   Helper: mask to 31-bit address
   ---------------------------------------------------------------- */

static uint32_t addr31(const void *p) {
  return (uint32_t)((uintptr_t)p & 0x7FFFFFFFU);
}

/* ----------------------------------------------------------------
   Helper: set high-order bit (last-parameter marker)
   ---------------------------------------------------------------- */

static uint32_t addr31Last(const void *p) {
  return addr31(p) | 0x80000000U;
}

/* ----------------------------------------------------------------
   LOAD a module by name and return its 31-bit entry point.

   Uses LOAD (SVC 8) with EPLOC form:
     R0 = address of 8-byte module name
     R1 = 0 (default search order: JOBLIB, STEPLIB, LINKLIB, LPA)
   On return:
     R0 = entry point address
     R15 = return code (0 = success)
   ---------------------------------------------------------------- */

#ifdef __ZOWE_OS_ZOS
static int loadModule(const char *name, uint32_t *entryOut) {
  char eploc[8];
  int nameLen;
  int rc = 0;
  uint32_t entry = 0;

  memset(eploc, ' ', 8);
  nameLen = strlen(name);
  if (nameLen > 8) nameLen = 8;
  memcpy(eploc, name, nameLen);

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      char modName[8];
      char saveArea[72];
    )
  );

  memcpy(parms31->modName, eploc, 8);
  memset(parms31->saveArea, 0, 72);

  __asm(ASM_PREFIX
    " LA    0,%[mod]   \n"   /* R0 -> 8-byte EPLOC */
    " XGR   1,1        \n"   /* R1 = 0 (no DCB)   */
    SAM31_IF_NEEDED
    " SVC   8          \n"   /* LOAD               */
    SAM64_IF_NEEDED
    " ST    15,%[rc]   \n"
    " ST    0,%[ep]    \n"
    : [rc]"=m"(rc), [ep]"=m"(entry)
    : [mod]"m"(parms31->modName)
    : "r0", "r1", "r14", "r15"
  );

  FREE_STRUCT31(STRUCT31_NAME(parms31));

  if (rc == 0 && entry != 0) {
    *entryOut = entry;
    return RC_TSO_OK;
  }
  return RC_TSO_LOAD_FAILED;
}
#endif

/* ----------------------------------------------------------------
   DELETE (unload) a previously LOADed module.
   SVC 9: R0 = address of 8-byte module name, R1 = 0
   ---------------------------------------------------------------- */

#ifdef __ZOWE_OS_ZOS
static void deleteModule(const char *name) {
  char eploc[8];
  int nameLen;

  memset(eploc, ' ', 8);
  nameLen = strlen(name);
  if (nameLen > 8) nameLen = 8;
  memcpy(eploc, name, nameLen);

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      char modName[8];
    )
  );

  memcpy(parms31->modName, eploc, 8);

  __asm(ASM_PREFIX
    " LA    0,%[mod]   \n"
    " XGR   1,1        \n"
    SAM31_IF_NEEDED
    " SVC   9          \n"   /* DELETE */
    SAM64_IF_NEEDED
    :
    : [mod]"m"(parms31->modName)
    : "r0", "r1", "r14", "r15"
  );

  FREE_STRUCT31(STRUCT31_NAME(parms31));
}
#endif

/* ----------------------------------------------------------------
   Generic service caller: given a 31-bit entry point and a
   31-bit parameter list address, call the service with standard
   OS linkage (R1=parmlist, R13=savearea, R15=entry, BASR 14,15).

   Returns R15 (service return code).

   Follows the exact pattern from ssi.c callSSI().
   ---------------------------------------------------------------- */

#ifdef __ZOWE_OS_ZOS
static int callService31(uint32_t entryPoint, uint32_t parmlistAddr) {
  int returnCode = 0;
  int r13;

#ifdef _LP64
  char *data31 = (char *__ptr32)safeMalloc31(72, "TSOsvc-sa");
#else
  char data31[72];
#endif
  char *__ptr32 saveArea = (char *__ptr32)data31;
  memset(saveArea, 0, 72);

  /* Chain to current save area */
  __asm(" ST 13,%0 " : : "m"(r13) :);
  ((int *)saveArea)[1] = r13;   /* back chain */

  __asm(ASM_PREFIX
    " LLGF  1,%[pl]   \n"   /* R1 = parmlist address (31-bit)    */
    " L     13,%[sa]   \n"   /* R13 = our 31-bit save area        */
    " LLGF  15,%[ep]   \n"   /* R15 = entry point (31-bit)        */
    SAM31_IF_NEEDED
    " BASR  14,15      \n"   /* Call the service                  */
    SAM64_IF_NEEDED
    " ST    15,%[rc]   \n"
    : [rc]"=m"(returnCode)
    : [pl]"m"(parmlistAddr), [sa]"m"(saveArea), [ep]"m"(entryPoint)
    : "r0", "r1", "r13", "r14", "r15"
  );

#ifdef _LP64
  safeFree31((char *)data31, 72);
#endif

  return returnCode;
}
#endif

/* ----------------------------------------------------------------
   Call IKJTSOEV to establish a background TSO/E environment.

   Parameters (5 fullwords, last has HOB):
     1: reserved (address of fullword zero)
     2: return code (output)
     3: reason code (output)
     4: info area (output)
     5: CPPL address (output) — HOB set
   ---------------------------------------------------------------- */

#ifdef __ZOWE_OS_ZOS
static int callIkjTsoev(TsoSession *session) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint32_t parmList[5];
      uint32_t reserved;
      uint32_t rc;
      uint32_t rsn;
      uint32_t info;
      uint32_t cpplOut;
    )
  );

  parms31->reserved = 0;
  parms31->rc       = 0;
  parms31->rsn      = 0;
  parms31->info     = 0;
  parms31->cpplOut  = 0;

  /* Build OS-style address list with HOB on last entry */
  parms31->parmList[0] = addr31(&parms31->reserved);
  parms31->parmList[1] = addr31(&parms31->rc);
  parms31->parmList[2] = addr31(&parms31->rsn);
  parms31->parmList[3] = addr31(&parms31->info);
  parms31->parmList[4] = addr31Last(&parms31->cpplOut);

  int svcRC = callService31(session->epTsoev, addr31(parms31->parmList));

  session->lastServiceRC  = svcRC;
  session->lastFunctionRC = parms31->rc;
  session->lastReasonCode = parms31->rsn;
  session->cpplAddr       = parms31->cpplOut;

  int result = RC_TSO_OK;
  if (svcRC != 0 || parms31->rc != 0) {
    result = RC_TSO_ENV_FAILED;
  }

  FREE_STRUCT31(STRUCT31_NAME(parms31));
  return result;
}
#endif

/* ----------------------------------------------------------------
   Call IKJEFTSR to execute a TSO command.

   Parameters for command invocation (6 required, up to 9):
     1: flags fullword
     2: command buffer address (EBCDIC, blank-padded to cmdLen)
     3: command length (fullword)
     4: function return code (output)
     5: TSF reason code (output)
     6: abend code (output) — HOB if no params 7-9
     7: (optional) address of fullword zero
     8: (optional) CPPL address (4 fullwords, or zeroes)
     9: (optional) platform token address (4 fullwords)

   For the first version we pass 6 parameters (no CPPL/token).
   IKJEFTSR constructs its own CPPL in the unauthorized path.
   ---------------------------------------------------------------- */

#ifdef __ZOWE_OS_ZOS
static int callIkjEftsr(TsoSession *session, const char *command) {

  int cmdLen = strlen(command);
  if (cmdLen > TSO_CMD_MAX_LEN) cmdLen = TSO_CMD_MAX_LEN;

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint32_t parmList[6];
      uint32_t flags;
      char     cmdBuf[TSO_CMD_MAX_LEN];
      uint32_t cmdLength;
      uint32_t funcRC;
      uint32_t tsfReason;
      uint32_t abendCode;
    )
  );

  parms31->flags     = TSR_FLAGS_CMD_DEFAULT;
  memset(parms31->cmdBuf, ' ', TSO_CMD_MAX_LEN);
  memcpy(parms31->cmdBuf, command, cmdLen);
  parms31->cmdLength = cmdLen;
  parms31->funcRC    = 0;
  parms31->tsfReason = 0;
  parms31->abendCode = 0;

  /* 6-parameter form: HOB on parameter 6 */
  parms31->parmList[0] = addr31(&parms31->flags);
  parms31->parmList[1] = addr31(parms31->cmdBuf);
  parms31->parmList[2] = addr31(&parms31->cmdLength);
  parms31->parmList[3] = addr31(&parms31->funcRC);
  parms31->parmList[4] = addr31(&parms31->tsfReason);
  parms31->parmList[5] = addr31Last(&parms31->abendCode);

  int svcRC = callService31(session->epEftsr, addr31(parms31->parmList));

  session->lastServiceRC  = svcRC;
  session->lastFunctionRC = parms31->funcRC;
  session->lastReasonCode = parms31->tsfReason;
  session->lastAbendCode  = parms31->abendCode;

  int result = RC_TSO_OK;
  /* RC 0 = normal, RC 4 = command issued warning */
  if (svcRC != 0 && svcRC != 4) {
    result = RC_TSO_EXEC_FAILED;
  }

  FREE_STRUCT31(STRUCT31_NAME(parms31));
  return result;
}
#endif

/* ----------------------------------------------------------------
   Allocate SYSTSPRT and SYSTSIN DDs using dynalloc.

   SYSTSPRT -> USS temp file (for capturing output)
   SYSTSIN  -> DUMMY (we're not feeding command input through it)
   ---------------------------------------------------------------- */

static int allocateTsoDDs(TsoSession *session) {
  char errorBuf[256];
  int rc;

  /* Generate a unique temp file path using PID */
  snprintf(session->systsprtPath, sizeof(session->systsprtPath),
           "%s_%d.txt", TSO_OUTPUT_PATH, (int)getpid());

  /* Allocate SYSTSPRT to the temp file */
  rc = dynallocUSSOutput(SYSTSPRT_DDNAME, session->systsprtPath, errorBuf);
  if (rc != 0) {
    printf("tsoService: SYSTSPRT alloc failed: %s\n", errorBuf);
    return RC_TSO_ALLOC_FAILED;
  }
  /* Allocate SYSTSIN to /dev/null — IKJTSOEV opens it for command input
     but we feed commands via IKJEFTSR, not SYSTSIN */
  rc = dynallocUSSOutput(SYSTSIN_DDNAME, "/dev/null", errorBuf);
  if (rc != 0) {
    printf("tsoService: SYSTSIN alloc failed: %s\n", errorBuf);
    /* Non-fatal: IKJTSOEV will allocate SYSTSIN as DUMMY if missing */
  }

  session->systsprtOffset = 0;
  return RC_TSO_OK;
}

/* ----------------------------------------------------------------
   Deallocate the TSO DDs
   ---------------------------------------------------------------- */

static void deallocateTsoDDs(TsoSession *session) {
  DeallocDDName(SYSTSPRT_DDNAME);
  DeallocDDName(SYSTSIN_DDNAME);
}

static void cleanupTempFile(TsoSession *session) {
  if (session->systsprtPath[0] != '\0') {
    unlink(session->systsprtPath);
    session->systsprtPath[0] = '\0';
  }
}

/* ----------------------------------------------------------------
   Read new output lines from SYSTSPRT temp file since last offset.
   ---------------------------------------------------------------- */

static int readNewOutput(TsoSession *session, char ***linesOut, int *lineCountOut) {
  *linesOut = NULL;
  *lineCountOut = 0;

  FILE *f = fopen(session->systsprtPath, "r");
  if (!f) {
    return RC_TSO_OUTPUT_ERROR;
  }

  /* Seek to where we left off */
  if (session->systsprtOffset > 0) {
    fseek(f, session->systsprtOffset, SEEK_SET);
  }

  /* Read new lines into a dynamic array */
  int capacity = 64;
  int count = 0;
  char **lines = (char **)safeMalloc(capacity * sizeof(char *), "TSOlines");
  if (!lines) {
    fclose(f);
    return RC_TSO_ALLOC_FAILED;
  }

  char lineBuf[512];
  while (fgets(lineBuf, sizeof(lineBuf), f)) {
    /* Strip trailing newline/CR */
    int len = strlen(lineBuf);
    while (len > 0 && (lineBuf[len - 1] == '\n' || lineBuf[len - 1] == '\r')) {
      lineBuf[--len] = '\0';
    }

    if (count >= capacity) {
      int newCap = capacity * 2;
      char **newLines = (char **)safeMalloc(newCap * sizeof(char *), "TSOlines");
      if (!newLines) {
        break;
      }
      memcpy(newLines, lines, count * sizeof(char *));
      safeFree((char *)lines, capacity * sizeof(char *));
      lines = newLines;
      capacity = newCap;
    }

    lines[count] = (char *)safeMalloc(len + 1, "TSOline");
    if (lines[count]) {
      memcpy(lines[count], lineBuf, len + 1);
      count++;
    }
  }

  /* Update offset for next read */
  session->systsprtOffset = ftell(f);
  fclose(f);

  *linesOut = lines;
  *lineCountOut = count;
  return RC_TSO_OK;
}

/* ----------------------------------------------------------------
   Public API
   ---------------------------------------------------------------- */

int tsoServiceInit(TsoSession **outSession) {
  *outSession = NULL;

  TsoSession *session = (TsoSession *)safeMalloc(sizeof(TsoSession), "TsoSession");
  if (!session) {
    return RC_TSO_ALLOC_FAILED;
  }
  memset(session, 0, sizeof(TsoSession));
  memcpy(session->eyecatcher, TSO_SESSION_EYECATCHER, 8);

#ifdef __ZOWE_OS_ZOS
  /* Load the TSO service modules */
  int rc;

  rc = loadModule("IKJTSOEV", &session->epTsoev);
  if (rc != RC_TSO_OK) {
    printf("tsoService: LOAD IKJTSOEV failed\n");
    safeFree((char *)session, sizeof(TsoSession));
    return RC_TSO_LOAD_FAILED;
  }
  rc = loadModule("IKJEFTSR", &session->epEftsr);
  if (rc != RC_TSO_OK) {
    printf("tsoService: LOAD IKJEFTSR failed\n");
    deleteModule("IKJTSOEV");
    safeFree((char *)session, sizeof(TsoSession));
    return RC_TSO_LOAD_FAILED;
  }
  /* Allocate SYSTSPRT and SYSTSIN DDs */
  rc = allocateTsoDDs(session);
  if (rc != RC_TSO_OK) {
    deleteModule("IKJEFTSR");
    deleteModule("IKJTSOEV");
    safeFree((char *)session, sizeof(TsoSession));
    return rc;
  }

  /* Establish the background TSO/E environment */
  rc = callIkjTsoev(session);
  if (rc != RC_TSO_OK) {
    printf("tsoService: IKJTSOEV failed svcRC=%d funcRC=%d rsn=%d\n",
           session->lastServiceRC,
           session->lastFunctionRC,
           session->lastReasonCode);
    deallocateTsoDDs(session);
    cleanupTempFile(session);
    deleteModule("IKJEFTSR");
    deleteModule("IKJTSOEV");
    safeFree((char *)session, sizeof(TsoSession));
    return rc;
  }

  session->flags |= TSO_FLAG_ENV_ACTIVE;
#else
  /* Non-z/OS stub: environment always "active" for compilation */
  session->flags |= TSO_FLAG_ENV_ACTIVE;
#endif

  *outSession = session;
  return RC_TSO_OK;
}


int tsoServiceExec(TsoSession *session, const char *command,
                   TsoResult **outResult) {
  *outResult = NULL;

  if (!session || memcmp(session->eyecatcher, TSO_SESSION_EYECATCHER, 8) != 0) {
    return RC_TSO_NOT_INITIALIZED;
  }
  if (!(session->flags & TSO_FLAG_ENV_ACTIVE)) {
    return RC_TSO_NOT_INITIALIZED;
  }

  TsoResult *result = (TsoResult *)safeMalloc(sizeof(TsoResult), "TsoResult");
  if (!result) {
    return RC_TSO_ALLOC_FAILED;
  }
  memset(result, 0, sizeof(TsoResult));
  memcpy(result->eyecatcher, TSO_RESULT_EYECATCHER, 8);

#ifdef __ZOWE_OS_ZOS
  /*
   * Strategy: use popen("tsocmd ...") to capture output.
   *
   * IKJEFTSR writes to SYSTSPRT via QSAM, which is held open by
   * the IKJTSOEV environment and won't flush until the environment
   * terminates.  Until we solve SYSTSPRT harvesting (possibly via
   * PUTLINE exit or a second task), use tsocmd as the exec backend.
   *
   * The IKJTSOEV environment is still useful — it proves the TSO/E
   * background environment concept works.  When we solve output
   * capture, callIkjEftsr() is ready to go.
   *
   * TODO: replace popen with callIkjEftsr + PUTLINE exit capture
   */
  char cmdBuf[1200];
  snprintf(cmdBuf, sizeof(cmdBuf), "tsocmd \"%s\" 2>&1", command);

  FILE *pipe = popen(cmdBuf, "r");
  if (!pipe) {
    result->serviceRC = -1;
    *outResult = result;
    return RC_TSO_EXEC_FAILED;
  }

  int capacity = 64;
  int count = 0;
  char **lines = (char **)safeMalloc(capacity * sizeof(char *), "TSOlines");
  char lineBuf[512];

  if (lines) {
    while (fgets(lineBuf, sizeof(lineBuf), pipe)) {
      int len = strlen(lineBuf);
      while (len > 0 && (lineBuf[len - 1] == '\n' || lineBuf[len - 1] == '\r')) {
        lineBuf[--len] = '\0';
      }

      if (count >= capacity) {
        int newCap = capacity * 2;
        char **newLines = (char **)safeMalloc(newCap * sizeof(char *), "TSOlines");
        if (!newLines) break;
        memcpy(newLines, lines, count * sizeof(char *));
        safeFree((char *)lines, capacity * sizeof(char *));
        lines = newLines;
        capacity = newCap;
      }

      lines[count] = (char *)safeMalloc(len + 1, "TSOline");
      if (lines[count]) {
        memcpy(lines[count], lineBuf, len + 1);
        count++;
      }
    }
  }

  int status = pclose(pipe);
  result->serviceRC = WEXITSTATUS(status);
  result->lines = lines;
  result->lineCount = count;

#else
  /* Non-z/OS stub */
  result->serviceRC = 0;
#endif

  *outResult = result;
  return RC_TSO_OK;
}


void tsoServiceFreeResult(TsoResult *result) {
  if (!result) return;
  if (memcmp(result->eyecatcher, TSO_RESULT_EYECATCHER, 8) != 0) return;

  if (result->lines) {
    for (int i = 0; i < result->lineCount; i++) {
      if (result->lines[i]) {
        safeFree(result->lines[i], strlen(result->lines[i]) + 1);
      }
    }
    safeFree((char *)result->lines, result->lineCount * sizeof(char *));
  }

  safeFree((char *)result, sizeof(TsoResult));
}


int tsoServiceTerm(TsoSession *session) {
  if (!session) return RC_TSO_OK;
  if (memcmp(session->eyecatcher, TSO_SESSION_EYECATCHER, 8) != 0) {
    return RC_TSO_NOT_INITIALIZED;
  }

#ifdef __ZOWE_OS_ZOS
  /* If platform was initialized, terminate it */
  if (session->flags & TSO_FLAG_PLATFORM_ACTIVE) {
    /* TODO: call IKJEFTST when platform support is added */
    session->flags &= ~TSO_FLAG_PLATFORM_ACTIVE;
  }

  /* Deallocate DDs and clean up temp file */
  deallocateTsoDDs(session);
  cleanupTempFile(session);

  /* DELETE (unload) the modules */
  deleteModule("IKJEFTSR");
  deleteModule("IKJTSOEV");
#endif

  session->flags = 0;
  memset(session->eyecatcher, 0, 8);
  safeFree((char *)session, sizeof(TsoSession));

  return RC_TSO_OK;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
