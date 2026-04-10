

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __TSOSERVICE__
#define __TSOSERVICE__ 1

/*
   tsoservice.h — Programmatic TSO/E command execution from USS.

   Uses IKJTSOEV to establish a background TSO/E environment,
   then IKJEFTSR to execute commands repeatedly within that
   environment.  Output is captured from SYSTSPRT.

   The session is task-level: one session per thread, serialized
   access only.  The environment is automatically released when
   the owning task terminates, but tsoServiceTerm() should be
   called for clean shutdown.

   Architecture:
     LP64 USS process
       -> safeMalloc31 for all parameter lists
       -> SAM31 + BASR to call IKJTSOEV / IKJEFTSR / IKJEFTST
       -> SAM64 on return
       -> SYSTSPRT allocated to a USS temp file via dynalloc

   References:
     z/OS TSO/E Programming Services (IKJB700)
     research/ikjtsoev-ikjeftsr-64to31-handoff.md
*/

#ifdef METTLE
#include <metal/stdint.h>
#else
#include <stdint.h>
#endif

#include "zowetypes.h"

#ifndef __LONGNAME__
#define tsoServiceInit   TSOSINI
#define tsoServiceExec   TSOSEXEC
#define tsoServiceTerm   TSOSTERM
#define tsoServiceFreeResult TSOSFRES
#endif

/* Return codes */
#define RC_TSO_OK                   0
#define RC_TSO_ERROR                8
#define RC_TSO_LOAD_FAILED          9
#define RC_TSO_ENV_FAILED          10
#define RC_TSO_PLATFORM_FAILED     11
#define RC_TSO_EXEC_FAILED         12
#define RC_TSO_TERM_FAILED         13
#define RC_TSO_ALLOC_FAILED        14
#define RC_TSO_NOT_INITIALIZED     15
#define RC_TSO_OUTPUT_ERROR        16

/* Session flags */
#define TSO_FLAG_ENV_ACTIVE         0x00000001
#define TSO_FLAG_PLATFORM_ACTIVE    0x00000002

/* Forward declaration — internal layout in tsoservice.c */
typedef struct TsoSession_tag TsoSession;

/* Result from a single command execution */
typedef struct TsoResult_tag {
  char     eyecatcher[8];   /* "TSORESLT" */
  int      serviceRC;       /* R15 from IKJEFTSR */
  int      functionRC;      /* Invoked function return code (parm 4) */
  int      reasonCode;      /* TSF reason code (parm 5) */
  int      abendCode;       /* Abend code (parm 6) */
  char   **lines;           /* Output lines captured from SYSTSPRT */
  int      lineCount;       /* Number of output lines */
} TsoResult;

/*
 * Initialize a background TSO/E environment.
 *   - Allocates SYSTSPRT to a temp file in /tmp
 *   - Calls IKJTSOEV to create the environment
 *   - On success, *outSession is set; caller must call tsoServiceTerm()
 */
int tsoServiceInit(TsoSession **outSession);

/*
 * Execute a TSO command string within the established environment.
 *   - command is an EBCDIC string (e.g. "LISTDS 'SYS1.PARMLIB' MEMBERS")
 *   - On success, *outResult contains captured output lines
 *   - Caller must call tsoServiceFreeResult() on the result
 */
int tsoServiceExec(TsoSession *session, const char *command,
                   TsoResult **outResult);

/* Free a result returned by tsoServiceExec */
void tsoServiceFreeResult(TsoResult *result);

/* Terminate the TSO/E environment and release all resources */
int tsoServiceTerm(TsoSession *session);

#endif /* __TSOSERVICE__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
