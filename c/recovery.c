

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include "stddef.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "cellpool.h"
#include "le.h"
#include "recovery.h"
#include "utils.h"

#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#include "qsam.h"
#endif

#if defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)
#include "errno.h"
#include "pthread.h"
#include "signal.h"
#include "unistd.h"
#endif

#if !defined(__ZOWE_OS_ZOS) && !defined(__ZOWE_OS_AIX) && !defined(__ZOWE_OS_LINUX)
#error "unsupported platform"
#endif

#define RECOVERY_TRACING  0

#if defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

RecoveryContext __thread *theRecoveryContext = NULL;

typedef void (*sighandler_t)(int);

pthread_mutex_t handlerMutex = PTHREAD_MUTEX_INITIALIZER;
unsigned int contextCounter = 0;
struct sigaction prevSIGILLAction;
struct sigaction prevSIGFPEAction;
struct sigaction prevSIGBUSAction;
struct sigaction prevSIGSEGVAction;

#endif

#ifdef __ZOWE_OS_ZOS

#ifndef METTLE
#if (!defined(_LP64) && defined(__XPLINK__)) || (defined(_LP64) && !defined(__XPLINK__))
#error "unsupported linkage convention"
#endif
#endif

ZOWE_PRAGMA_PACK

typedef struct ESTAEXFeedback_tag {
  int returnCode;
  int reasonCode;
} ESTAEXFeedback;

#define RCVR_ESTAEX_FRR_EXISTS  0x30

ZOWE_PRAGMA_PACK_RESET

static void resetESPIE(int token) {

  int wasProblemState = supervisorMode(FALSE);

  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ESPIE RESET,(%0)                                               \n"
      "         SYSSTATE POP                                                   \n"
      :
      : "r"(token)
      : "r0", "r1", "r14", "r15"
  );

  if (!wasProblemState) {
    supervisorMode(TRUE);
  }

}

static int setDummyESPIE() {

  int wasProblemState = supervisorMode(FALSE);

  int dummyUserExit = 0;

  __asm(
      ASM_PREFIX
      "         LARL  2,DUMEXIT                                                \n"
      "         ST    2,%0                                                     \n"
      "         J     DEBARND                                                  \n"
      "DUMEXIT  DS    0H                                                       \n"

      /*** EXIT ROUTINE START ***/

      "         OI    153(1),X'10'        SET EPIEPERC BIT TO PERCOLATE        \n"
      "         BR    14                                                       \n"

      /*** EXIT ROUTINE END ***/

      "DEBARND  NR    2,2                                                      \n"
      : "=m"(dummyUserExit)
      :
      : "r2"
  );


  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      char espieParmList[24]; /* 16 is required, the rest is for safety */
    )
  );

  int token = 0;
  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         IPM   2                                                        \n"
      "         ESPIE SET,(%0),((1,15)),PARAM=(%1),MF=(E,(%1))                 \n"
      "         SPM   2                                                        \n"
      "         ST    1,0(%2)                                                  \n"
      "         SYSSTATE POP                                                   \n"
      :
      : "r"(dummyUserExit), "r"(&parms31->espieParmList), "r"(&token)
      : "r0", "r1", "r2", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  if (!wasProblemState) {
    supervisorMode(TRUE);
  }

  return token;
}

#define RCVR_ESTAEX_FLAG_PURGE_HALT           0x01
#define RCVR_ESTAEX_FLAG_PURGE_NONE           0x02
#define RCVR_ESTAEX_FLAG_ASYNCH               0x04
#define RCVR_ESTAEX_FLAG_TOKEN                0x08
#define RCVR_ESTAEX_FLAG_NON_INTERRUPTIBLE    0x10
#define RCVR_ESTAEX_FLAG_XCTL                 0x20
#define RCVR_ESTAEX_FLAG_RECORD_SDWA          0x40
#define RCVR_ESTAEX_FLAG_RUN_ON_TERM          0x80

static ESTAEXFeedback setESTAEX(void * __ptr32 userExit, void *userData, char flags) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      ESTAEXFeedback feedback;
      char estaexParmList[24]; /* 20 is required, the rest is safety */
    )
  );

  char *estaexFlags = &parms31->estaexParmList[0];
  *estaexFlags = flags;

  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ESTAEX (%0),PARAM=(%1),MF=(E,(%2))                             \n"
      "         ST     15,0(%3)                                                \n"
      "         ST     0,0(%4)                                                 \n"
      "         SYSSTATE POP                                                   \n"
      :
      : "r"(userExit), "r"(userData), "r"(&parms31->estaexParmList),
        "r"(&parms31->feedback.returnCode), "r"(&parms31->feedback.reasonCode)
      : "r0", "r1", "r14", "r15"
  );

  ESTAEXFeedback localFeedback;
  localFeedback = parms31->feedback;

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  return localFeedback;
}

static ESTAEXFeedback deleteESTAEX() {

  ESTAEXFeedback localFeedback;

  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ESTAEX 0                                                       \n"
      "         ST     15,0(%0)                                                \n"
      "         ST     0,0(%1)                                                 \n"
      "         SYSSTATE POP                                                   \n"
      :
      : "r"(&localFeedback.returnCode),
        "r"(&localFeedback.reasonCode)
      : "r0", "r1", "r14", "r15"
  );

  return localFeedback;
}

static void setFRR(void * __ptr32 userExit, void *userData,
                   bool nonInterruptible) {

  int wasProblem = supervisorMode(TRUE);
  int oldKey = setKey(0);

  union {
    void *userData;
    char data[24];
  } * __ptr32 parmArea = NULL;

  if (nonInterruptible) {

    __asm(
        ASM_PREFIX
        "         SYSSTATE PUSH                                                  \n"
        "         SYSSTATE OSREL=ZOSV1R8                                         \n"
        "         SETFRR A"
        ",FRRAD=(%[frr])"
        ",WRKREGS=(9,10)"
        ",PARMAD=%[parm]"
        ",CANCEL=NO"
        ",MODE=FULLXM"
        ",SDWALOC31=YES                                                          \n"
        "         SYSSTATE POP                                                   \n"
        : [parm]"=m"(parmArea)
        : [frr]"r"(userExit)
        : "r9", "r10"
    );

  } else {

    __asm(
        ASM_PREFIX
        "         SYSSTATE PUSH                                                  \n"
        "         SYSSTATE OSREL=ZOSV1R8                                         \n"
        "         SETFRR A"
        ",FRRAD=(%[frr])"
        ",WRKREGS=(9,10)"
        ",PARMAD=%[parm]"
        ",CANCEL=YES"
        ",MODE=FULLXM"
        ",SDWALOC31=YES                                                          \n"
        "         SYSSTATE POP                                                   \n"
        : [parm]"=m"(parmArea)
        : [frr]"r"(userExit)
        : "r9", "r10"
    );

  }

  parmArea->userData = userData;

  setKey(oldKey);
  if (wasProblem) {
    supervisorMode(FALSE);
  }

}

static void deleteFRR(void) {

  int wasProblem = supervisorMode(TRUE);
  int oldKey = setKey(0);

  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R8                                         \n"
      "         SETFRR D,WRKREGS=(9,10)                                        \n"
#ifndef METTLE
      "* Prevent LE compiler errors caused by the way XL C treats __asm        \n"
      "         NOPR  0                                                        \n"
#endif
      "         SYSSTATE POP                                                   \n"
      :
      :
      : "r9", "r10"
  );

  setKey(oldKey);
  if (wasProblem) {
    supervisorMode(FALSE);
  }

}

static void *storageObtain(int size){
  char * __ptr32 data = NULL;
  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         STORAGE OBTAIN,LENGTH=(%1),LOC=31,SP=132,CALLRKY=YES           \n"
      "         ST     1,%0                                                    \n"
      "         SYSSTATE POP                                                   \n"
      : "=m"(data)
      : "r"(size)
      : "r0", "r1", "r14", "r15"
  );
  return data;
}

static void storageRelease(void *data, int size){
  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      /* TODO consider a different subpool */
      "         STORAGE RELEASE,LENGTH=(%0),ADDR=(%1),SP=132,CALLRKY=YES       \n"
      "         SYSSTATE POP                                                   \n"
      :
      : "r"(size), "r"(data)
      : "r0", "r1", "r14", "r15"
  );
}

#ifdef RCVR_CPOOL_STATES

  #ifndef RCVR_STATE_POOL_SUBPOOL
  #define RCVR_STATE_POOL_SUBPOOL 132
  #endif

  #ifndef RCVR_STATE_POOL_PRIMARY_COUNT
  #define RCVR_STATE_POOL_PRIMARY_COUNT    32
  #endif

#endif /* RCVR_CPOOL_STATES */

static RecoveryStateEntry *allocRecoveryStateEntry(RecoveryContext *context) {

  RecoveryStateEntry *entry;

#ifdef RCVR_CPOOL_STATES
  entry = cellpoolGet(context->stateCellPool, true);
#else
  entry = storageObtain(sizeof(RecoveryStateEntry));
#endif /* RCVR_CPOOL_STATES */

  if (entry == NULL) {
    return NULL;
  }

  memset(entry, 0, sizeof(RecoveryStateEntry));
  memcpy(entry->eyecatcher, "RSRSENTR", sizeof(entry->eyecatcher));
  entry->structVersion = RCVR_STATE_VERSION;

#ifdef RCVR_CPOOL_STATES
  entry->flags |= RCVR_FLAG_CPOOL_BASED;
#endif

  return entry;
}

static void freeRecoveryStateEntry(RecoveryContext *context,
                                   RecoveryStateEntry *entry) {

#ifdef RCVR_CPOOL_STATES

  if (entry->flags & RCVR_FLAG_CPOOL_BASED) {
    return cellpoolFree(context->stateCellPool, entry);
  } else {
    return storageRelease(entry, sizeof(RecoveryStateEntry));
  }

#else

  return storageRelease(entry, sizeof(RecoveryStateEntry));

#endif /* RCVR_CPOOL_STATES */

}

static void * __ptr32 getRecoveryRouterAddress() {

  void * __ptr32 address = NULL;

  __asm(
      ASM_PREFIX
      "         LARL  10,RCVEXIT                                               \n"
      "         ST    10,%0                                                    \n"
      "         J     RCVTRTN                                                  \n"
      "RCVEXIT  DS    0H                                                       \n"

      /*** EXIT ROUTINE START ***/

      /*
       * Register usage:
       * R0-R7    - work registers (router work registers and passing parms to
       *            LE and Metal functions)
       * R8       - recovery state
       * R9       - SDWA
       * R10      - routine addressability
       * R11      - recovery context
       * R12-R13  - work registers (CAA, stack for LE, Metal function calls)
       * R14      - return address
       *
       */

      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  10,RCVEXIT                                               \n"
      "         USING RCVEXIT,10                                               \n"
      /* validate input */
      "         CHI   0,12                HAVE SDWA (CAN BE 12 IN TCB ONLY)?   \n"
      "         JE    RCVRET              NO, LEAVE                            \n"
      "         DS    0H                                                       \n"
      "         LGR   9,1                 SDWA WILL BE IN R9                   \n"
      "         USING SDWA,9                                                   \n"
      "         LTGF  2,SDWAPARM          LOAD RECOVERY PARMS HANDLE           \n"
      "         BZ    RCVRET              ZERO? YES, LEAVE                     \n"
#ifdef _LP64
      "         LTG   11,0(2)             LOAD RECOVERY ROUTER CONTEXT         \n"
#else
      "         LT    11,0(2)             LOAD RECOVERY ROUTER CONTEXT         \n"
#endif
      "         BZ    RCVRET              ZERO? YES, LEAVE                     \n"
      "         USING RCVCTX,11                                                \n"
      "         CLC   RCXEYECT,=C'RSRCVCTX' EYECATHER IS VALID?                \n"
      "         BNE   RCVRET              NO, LEAVE                            \n"
#if !defined(METTLE) && !defined(_LP64)
      /* check if the LE ESTAE needs to handle this */
      "         L     12,RCXCAA           LOAD CAA                             \n"
      "         USING CEECAA,12                                                \n"
      "         L     15,CEECAAHERP       ADDRESS OF CEE3ERP                   \n"
      "         BALR  14,15                                                    \n"
      "         CFI   15,4                LET LE HANDLE THIS?                  \n"
      "         BE    RCVRET              YES, SETRP HAS BEEN SET UP           \n"
#endif
      /* see if we need to change PSW key */
      "         LA    3,1                 STACKED STATE 1                      \n"
      "         ESTA  2,3                 GET STATE                            \n"
      "         SRL   2,16                MOVE KEY TO BIT 24-27                \n"
      "         N     2,=X'000000F0'      CLEAR OUT THE REST                   \n"
      "         ICM   3,X'0001',RCXPRKEY  LOAD ROUTER KEY INTO BITS 24-27      \n"
      "         N     3,=X'000000F0'      CLEAR OUT THE REST                   \n"
      "         CR    2,3                 EQUAL?                               \n"
      "         BE    RCVFRL0             YES, SKIP SETTING KEY                \n"
      "         SPKA  0(3)                SET KEY TO ROUTER VALUE              \n"
      /* process recovery states */
      "RCVFRL0  DS    0H                  CHECK IF ANY STATES ARE AVAILABLE    \n"
      "         LLGT  8,RCXRSC            LOAD RECOVERY STATE ENTRY            \n"
      "         LTR   8,8                 ZERO?                                \n"
      "         BNZ   RCVFRL05            NO, GO CHECK FLAGS                   \n"
      "         LA    1,RCVXINF           LOAD ROUTER SERVICE INFO             \n"
      "         BRAS  14,RCVSIFLB         RECORD IT, REMOVE CONTEXT, PERCOLATE \n"
      "         TM    RCXFLAG1,R@CF1USP   USER STATE POOL?                     \n"
      "         BZ    RCVFRL04            NO, DO NOT FREE IT                   \n"
      "         LT    2,RCXSCPID          CELL POOL ZERO?                      \n"
      "         BZ    RCVFRL04            YES, DO NOT FREE IT                  \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         CPOOL DELETE,CPID=(2)     FREE THE STATE CELL POOL             \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "RCVFRL04 DS    0H                                                       \n"
      "         TM    RCXFLAG1,R@CF1UCX   USER CONTEXT?                        \n"
      "         BNZ   RCVRET              YES, DO NOT FREE                     \n"
      "         LA    4,RCXLEN            LENGTH OF CONTEXT                    \n"
      "         STORAGE RELEASE,LENGTH=(4),ADDR=(11),SP=132,CALLRKY=YES        \n"
      "         B     RCVRET                                                   \n"
      "RCVFRL05 DS    0H                                                       \n"
      "         USING RCVSTATE,8                                               \n"
      "         TM    RSTSTATE,R@STENBL   IS IT ENABLED?                       \n"
      "         BZ    RCVFRL5             NO, REMOVE                           \n"
      "         NI    RSTSTATE,X'FF'-R@STENBL  DISABLE                         \n"
      "         OI    RSTSTATE,R@STABND   MARK ABENDED                         \n"
      "RCVFRL1  DS    0H                  ANALYSIS FUNCTION PROCESSING         \n"
      "         CLC   RSTAFEP,=F'0'       ANALYSIS FUNCTION ZERO?              \n"
      "         BE    RCVFRL15            YES, SKIP                            \n"
#if !defined(METTLE) && defined(_LP64)
      "         LGR   1,11                CONTEXT AS THE FIRST PARM            \n"
      "         LGR   2,9                 SDWA AS THE SECOND PARM              \n"
      "         LLGT  3,RSTAFUD           USER DATA AS THE THIRD PARM          \n"
      "         LG    4,RSTRGPR+32        STACK                                \n"
      "         LG    12,RSTRGPR+96       CAA                                  \n"
      "         LLGT  6,RSTAFEP           ANALYSIS FUNCTION ENTRY POINT        \n"
      "         LG    5,RSTAFEV           ANALYSIS FUNCTION ENVIRONMENT        \n"
      "         BASR  7,6                 CALL ANALYSIS FUNCTION               \n"
      "         NOPR  0                                                        \n"
#else
#ifdef _LP64
      "         STG   11,RSTUFPB          SAVE CONTEXT AS THE FIRST PARM       \n"
      "         STG   9,RSTUFPB+8         SAVE SDWA AS THE SECOND              \n"
      "         LLGT  1,RSTAFUD           ADDRESS OF USER PARMS                \n"
      "         STG   1,RSTUFPB+16        SAVE USER PARMS                      \n"
#else
      "         ST    11,RSTUFPB          SAVE CONTEXT AS THE FIRST PARM       \n"
      "         ST    9,RSTUFPB+4         SAVE SDWA AS THE SECOND              \n"
      "         L     1,RSTAFUD           ADDRESS OF USER PARMS                \n"
      "         ST    1,RSTUFPB+8         SAVE USER PARMS                      \n"
#endif
      "         LG    12,RSTRGPR+96       CAA OR OTHER ENVIRONMENT             \n"
      "         LG    13,RSTRGPR+104      STACK                                \n"
      "         LA    1,RSTUFPB           PARM AREA ADDRESS                    \n"
      "         LLGT  15,RSTAFEP          ANALYSIS FUNCTION ADDRESS            \n"
      "         BASR  14,15               CALL ANALYSIS FUNCTION               \n"
#endif
      "RCVFRL15 DS    0H                  RECORD SERVICE INFO                  \n"
      "         LA    1,RCVXINF           LOAD ROUTER SERVICE INFO             \n"
      "         BRAS  14,RCVSIFLB         GO RECORD                            \n"
      "         LA    1,RSTRINF           LOAD STATE SERVICE INFO              \n"
      "         BRAS  14,RCVSIFLB         GO RECORD                            \n"
      "         OI    RSTSTATE,R@STIREC   SERVICE INFO RECORDED                \n"
      "RCVFRL2  DS    0H                                                       \n"
      "         TM    RSTFLG1,R@F1SDMP    DUMP FLAG SET?                       \n"
      "         BZ    RCVFRL3             NO, SKIP                             \n"
      "         LA    5,RSTDMTLT          DUMP TITLE                           \n"
      "         TM    RCXFLAG1,R@CF1PCC+R@CF1SRB+R@CF1LCK PC, SRB, OR LOCKED?  \n"
      "         BNZ   RCVFRL25            YES, USE A SPECIAL SDUMPX CALL       \n"
      "         SDUMPX  PLISTVER=3,HDRAD=(5),TYPE=FAILRC,"
      ",SDATA=("
      "ALLNUC,ALLPSA,COUPLE,CSA,"
      "GRSQ,IO,LPA,LSQA,NUC,PSA,"
      "RGN,SQA,SUM,SWA,TRT,XESDATA"
      ")"
      ",MF=(E,RCXSDPLS)                                                        \n"
      "         ST    15,RSTSDRC          SDUMPX RC                            \n"
      "         B     RCVFRL3                                                  \n"
      "RCVFRL25 DS    0H                                                       \n"
      "         LA    13,RCXSDPSA         SAVE AREA FOR SDUMPX                 \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         IPK   ,                   PUT CURRENT KEY IN R2                \n"
      "         SPKA  0                   KEY 0                                \n"
      "         SDUMPX  PLISTVER=3,HDRAD=(5),BRANCH=YES,TYPE=XMEME,"
      ",SDATA=("
      "ALLNUC,ALLPSA,COUPLE,CSA,"
      "GRSQ,IO,LPA,LSQA,NUC,PSA,"
      "RGN,SQA,SUM,SWA,TRT,XESDATA"
      ")"
      ",MF=(E,RCXSDPLS)                                                        \n"
      "         SPKA  0(2)                RESTORE KEY                          \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "RCVFRL3  DS    0H                  CLEAN-UP FUNCTION PROCESSING         \n"
      "         CLC   RSTCFEP,=F'0'       CLEAN-UP FUNCTION ZERO?              \n"
      "         BE    RCVFRL4             YES, SKIP                            \n"
#if !defined(METTLE) && defined(_LP64)
      "         LGR   1,11                CONTEXT AS THE FIRST PARM            \n"
      "         LGR   2,9                 SDWA AS THE SECOND PARM              \n"
      "         LLGT  3,RSTCFUD           USER DATA AS THE THIRD PARM          \n"
      "         LG    4,RSTRGPR+32        STACK                                \n"
      "         LG    12,RSTRGPR+96       CAA                                  \n"
      "         LLGT  6,RSTCFEP           CLEAN-UP FUNCTION ENTRY POINT        \n"
      "         LG    5,RSTCFEV           CLEAN-UP FUNCTION ENVIRONMENT        \n"
      "         BASR  7,6                 CALL CLEAN-UP FUNCTION               \n"
      "         NOPR  0                                                        \n"
#else
#ifdef _LP64
      "         STG   11,RSTUFPB          SAVE CONTEXT AS THE FIRST PARM       \n"
      "         STG   9,RSTUFPB+8         SAVE SDWA AS THE SECOND              \n"
      "         LLGT  1,RSTCFUD           ADDRESS OF USER PARMS                \n"
      "         STG   1,RSTUFPB+16        SAVE USER PARMS                      \n"
#else
      "         ST    11,RSTUFPB          SAVE CONTEXT AS THE FIRST PARM       \n"
      "         ST    9,RSTUFPB+4         SAVE SDWA AS THE SECOND              \n"
      "         L     1,RSTCFUD           ADDRESS OF USER PARMS                \n"
      "         ST    1,RSTUFPB+8         SAVE USER PARMS                      \n"
#endif
      "         LG    12,RSTRGPR+96       CAA OR OTHER ENVIRONMENT             \n"
      "         LG    13,RSTRGPR+104      STACK                                \n"
      "         LA    1,RSTUFPB           PARM AREA ADDRESS                    \n"
      "         LLGT  15,RSTCFEP          CLEAN-UP FUNCTION ADDRESS            \n"
      "         BASR  14,15               CALL CLEAN-UP FUNCTION               \n"
#endif
      "RCVFRL4  DS    0H                  RETRY PROCESSING                     \n"
      "         TM    RSTFLG1,R@F1RTRY    ARE WE RETRYING?                     \n"
      "         BZ    RCVFRL5             NO, REMOVE AND LEAVE                 \n"
      "         LA    3,1                 STACKED STATE 1                      \n"
      "         ESTA  2,3                 GET STATE                            \n"
      "         SRL   2,16                MOVE KEY TO BIT 24-27                \n"
      "         SPKA  0(2)                GO TO ORIGINAL KEY (PKM APPROVED)    \n"
      "         LLGT  4,SDWAXPAD          LOAD EXTENSION POINTERS ADDRESS      \n"
      "         USING SDWAPTRS,4                                               \n"
      "         LLGT  5,SDWASRVP          LOAD RECORDABLE EXTENSION            \n"
      "         USING SDWARC1,5                                                \n"
      "         LLGT  4,SDWAXEME          LOAD 64-BIT EXTENSION                \n"
      "         LTR   4,4                 IS IT THERE?                         \n"
      "         BZ    RCVRET              NO, LEAVE                            \n"
      "         USING SDWARC4,4                                                \n"
      "         MVC   SDWAG64,RSTRGPR     MOVE OUR REGISTERS                   \n"
      "         MVC   SDWALSLV,RSTLSTKN   MOVE LINKAGE STACK TOKEN             \n"
      "         LLGT  7,RSTRTRAD          LOAD RETRY ADDRESS                   \n"
      "         LGR   1,9                 RESTORE SDWA IN R1                   \n"
      "         TM    RSTFLG1,R@F1LREC    RECORD SDWA TO LOGREC?               \n"
      "         BZ    RCVFRL48            NO, USE SETRP WITHOUT RECORD=YES     \n"
      "RCVFRL47 DS    0H                  SETRP WITH LOGREC                    \n"
      "         SETRP RC=4,RECORD=YES"
      ",DUMP=NO,RETREGS=64,RETRY15=YES,FRESDWA=YES"
      ",RETADDR=(7)                                                            \n"
      "         B     RCVRET              LEAVE                                \n"
      "RCVFRL48 DS    0H                  SETRP WITHOUT LOGREC                 \n"
      "         SETRP RC=4,RECORD=NO"
      ",DUMP=NO,RETREGS=64,RETRY15=YES,FRESDWA=YES"
      ",RETADDR=(7)                                                            \n"
      "         B     RCVRET              LEAVE                                \n"
      "RCVFRL5  DS    0H                  ENTRY REMOVAL                        \n"
      "         TM    RSTSTATE,R@STIREC   SERVICE INFO RECORDED?               \n"
      "         BNZ   RCVFRL55            YES, SKIP RECORDING                  \n"
      "         LA    1,RSTRINF           COLLECT SERVICE INFO BEFORE REMOVAL  \n"
      "         BRAS  14,RCVSIFLB         GO RECORD                            \n"
      "RCVFRL55 DS    0H                                                       \n"
      "         MVC   RCXRSC,RSTNEXT      REMOVE ENTRY FROM CHAIN              \n"
      "         TM    RSTFLG1,R@F1CELL    CELL POOL BASED?                     \n"
      "         BZ    RCVFRL57            NO, DO STORAGE RELEASE               \n"
      /* CPOOL FREE */
      "         LA    13,RCXSDPSA         SAVE AREA FOR CPOOL                  \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         CPOOL FREE,CPID=RCXSCPID,CELL=(8),REGS=SAVE                    \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         B     RCVFRL0             GO LOOK FOR NEXT ENTRY               \n"
      /* STORAGE RELEASE */
      "RCVFRL57 DS    0H                                                       \n"
      "         LA    4,RSTLEN            LENGTH OF RECOVERY STATE ENTRY       \n"
      "         STORAGE RELEASE,LENGTH=(4),ADDR=(8),SP=132,CALLRKY=YES         \n"
      "         B     RCVFRL0             GO LOOK FOR NEXT ENTRY               \n"
      /* leave */
      "RCVRET   DS    0H                                                       \n"
      "         EREGG 0,1                                                      \n"
      "         PR                                                             \n"
      /*
       * Service info subroutine.
       *
       * Register usage:
       * R1       - service info data structure
       * R2       - caller's key
       * R4-R5    - work registers
       * R10      - routine addressability
       * R9       - SDWA
       * R14      - return address
       *
       *  */
      "RCVSIFLB DS    0H                  PROCESS SERVICE INFORMATION          \n"
      "         USING RCVSINF,1                                                \n"
      "         IPK   ,                   STORE OUR KEY IN R2                  \n"
      "         LA    5,1                 STACKED STATE 1                      \n"
      "         ESTA  4,5                 GET STATE                            \n"
      "         SRL   4,16                MOVE KEY TO BIT 24-27                \n"
      "         SPKA  0(4)                GO TO SDWA KEY                       \n"
      "RCVSIF01 DS    0H                  RECORD LOAD MODULE NAME FIELD        \n"
      "         CLC   SDWAMODN,=XL8'00'                                        \n"
      "         BNE   RCVSIF02                                                 \n"
      "         CLC   RSIMODN,=XL8'00'                                         \n"
      "         BE    RCVSIF02                                                 \n"
      "         MVC   SDWAMODN,RSIMODN                                         \n"
      "RCVSIF02 DS    0H                  RECORD CSECT NAME FIELD              \n"
      "         CLC   SDWACSCT,=XL8'00'                                        \n"
      "         BNE   RCVSIF03                                                 \n"
      "         CLC   RSICSCT,=XL8'00'                                         \n"
      "         BE    RCVSIF03                                                 \n"
      "         MVC   SDWACSCT,RSICSCT                                         \n"
      "RCVSIF03 DS    0H                  RECORD RECOVERY ROUTINE NAME FIELD   \n"
      "         CLC   SDWAREXN,=XL8'00'                                        \n"
      "         BNE   RCVSIF04                                                 \n"
      "         CLC   RSIREXN,=XL8'00'                                         \n"
      "         BE    RCVSIF04                                                 \n"
      "         MVC   SDWAREXN,RSIREXN                                         \n"
      "RCVSIF04 DS    0H                  RECORD VARIABLE LENGTH USER DATA     \n"
      "         CLC   RSIFNML,=XL2'00'    FUNCTION NAME LENGTH ZERO?           \n"
      "         BE    RCVSIF05            YES, SKIP WRITING VRA                \n"
      "         VRADATA VRAINIT=SDWAVRA,KEY=VRALBL,LENADDR=RSIFNML,DATA=RSIFNAM"
      ",SDWAREG=9,VRAREG=(5,NOSET),TYPE=(LEN,TEST)                             \n"
      "RCVSIF05 DS    0H                                                       \n"
      "         LLGT  5,SDWAXPAD          LOAD EXTENSION POINTERS ADDRESS      \n"
      "         USING SDWAPTRS,5                                               \n"
      "         LLGT  5,SDWASRVP          LOAD RECORDABLE EXTENSION            \n"
      "         USING SDWARC1,5                                                \n"
      "RCVSIF06 DS    0H                  RECORD COMPONENT ID FIELD            \n"
      "         CLC   SDWACID,=XL5'00'                                         \n"
      "         BNE   RCVSIF07                                                 \n"
      "         CLC   RSICID,=XL5'00'                                          \n"
      "         BE    RCVSIF07                                                 \n"
      "         MVC   SDWACID,RSICID                                           \n"
      "RCVSIF07 DS    0H                  RECORD SUBCOMPONENT NAME FIELD       \n"
      "         CLC   SDWASC,=XL23'00'                                         \n"
      "         BNE   RCVSIF08                                                 \n"
      "         CLC   RSISC,=XL23'00'                                          \n"
      "         BE    RCVSIF08                                                 \n"
      "         MVC   SDWASC,RSISC                                             \n"
      "RCVSIF08 DS    0H                  RECORD ASSEMBLY DATE FIELD           \n"
      "         CLC   SDWAMDAT,=XL8'00'                                        \n"
      "         BNE   RCVSIF09                                                 \n"
      "         CLC   RSIMDAT,=XL8'00'                                         \n"
      "         BE    RCVSIF09                                                 \n"
      "         MVC   SDWAMDAT,RSIMDAT                                         \n"
      "RCVSIF09 DS    0H                  RECORD VERSION FIELD                 \n"
      "         CLC   SDWAMVRS,=XL8'00'                                        \n"
      "         BNE   RCVSIF10                                                 \n"
      "         CLC   RSIMVRS,=XL8'00'                                         \n"
      "         BE    RCVSIF10                                                 \n"
      "         MVC   SDWAMVRS,RSIMVRS                                         \n"
      "RCVSIF10 DS    0H                  RECORD COMP ID BASE NUMBER FIELD     \n"
      "         CLC   SDWACIDB,=XL4'00'                                        \n"
      "         BNE   RCVSIF11                                                 \n"
      "         CLC   RSICIDB,=XL4'00'                                         \n"
      "         BE    RCVSIF11                                                 \n"
      "         MVC   SDWACIDB,RSICIDB                                         \n"
      "         DROP  5                                                        \n"
      "RCVSIF11 DS    0H                                                       \n"
      "         SPKA  0(2)                RESTORE CALLER'S KEY                 \n"
      "         BR    14                  RETURN TO THE CALLER                 \n"
      "         DROP  1                                                        \n"
      /* non executable code */
      "         LTORG                                                          \n"
      "         POP   USING                                                    \n"

      /*** EXIT ROUTINE END ***/

      "RCVTRTN  NR    10,10                                                    \n"
      : "=m"(address)
      :
      : "r10"
  );


  return address;

}

#define recoveryDESCTs RCVDSECT
void recoveryDESCTs(){

  __asm(
      "         DS    0H                                                       \n"
      "RCVSINF  DSECT ,                                                        \n"
      "RSIMODN  DS    CL8                                                      \n"
      "RSICSCT  DS    CL8                                                      \n"
      "RSIREXN  DS    CL8                                                      \n"
      "RSICID   DS    CL5                                                      \n"
      "RSISC    DS    CL23                                                     \n"
      "RSIMDAT  DS    CL8                                                      \n"
      "RSIMVRS  DS    CL8                                                      \n"
      "RSICIDB  DS    CL4                                                      \n"
      "RSIFNML  DS    CL2                                                      \n"
      "RSIFNAM  DS    CL255                                                    \n"
      "         DS    CL7                                                      \n"
      "RCVSINFL EQU   *-RCVSINF                                                \n"
      "         EJECT ,                                                        \n"

      "         DS    0H                                                       \n"
      "RCVCTX   DSECT ,                                                        \n"
      "RCXEYECT DS    CL8                                                      \n"
      "RCXFLAG1 DS    X                                                        \n"
      "R@CF1NIN EQU   X'01'                                                    \n"
      "R@CF1PCC EQU   X'02'                                                    \n"
      "R@CF1TRM EQU   X'04'                                                    \n"
      "R@CF1UCX EQU   X'08'                                                    \n"
      "R@CF1USP EQU   X'10'                                                    \n"
      "R@CF1SRB EQU   X'20'                                                    \n"
      "R@CF1LCK EQU   X'40'                                                    \n"
      "R@CF1FRR EQU   X'80'                                                    \n"
      "RCXFLAG2 DS    X                                                        \n"
      "RCXFLAG3 DS    X                                                        \n"
      "RCXFLAG4 DS    X                                                        \n"
      "RCXPRTK  DS    F                                                        \n"
      "RCXPRKEY DS    X                                                        \n"
      "RCXVER   DS    X                                                        \n"
      "RCXPRSV1 DS    2X                                                       \n"
      "RCXSCPID DS    F                                                        \n"
      "RCXRSC   DS    A                                                        \n"
      "RCXCAA   DS    A                                                        \n"
      "RCVXINF  DS    CL(RCVSINFL)                                             \n"
      "RCXSDPLS DS    CL200                                                    \n"
      "RCXPRSV2 DS    56X                                                      \n"
      "RCXSDPSA DS    CL72                                                     \n"
      "RCXLEN   EQU   *-RCVCTX                                                 \n"
      "         EJECT ,                                                        \n"

      "         DS    0H                                                       \n"
      "RCVSTATE DSECT ,                                                        \n"
      "RSTEYECT DS    CL8                                                      \n"
      "RSTNEXT  DS    A                                                        \n"
      "RSTRTRAD DS    A                                                        \n"
      "RSTAFEP  DS    A                                                        \n"
      "RSTAFUD  DS    A                                                        \n"
      "RSTCFEP  DS    A                                                        \n"
      "RSTCFUD  DS    A                                                        \n"
      "RSTAFEV  DS    D                                                        \n"
      "RSTCFEV  DS    D                                                        \n"
      "RSTFLG1  DS    X                                                        \n"
      "R@F1SDMP EQU   X'01'                                                    \n"
      "R@F1RTRY EQU   X'02'                                                    \n"
      "R@F1DORT EQU   X'04'                                                    \n"
      "R@F1LREC EQU   X'08'                                                    \n"
      "R@F1LDSB EQU   X'10'                                                    \n"
      "R@F1CELL EQU   X'20'                                                    \n"
      "RSTFLG2  DS    X                                                        \n"
      "RSTFLG3  DS    X                                                        \n"
      "RSTFLG4  DS    X                                                        \n"
      "RSTSTATE DS    X                                                        \n"
      "R@STENBL EQU   X'01'                                                    \n"
      "R@STABND EQU   X'02'                                                    \n"
      "R@STIREC EQU   X'04'                                                    \n"
      "RSTLSTKN DS    2X                                                       \n"
      "RSTKEY   DS    1X                                                       \n"
      "RSTVER   DS    1X                                                       \n"
      "RSTRESRV DS    3X                                                       \n"
      "RSTSDRC  DS    F                                                        \n"
      "RSTRGPR  DS    16D                                                      \n"
      "RSTCGPR  DS    16D                                                      \n"
      "RSTSTFR  DS    CL256                                                    \n"
      "RSTUFPB  DS    CL32                                                     \n"
      "RSTRINF  DS    CL(RCVSINFL)                                             \n"
      "RSTDMTLT DS    CL101                                                    \n"
      "RSTLEN   EQU   *-RCVSTATE                                               \n"
      "         EJECT ,                                                        \n"

      "         DS    0H                                                       \n"
      "         IHASDWA                                                        \n"
      "         EJECT ,                                                        \n"
      "         IHAFRRS                                                        \n"
      "         EJECT ,                                                        \n"
      "         IHAPSA                                                         \n"
      "         EJECT ,                                                        \n"
#ifndef METTLE
      "         DS    0H                                                       \n"
      "         CEECAA                                                         \n"
      "         EJECT ,                                                        \n"
#endif

      "         CVT   DSECT=YES,LIST=NO                                        \n"
      "         EJECT ,                                                        \n"

      /* TODO figure out why CSECT makes the compiler skip some __asm blocks when building for LE */
#ifdef METTLE
      "         CSECT ,                                                        \n"
      "         RMODE ANY                                                      \n"
#endif
  );

}

#endif

RecoveryContext *getRecoveryContext() {
#ifdef CUSTOM_CONTEXT_GETTER
  return rcvrgcxt();
#else
#ifdef __ZOWE_OS_ZOS
  CAA *caa = (CAA *)getCAA();
  if (caa == NULL) {
    return NULL;
  }
  RLETask *rleTask = (RLETask *)caa->rleTask;
  if (rleTask == NULL) {
    return NULL;
  }
  return rleTask->recoveryContext;
#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)
  return theRecoveryContext;
#endif /* __ZOWE_OS_ZOS */
#endif /* CUSTOM_CONTEXT_GETTER */
}

static RecoveryStateEntry *getLatestState() {
  RecoveryContext *context = getRecoveryContext();
  return context ? context->recoveryStateChain : NULL;
}

static int setRecoveryContext(RecoveryContext *context) {

#ifdef CUSTOM_CONTEXT_GETTER
  return rcvrscxt(context);
#else
#ifdef __ZOWE_OS_ZOS
  CAA *caa = (CAA *)getCAA();
  if (caa == NULL) {
    return RC_RCV_ERROR;
  }
  RLETask *rleTask = (RLETask *)caa->rleTask;
  if (rleTask == NULL) {
    return RC_RCV_ERROR;
  }
  rleTask->recoveryContext = context;
  return RC_RCV_OK;
#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)
  theRecoveryContext = context;
  return RC_RCV_OK;
#endif /* __ZOWE_OS_ZOS */
#endif /* CUSTOM_CONTEXT_GETTER */

}

#ifdef __ZOWE_OS_ZOS

ZOWE_PRAGMA_PACK

typedef union StackedState_tag {

  struct {
    unsigned short pkm;
    unsigned short sasn;
    unsigned short eax;
    unsigned short pasn;
  } state00;

  struct {
    uint64 psw;
  } state01;

  struct {
    uint64 branchStateEntry;
  } state02;

  struct {
    unsigned int pcNumber;
    unsigned int modifiableArea;
  } state03;

  struct {
    char psw[16];
  } state04;

} StackedState;

ZOWE_PRAGMA_PACK_RESET

typedef enum StackedStatedExtractionCode_tag {
  STACKED_STATE_EXTRACTION_CODE_00 = 0,
  STACKED_STATE_EXTRACTION_CODE_01 = 1,
  STACKED_STATE_EXTRACTION_CODE_02 = 2,
  STACKED_STATE_EXTRACTION_CODE_03 = 3,
  STACKED_STATE_EXTRACTION_CODE_04 = 4,
} StackedStateExtractionCode;

/* this function can be used to extract PKM, ASID information, PSW key in non APF-authorized applications */
static StackedState getStackedState(StackedStateExtractionCode code) {

  StackedState state;

  __asm(
      ASM_PREFIX
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "GSSGN    DS    0H                                                       \n"
      "         LARL  10,GSSGN                                                 \n"
      "         USING GSSGN,10                                                 \n"
      "         LARL  14,GSSEXT                                                \n"
#ifdef _LP64
      "         OILL  14,X'0001'                                               \n"
#else
      "         OILH  14,X'8000'                                               \n"
#endif
      "         BAKR  14,0                                                     \n"
      "         LR    3,6                                                      \n"
      "         ESTA  2,3                                                      \n"
      "         CLFI  6,X'00000004'                                            \n"
      "         BE    GSS04                                                    \n"
      "         ST    2,0(,5)                                                  \n"
      "         ST    3,4(,5)                                                  \n"
      "         B     GSSPR                                                    \n"
      "GSS04    DS    0H                                                       \n"
      "         STG   2,0(,5)                                                  \n"
      "         STG   3,8(,5)                                                  \n"
      "GSSPR    DS    0H                                                       \n"
      "         PR                                                             \n"
      "GSSEXT   DS    0H                                                       \n"
      "         POP   USING                                                    \n"
      :
      : "NR:r5"(&state), "NR:r6"(code)
      : "r5", "r6", "r10", "r14"
  );

  return state;
}

typedef struct RecoveryStatePool_tag {

#define RCVR_STATE_POOL_EYECATCHER    "RCVSPOOL"
#define RCVR_STATE_POOL_VERSION       1

  char eyecatcher[8];
  uint8_t version;
  char reserved0[3];

  CPID cellPool;

} RecoveryStatePool;

#ifdef RCVR_CPOOL_STATES

static CPID makeRecoveryStatePool(unsigned int primaryCellCount,
                                  unsigned int secondaryCellCount,
                                  int storageSubpool) {

  StackedState stackedState = getStackedState(STACKED_STATE_EXTRACTION_CODE_01);
  uint8_t pswKey = (stackedState.state01.psw & 0x00F0000000000000LLU) >> 52;

  unsigned alignedCellSize =
      cellpoolGetDWordAlignedSize(sizeof(RecoveryStateEntry));
  CPID poolID = cellpoolBuild(primaryCellCount,
                              secondaryCellCount,
                              alignedCellSize,
                              storageSubpool, pswKey,
                              &(CPHeader){"ZWESRECOVERYSTATEPOOL   "});

  return poolID;
}

static void removeRecoveryStatePool(CPID pool) {
  cellpoolDelete(pool);
}

#endif /* RCVR_CPOOL_STATES */

static int establishRouterInternal(RecoveryContext *userContext,
                                   RecoveryStatePool *userStatePool,
                                   int flags) {

  /* Which DU are we? */
  bool isSRB = isCallerSRB();
  if (isSRB) {
    flags |= RCVR_ROUTER_FLAG_SRB;
  }

  /* Are we holding a lock? */
  bool lockHeld = isCallerLocked();
  if (lockHeld) {
    /* Return an error code if user storage has not been provided or
     * compiled without cell pool support  */
#ifdef RCVR_CPOOL_STATES
    if (userContext == NULL || userStatePool == NULL) {
      return RC_RCV_LOCKED_ENV;
    }
#else
    return RC_RCV_LOCKED_ENV;
#endif
    flags |= RCVR_ROUTER_FLAG_LOCKED;
  }

  bool frrRequired = isSRB || lockHeld;

  /* set dummy ESPIE */
  bool isESPIERequired = false;
  if (!(flags & RCVR_ROUTER_FLAG_PC_CAPABLE)) {
    if (!isSRB && !lockHeld) {
      isESPIERequired = true;
    }
  }

  int previousESPIEToken = 0;
  if (isESPIERequired) {
    previousESPIEToken = setDummyESPIE();
  }

  int rc = RC_RCV_OK;

  /* init recovery block */
  RecoveryContext *context = NULL;
  if (userContext == NULL) {
    context = (RecoveryContext *)storageObtain(sizeof(RecoveryContext));
    if (context == NULL) {
      rc = RC_RCV_ALLOC_FAILED;
      goto failure;
    }
  } else {
    flags |= RCVR_ROUTER_FLAG_USER_CONTEXT;
    context = userContext;
  }

  int setRC = setRecoveryContext(context);
  if (setRC != RC_RCV_OK) {
    rc = RC_RCV_CONTEXT_NOT_SET;
    goto failure;
  }

  memset(context, 0, sizeof(RecoveryContext));
  memcpy(context->eyecatcher, "RSRCVCTX", sizeof(context->eyecatcher));
  context->structVersion = RCVR_CONTEXT_VERSION;
  context->previousESPIEToken = previousESPIEToken;

#ifdef RCVR_CPOOL_STATES
  if (userStatePool != NULL) {
    context->stateCellPool = userStatePool->cellPool;
    flags |= RCVR_ROUTER_FLAG_USER_STATE_POOL;
  } else {
    context->stateCellPool =
        makeRecoveryStatePool(RCVR_STATE_POOL_PRIMARY_COUNT, 0,
                              RCVR_STATE_POOL_SUBPOOL);
    if (context->stateCellPool == CPID_NULL) {
      rc = RC_RCV_ALLOC_FAILED;
      goto failure;
    }
  }
#endif /* RCVR_CPOOL_STATES */

  StackedState stackedState = getStackedState(STACKED_STATE_EXTRACTION_CODE_01);
  context->routerPSWKey = (stackedState.state01.psw & 0x00F0000000000000LLU) >> 48;

  __asm(
      "         ST    12,%0                                                    \n"
      : "=m"(context->caa)
      :
      :
  );


  if (!frrRequired) {

    /* ESTAEX */
    char estaexFlags = 0;
    if (flags & RCVR_ROUTER_FLAG_NON_INTERRUPTIBLE) {
      estaexFlags |= RCVR_ESTAEX_FLAG_NON_INTERRUPTIBLE;
    }
    if (flags & RCVR_ROUTER_FLAG_RUN_ON_TERM) {
      estaexFlags |= RCVR_ESTAEX_FLAG_RUN_ON_TERM;
    }

    ESTAEXFeedback feedback = setESTAEX(getRecoveryRouterAddress(), context,
                                        estaexFlags);
    if (feedback.returnCode != 0) {

      /* Check ESTAEX RC. If it is 0x30, it means there's already an FRR set,
       * so we must use an FRR instead of an ESTAEX.*/
      if (feedback.returnCode == RCVR_ESTAEX_FRR_EXISTS) {
        frrRequired = true;
      } else {
#if RECOVERY_TRACING
<<<<<<< HEAD
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "error: set ESTAEX RC = %d, RSN = %d\n",
             feedback.returnCode, feedback.reasonCode);
=======
        printf("error: set ESTAEX RC = %d, RSN = %d\n",
               feedback.returnCode, feedback.reasonCode);
>>>>>>> 4cfcc5e... Use cell pools in recovery to support locked callers.
#endif
        rc = RC_RCV_SET_ESTAEX_FAILED;
        goto failure;
      }

    }

  }

  if (frrRequired) {

    /* FRR */
    setFRR(getRecoveryRouterAddress(), context,
           flags & RCVR_ROUTER_FLAG_NON_INTERRUPTIBLE);
    flags |= RCVR_ROUTER_FLAG_FRR;

  }

  context->flags = flags;

  return RC_RCV_OK;

  failure:
  {

    if (context != NULL) {

#ifdef RCVR_CPOOL_STATES
      if (userStatePool == NULL &&
          context->stateCellPool != CPID_NULL) {
        removeRecoveryStatePool(context->stateCellPool);
        context->stateCellPool = CPID_NULL;
      }
#endif

      if (userContext == NULL) {
        storageRelease((char *)context, sizeof(RecoveryContext));
        context = NULL;
      }

    }

    if (previousESPIEToken != 0) {
      resetESPIE(previousESPIEToken);
      previousESPIEToken = 0;
    }

  }

  return rc;
}

int recoveryEstablishRouter(int flags) {
  return establishRouterInternal(NULL, NULL, flags);
}

#ifdef RCVR_CPOOL_STATES

RecoveryStatePool *recoveryMakeStatePool(unsigned int stateCount) {


  RecoveryStatePool *pool = storageObtain(sizeof(RecoveryStatePool));
  if (pool == NULL) {
    return NULL;
  }

  memset(pool, 0, sizeof(*pool));
  memcpy(pool->eyecatcher, RCVR_STATE_POOL_EYECATCHER,
         sizeof(pool->eyecatcher));
  pool->version = RCVR_STATE_POOL_VERSION;

  pool->cellPool = makeRecoveryStatePool(stateCount, 0, RCVR_STATE_POOL_SUBPOOL);
  if (pool->cellPool == CPID_NULL) {
    storageRelease(pool, sizeof(RecoveryStatePool));
    pool = NULL;
  }

  return pool;
}

void recoveryRemoveStatePool(RecoveryStatePool *statePool) {
  removeRecoveryStatePool(statePool->cellPool);
  storageRelease(statePool, sizeof(RecoveryStatePool));
}

int recoveryEstablishRouter2(RecoveryContext *userContext,
                             RecoveryStatePool *userStatePool,
                             int flags) {
  return establishRouterInternal(userContext, userStatePool, flags);
}

#endif /* RCVR_CPOOL_STATES */


#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

static void generateCoreDump() {
  if(!fork()) {
    abort();
  }
}

static void handleSignals(int signal, siginfo_t *info, void *ucontext) {

  RecoveryContext *context = getRecoveryContext();

  if (context == NULL) {
    abort();
  }
  if (memcmp(theRecoveryContext->eyecatcher, "RSRCVCTX", sizeof(theRecoveryContext->eyecatcher))) {
    abort();
  }

  RecoveryStateEntry *currentState = getLatestState();
  while (currentState != NULL) {

    RecoveryStateEntry *nextState = currentState->next;

    if (!(currentState->state & RECOVERY_STATE_ENABLED)) {
      safeFree((char *)currentState, sizeof(RecoveryStateEntry));
      currentState = nextState;
      context->recoveryStateChain = nextState;
      continue;
    }

    currentState->state &= ~RECOVERY_STATE_ENABLED;
    currentState->state |= RECOVERY_STATE_ABENDED;

    if (currentState->analysisFunction != NULL) {
      currentState->analysisFunction(context, signal, info, currentState->analysisFunctionUserData);
    }

    if (currentState->flags & RCVR_FLAG_PRODUCE_DUMP) {
      generateCoreDump();
    }

    if (currentState->cleanupFunction != NULL) {
      currentState->cleanupFunction(context, signal, info, currentState->cleanupFunctionUserData);
    }

    if (currentState->flags & RCVR_FLAG_RETRY) {
      siglongjmp(currentState->environment, RECOVERY_STATE_ABENDED);
    }
    else {
      safeFree((char *)currentState, sizeof(RecoveryStateEntry));
      currentState = NULL;
      context->recoveryStateChain = nextState;
    }

    currentState = nextState;
  }

  abort();
}

int recoveryEstablishRouter(int flags) {

  RecoveryContext *context = (RecoveryContext *)safeMalloc(sizeof(RecoveryContext), "RecoveryContext");
  int setRC = setRecoveryContext(context);
  if (setRC != RC_RCV_OK) {
    safeFree((char *)context, sizeof(RecoveryContext));
    context = NULL;
    return RC_RCV_CONTEXT_NOT_SET;
  }

  int status = RC_RCV_OK;
  int sigactionErrno = 0;
  memset(context, 0, sizeof(RecoveryContext));
  memcpy(context->eyecatcher, "RSRCVCTX", sizeof(context->eyecatcher));
  context->flags = flags;

  pthread_mutex_lock(&handlerMutex);
  {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = handleSignals;
    action.sa_flags |= SA_SIGINFO;

    if (contextCounter == 0) {
      int sigactionRC = 0;
      sigactionRC = sigaction(SIGILL, &action, &prevSIGILLAction);
      if (sigactionRC) {
        sigactionErrno = errno;
        goto sigillFailed;
      }
      sigactionRC = sigaction(SIGFPE, &action, &prevSIGFPEAction);
      if (sigactionRC) {
        sigactionErrno = errno;
        goto sigfpeFailed;
      }
      sigactionRC = sigaction(SIGBUS, &action, &prevSIGBUSAction);
      if (sigactionRC) {
        sigactionErrno = errno;
        goto sigbusFailed;
      }
      sigactionRC = sigaction(SIGSEGV, &action, &prevSIGSEGVAction);
      if (sigactionRC) {
        sigactionErrno = errno;
        goto sigsegvFailed;
      }

      goto sigactionSuccess;

      /* release resources depending on where we failed */
      sigsegvFailed:
      sigaction(SIGBUS, &prevSIGBUSAction, NULL);

      sigbusFailed:
      sigaction(SIGFPE, &prevSIGFPEAction, NULL);

      sigfpeFailed:
      sigaction(SIGILL, &prevSIGILLAction, NULL);

      sigillFailed:
      safeFree((char *)context, sizeof(RecoveryContext));
      context = NULL;
      setRecoveryContext(NULL);

      status = RC_RCV_SIGACTION_FAILED;

      sigactionSuccess: {}
    }

    if (status == RC_RCV_OK) {
      contextCounter++;
    }
  }
  pthread_mutex_unlock(&handlerMutex);

  if (status == RC_RCV_SIGACTION_FAILED) {
    errno = sigactionErrno;
  }

  return status;
}

#endif /* __ZOWE_OS_ZOS */

#ifdef __ZOWE_OS_ZOS

int recoveryRemoveRouter() {

  RecoveryContext *context = getRecoveryContext();
  if (context == NULL) {
    return RC_RCV_OK;
  }

  int returnCode = RC_RCV_OK;

  if (!(context->flags & RCVR_ROUTER_FLAG_FRR)) {

  ESTAEXFeedback feedback = deleteESTAEX();
  if (feedback.returnCode != 0) {
#if RECOVERY_TRACING
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "error: delete ESTAEX RC = %d, RSN = %d\n", feedback.returnCode, feedback.reasonCode);
#endif
    returnCode = RC_RCV_DEL_ESTAEX_FAILED;
  }

  } else {

    deleteFRR();

  }

  if (context->previousESPIEToken != 0) {
    resetESPIE(context->previousESPIEToken);
    context->previousESPIEToken = 0;
  }

  RecoveryStateEntry *currentEntry = context->recoveryStateChain;
  for (int i = 0; i < 32768 && currentEntry != NULL; i++) {
    RecoveryStateEntry *nextEntry = currentEntry->next;
    freeRecoveryStateEntry(context, currentEntry);
    currentEntry = nextEntry;
  }
  context->recoveryStateChain = NULL;

#ifdef RCVR_CPOOL_STATES
  if (!(context->flags & RCVR_ROUTER_FLAG_USER_STATE_POOL)) {
    removeRecoveryStatePool(context->stateCellPool);
    context->stateCellPool = CPID_NULL;
  }
#endif /* RCVR_CPOOL_STATES */

  if (!(context->flags & RCVR_ROUTER_FLAG_USER_CONTEXT)) {
    storageRelease((char *)context, sizeof(RecoveryContext));
  }

  context = NULL;
  if (setRecoveryContext(NULL) != RC_RCV_OK) {
    returnCode = (returnCode != RC_RCV_OK) ? returnCode : RC_RCV_CONTEXT_NOT_SET;
  }

  return returnCode;
}

#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

int recoveryRemoveRouter() {

  RecoveryContext *context = getRecoveryContext();
  if (context == NULL) {
    return RC_RCV_OK;
  }

  int status = RC_RCV_OK;
  int sigactionErrno = 0;

  pthread_mutex_lock(&handlerMutex);
  {
    if (contextCounter == 1) {
      int sigactionRC = 0;
      sigactionRC = sigaction(SIGILL, &prevSIGILLAction, NULL);
      if (sigactionRC) {
        sigactionErrno = sigactionErrno ? sigactionErrno : errno;
        status = status ? status : RC_RCV_SIGACTION_FAILED;
      }
      sigactionRC = sigaction(SIGFPE, &prevSIGFPEAction, NULL);
      if (sigactionRC) {
        sigactionErrno = sigactionErrno ? sigactionErrno : errno;
        status = status ? status : RC_RCV_SIGACTION_FAILED;
      }
      sigactionRC = sigaction(SIGBUS, &prevSIGBUSAction, NULL);
      if (sigactionRC) {
        sigactionErrno = sigactionErrno ? sigactionErrno : errno;
        status = status ? status : RC_RCV_SIGACTION_FAILED;
      }
      sigactionRC = sigaction(SIGSEGV, &prevSIGSEGVAction, NULL);
      if (sigactionRC) {
        sigactionErrno = sigactionErrno ? sigactionErrno : errno;
        status = status ? status : RC_RCV_SIGACTION_FAILED;
      }
    }
    contextCounter--;
  }
  pthread_mutex_unlock(&handlerMutex);

  RecoveryStateEntry *currentEntry = context->recoveryStateChain;
  for (int i = 0; i < 32768 && currentEntry != NULL; i++) {
    RecoveryStateEntry *nextEntry = currentEntry->next;
    safeFree((char *)currentEntry, sizeof(RecoveryStateEntry));
    currentEntry = nextEntry;
  }
  context->recoveryStateChain = NULL;

  safeFree((char *)context, sizeof(RecoveryContext));
  context = NULL;
  int setRC = setRecoveryContext(NULL);
  if (setRC != RC_RCV_OK) {
    status = RC_RCV_CONTEXT_NOT_SET;
  }

  if (status == RC_RCV_SIGACTION_FAILED) {
    errno = sigactionErrno;
  }

  return status;
}

#endif /* __ZOWE_OS_ZOS */

#ifdef __ZOWE_OS_ZOS

static RecoveryStateEntry *addRecoveryStateEntry(RecoveryContext *context, char *name, int flags, char *dumpTitle,
                                                 AnalysisFunction *userAnalysisFunction, void * __ptr32 analysisFunctionUserData,
                                                 CleanupFunction *userCleanupFunction, void * __ptr32 cleanupFunctionUserData) {

  RecoveryStateEntry *newEntry = allocRecoveryStateEntry(context);
  if (newEntry == NULL) {
    return NULL;
  }

#if !defined(METTLE) && defined(_LP64)
  if (userAnalysisFunction != NULL) {
    newEntry->analysisFunctionEnvironment = ((uint64 *)userAnalysisFunction)[0];
    newEntry->analysisFunction = ((void **)userAnalysisFunction)[1];
  }
  if (userCleanupFunction != NULL) {
    newEntry->cleanupFunctionEnvironment = ((uint64 *)userCleanupFunction)[0];
    newEntry->cleanupFunctionEntryPoint = ((void **)userCleanupFunction)[1];
  }
#else
  newEntry->analysisFunction = (void * __ptr32 )userAnalysisFunction;
  newEntry->cleanupFunctionEntryPoint = (void * __ptr32 )userCleanupFunction;
#endif

  newEntry->analysisFunctionUserData = analysisFunctionUserData;
  newEntry->cleanupFunctionUserData = cleanupFunctionUserData;

  if (name != NULL) {
    int stateNameLength = strlen(name);
    if (stateNameLength > sizeof(newEntry->serviceInfo.stateName)) {
      stateNameLength = sizeof(newEntry->serviceInfo.stateName);
    }
    newEntry->serviceInfo.stateNameLength = stateNameLength;
    memcpy(newEntry->serviceInfo.stateName, name, stateNameLength);
  }

  newEntry->flags |= flags;
  newEntry->state = (flags & RCVR_FLAG_DISABLE) ? RECOVERY_STATE_DISABLED : RECOVERY_STATE_ENABLED;
  memset(newEntry->dumpTitle.title, ' ', sizeof(newEntry->dumpTitle.title));
  newEntry->dumpTitle.length = 0;
  if (dumpTitle != NULL) {
    int titleLength = strlen(dumpTitle);
    titleLength = titleLength > sizeof(newEntry->dumpTitle.title) ? sizeof(newEntry->dumpTitle.title) : titleLength;
    memcpy(newEntry->dumpTitle.title, dumpTitle, titleLength);
    newEntry->dumpTitle.length = titleLength;
  }
  newEntry->next = context->recoveryStateChain;
  context->recoveryStateChain = newEntry;

  return newEntry;
}

static void removeRecoveryStateEntry(RecoveryContext *context) {

  RecoveryStateEntry *entryToRemove = context->recoveryStateChain;
  if (entryToRemove != NULL) {
    context->recoveryStateChain = entryToRemove->next;
  } else {
    return;
  }

  freeRecoveryStateEntry(context, entryToRemove);
  entryToRemove = NULL;
}

#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

RecoveryStateEntry *addRecoveryStateEntry(RecoveryContext *context, char *name, int flags, char *dumpTitle,
                                          AnalysisFunction *userAnalysisFunction, void *analysisFunctionUserData,
                                          CleanupFunction *userCleanupFunction, void *cleanupFunctionUserData) {

  RecoveryStateEntry *newEntry = (RecoveryStateEntry *)safeMalloc(sizeof(RecoveryStateEntry), "RecoveryStateEntry");
  memset(newEntry, 0, sizeof(RecoveryStateEntry));
  memcpy(newEntry->eyecatcher, "RSRSENTR", sizeof(newEntry->eyecatcher));

  newEntry->analysisFunction = userAnalysisFunction;
  newEntry->cleanupFunction= userCleanupFunction;

  newEntry->analysisFunctionUserData = analysisFunctionUserData;
  newEntry->cleanupFunctionUserData = cleanupFunctionUserData;

  /* TODO figure out what to do with the name and dump title, update corresponding function descriptions */
  memset(newEntry->dumpTitle, 0, sizeof(newEntry->dumpTitle));
  if (dumpTitle != NULL) {
    strncpy(newEntry->dumpTitle, dumpTitle, sizeof(newEntry->dumpTitle) - 1);
  }

  newEntry->flags = flags;
  newEntry->state = (flags & RCVR_FLAG_DISABLE) ? RECOVERY_STATE_DISABLED : RECOVERY_STATE_ENABLED;
  newEntry->next = context->recoveryStateChain;
  context->recoveryStateChain = newEntry;

  return newEntry;
}

static void removeRecoveryStateEntry(RecoveryContext *context) {

  RecoveryStateEntry *entryToRemove = context->recoveryStateChain;
  if (entryToRemove != NULL) {
    context->recoveryStateChain = entryToRemove->next;
  }

  safeFree((char *)entryToRemove, sizeof(RecoveryStateEntry));
  entryToRemove = NULL;
}

#endif /* __ZOWE_OS_ZOS */

#ifdef __ZOWE_OS_ZOS

static int16_t getLinkageStackToken(void) {

  int32_t rc = 0;
  int16_t token = 0;

  __asm(
      ASM_PREFIX
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         IEALSQRY                                                       \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif

      : "=NR:r0"(token), "=NR:r15"(rc)
      :
      : "r0", "r1", "r14", "r15"
  );

  if (rc != 0) {
    return -1;
  }

  return token;
}

int recoveryPush(char *name, int flags, char *dumpTitle,
                 AnalysisFunction *userAnalysisFunction, void * __ptr32 analysisFunctionUserData,
                 CleanupFunction *userCleanupFunction, void * __ptr32 cleanupFunctionUserData) {

  RecoveryContext *context = getRecoveryContext();
  if (context == NULL) {
    return RC_RCV_CONTEXT_NOT_FOUND;
  }

  int16_t linkageStackToken = getLinkageStackToken();
  if (linkageStackToken == -1) {
    return RC_RCV_LNKSTACK_ERROR;
  }

  RecoveryStateEntry *newEntry =
      addRecoveryStateEntry(context, name, flags, dumpTitle,
                            userAnalysisFunction, analysisFunctionUserData,
                            userCleanupFunction, cleanupFunctionUserData);
  if (newEntry == NULL) {
    return RC_RCV_ALLOC_FAILED;
  }

  newEntry->linkageStackToken = linkageStackToken;

  /* Extract key from the newly created stacked state. IPK cannot be used
   * as it requires the extract-authority set.
   * This value will be used to restore the current's recovery state key if
   * it changes during the recovery process - this usually happens under an SRB
   * when SETRP retry with key 0. */
  StackedState stackedState = getStackedState(STACKED_STATE_EXTRACTION_CODE_01);
  newEntry->key = (stackedState.state01.psw & 0x00F0000000000000LLU) >> 48;

  __asm(

      /*
       * Register use:
       *
       * R2       - work register (addressability, key for SPKA)
       * R9       - recovery state
       * R10,R11  - address and size of the stack frame buffer for MVCL
       * R14,R15  - address and size of the stack frame for MVCL
       *
       */

      ASM_PREFIX
      "&LX      SETA  &LX+1                                                    \n"
      "&LRETRY  SETC  'RETRY&LX'                                               \n"
      "&LRSTORE SETC  'RSTR&LX'                                                \n"
      "&LEXIT   SETC  'EXIT&LX'                                                \n"
      "&LRSEYE  SETC  'RSEC&LX'                                                \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         USING RCVSTATE,9                                               \n"
      "         LARL  10,&LRETRY         GET RETRY ADDRESS                     \n"
      "         ST    10,RSTRTRAD        STORE RETRY ADDRESS FOR SETRP         \n"
      "         LA    10,RSTSTFR         ADDRESS OF STACK FRAME BUFFER         \n"
      "         LA    11,L'RSTSTFR       SIZE OF STACK FRAME BUFFER            \n"
#if defined(_LP64) && !defined(METTLE)
      "         LA    14,2048(4)         ADDRESS OF STACK FRAME                \n"
#else
      "         LGR   14,13              ADDRESS OF STACK FRAME                \n"
#endif
      "         LGR   15,11              SIZE OF STACK FRAME                   \n"
      "         STMG  0,15,RSTRGPR       SAVE GRPs FOR RETRY                   \n"
      "         MVCL  10,14              SAVE STACK FRAME                      \n"
#if !defined(_LP64)
      "         L     10,4(13)           ADDRESS OF PREVIOUS SAVE AREA         \n"
      "         MVC   RSTCGPR(60),12(10) CALLER'S REGISTERS                    \n"
#elif defined(_LP64) && defined(METTLE)
      "         LG    10,128(13)         ADDRESS OF PREVIOUS SAVE AREA         \n"
      "         MVC   RSTCGPR(120),8(10) CALLER'S REGISTERS                    \n"
#endif
      "         J     &LEXIT             BRANCH AROUND RETRY BLOCK             \n"
      /* retry block */
      "&LRETRY  DS    0H                                                       \n"
      "         LARL  2,&LRSEYE          LOAD EYECATCHER CONSTANT ADDRESS      \n"
      "         CLC   RSTEYECT,0(2)      STATE EYECATCHER IS VALID?            \n"
      "         JE    &LRSTORE           YES, CONTINUE                         \n"
      "         ABEND 1                  SOMETHING WENT TERRIBLY WRONG         \n"
      "&LRSEYE  DC    CL8'RSRSENTR'      STATE EYECATCHER CONSTANT             \n"
      "&LRSTORE DS    0H                                                       \n"
      "         LLC   2,RSTKEY           LOAD KEY TO R2                        \n"
      "         SPKA  0(2)               RESTORE KEY                           \n"
      "         MVCL  14,10              RESTORE STACK FRAME ON RETRY          \n"
#if !defined(_LP64)
      "         L     10,4(13)           ADDRESS OF PREVIOUS SAVE AREA         \n"
      "         MVC   12(60,10),RSTCGPR  RESTORE CALLER'S REGISTERS            \n"
#elif defined(_LP64) && defined(METTLE)
      "         LG    10,128(13)         ADDRESS OF PREVIOUS SAVE AREA         \n"
      "         MVC   8(120,10),RSTCGPR  RESTORE CALLER'S REGISTERS            \n"
#endif
      /* exit */
      "&LEXIT   DS    0H                                                       \n"
      "         POP   USING                                                    \n"
#ifndef METTLE
      "* Prevent LE compiler errors caused by the way XL C treats __asm        \n"
      "         NOPR  0                                                        \n"
#endif
      :
      : "NR:r9"(newEntry)
      : "r2", "r10", "r11", "r14", "r15"
  );

  if (newEntry->state & RECOVERY_STATE_ABENDED) {
    if (newEntry->flags & RCVR_FLAG_DELETE_ON_RETRY) {
      removeRecoveryStateEntry(context);
      newEntry = NULL;
    }
    return RC_RCV_ABENDED;
  }

  return RC_RCV_OK;
}

#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

/* recoveryPush is a macro on AIX and Linux */

#endif /* __ZOWE_OS_ZOS */

void recoveryPop() {
  RecoveryContext *context = getRecoveryContext();
  if (context != NULL) {
    removeRecoveryStateEntry(context);
  }
}

#ifdef __ZOWE_OS_ZOS

void recoverySetDumpTitle(char *title) {
  RecoveryStateEntry *state = getLatestState();
  if (state != NULL && title != NULL) {
    int titleLength = strlen(title);
    titleLength = titleLength > sizeof(state->dumpTitle.title) ? sizeof(state->dumpTitle.title) : titleLength;
    memcpy(state->dumpTitle.title, title, titleLength);
    state->dumpTitle.length = titleLength;
  }
}

#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

void recoverySetDumpTitle(char *title) {
  RecoveryStateEntry *state = getLatestState();
  if (state != NULL && title != NULL) {
    strncpy(state->dumpTitle, title, sizeof(state->dumpTitle) - 1);
  }
}

#endif /* __ZOWE_OS_ZOS */

void recoverySetFlagValue(int flag, bool value) {
  RecoveryStateEntry *state = getLatestState();
  if (state != NULL) {
    if (value == TRUE) {
      state->flags = state->flags | flag;
    }
    else {
      state->flags = state->flags & ~flag;
    }
  }
}

void recoveryEnableCurrentState() {
  RecoveryStateEntry *state = getLatestState();
  if (state != NULL) {
    state->state |= RECOVERY_STATE_ENABLED;
  }
}

void recoveryDisableCurrentState() {
  RecoveryStateEntry *state = getLatestState();
  if (state != NULL) {
    state->state &= ~RECOVERY_STATE_ENABLED;
  }
}

bool recoveryIsRouterEstablished() {
  return getRecoveryContext() ? true : false;
}

#ifdef __ZOWE_OS_ZOS

static void updateServiceInfo(RecoveryServiceInfo *info,
                              char *loadModuleName, char *csect, char *recoveryRoutineName,
                              char *componentID, char *subcomponentName,
                              char *buildDate, char *version, char *componentIDBaseNumber,
                              char *stateName) {

  if (loadModuleName != NULL) {
    int loadModuleNameLength = strlen(loadModuleName);
    if (loadModuleNameLength > sizeof(info->loadModuleName)) {
      loadModuleNameLength = sizeof(info->loadModuleName);
    }
    memset(info->loadModuleName, ' ', sizeof(info->loadModuleName));
    memcpy(info->loadModuleName, loadModuleName, loadModuleNameLength);
  }

  if (csect != NULL) {
    int csectLength = strlen(csect);
    if (csectLength > sizeof(info->csect)) {
      csectLength = sizeof(info->csect);
    }
    memset(info->csect, ' ', sizeof(info->csect));
    memcpy(info->csect, csect, csectLength);
  }

  if (recoveryRoutineName != NULL) {
    int stateNameLength = strlen(recoveryRoutineName);
    if (stateNameLength > sizeof(info->recoveryRoutineName)) {
      stateNameLength = sizeof(info->recoveryRoutineName);
    }
    memset(info->recoveryRoutineName, ' ', sizeof(info->recoveryRoutineName));
    memcpy(info->recoveryRoutineName, recoveryRoutineName, stateNameLength);
  }

  if (componentID != NULL) {
    int componentIDLength = strlen(componentID);
    if (componentIDLength > sizeof(info->componentID)) {
      componentIDLength = sizeof(info->componentID);
    }
    memset(info->componentID, ' ', sizeof(info->componentID));
    memcpy(info->componentID, componentID, componentIDLength);
  }

  if (subcomponentName != NULL) {
    int subcomponentNameLength = strlen(subcomponentName);
    if (subcomponentNameLength > sizeof(info->subcomponentName)) {
      subcomponentNameLength = sizeof(info->subcomponentName);
    }
    memset(info->subcomponentName, ' ', sizeof(info->subcomponentName));
    memcpy(info->subcomponentName, subcomponentName, subcomponentNameLength);
  }

  if (buildDate != NULL) {
    int buildDateLength = strlen(buildDate);
    if (buildDateLength > sizeof(info->buildDate)) {
      buildDateLength = sizeof(info->buildDate);
    }
    memset(info->buildDate, ' ', sizeof(info->buildDate));
    memcpy(info->buildDate, buildDate, buildDateLength);
  }

  if (version != NULL) {
    int versionLength = strlen(version);
    if (versionLength > sizeof(info->version)) {
      versionLength = sizeof(info->version);
    }
    memset(info->version, ' ', sizeof(info->version));
    memcpy(info->version, version, versionLength);
  }

  if (componentIDBaseNumber != NULL) {
    int componentIDBaseNumberLength = strlen(componentIDBaseNumber);
    if (componentIDBaseNumberLength > sizeof(info->componentIDBaseNumber)) {
      componentIDBaseNumberLength = sizeof(info->componentIDBaseNumber);
    }
    memset(info->componentIDBaseNumber, ' ', sizeof(info->componentIDBaseNumber));
    memcpy(info->componentIDBaseNumber, componentIDBaseNumber, componentIDBaseNumberLength);
  }

  if (stateName != NULL) {
    int stateNameLength = strlen(stateName);
    if (stateNameLength > sizeof(info->stateName)) {
      stateNameLength = sizeof(info->stateName);
    }
    info->stateNameLength = stateNameLength;
    memcpy(info->stateName, stateName, stateNameLength);
  }

}

void recoveryUpdateRouterServiceInfo(char *loadModuleName, char *csect, char *recoveryRoutineName,
                                     char *componentID, char *subcomponentName,
                                     char *buildDate, char *version, char *componentIDBaseNumber,
                                     char *stateName) {

  RecoveryContext *context = getRecoveryContext();
  if (context == NULL) {
    return;
  }

  updateServiceInfo(&context->serviceInfo,
                    loadModuleName, csect, recoveryRoutineName,
                    componentID, subcomponentName,
                    buildDate, version, componentIDBaseNumber,
                    stateName);

}

void recoveryUpdateStateServiceInfo(char *loadModuleName, char *csect, char *recoveryRoutineName,
                                    char *componentID, char *subcomponentName,
                                    char *buildDate, char *version, char *componentIDBaseNumber,
                                    char *stateName) {

  RecoveryStateEntry *state = getLatestState();
  if (state == NULL) {
    return;
  }

  updateServiceInfo(&state->serviceInfo,
                    loadModuleName, csect, recoveryRoutineName,
                    componentID, subcomponentName,
                    buildDate, version, componentIDBaseNumber,
                    stateName);

}

void recoveryGetABENDCode(SDWA *sdwa, int *completionCode, int *reasonCode) {

  int cc = -1;
  int rsn = 0;

  if (sdwa == NULL) {
    return;
  }

  cc = (sdwa->flagsAndCode >> 12) & 0x00000FFF;
  int flag = (sdwa->flagsAndCode >> 24) & 0x000000FF;

  if (flag & 0x04) {
    char *sdwadata = (char*)sdwa;
    SDWAPTRS *sdwaptrs = (SDWAPTRS *)(sdwa->sdwaxpad);
    if (sdwaptrs != NULL) {
      char *sdwarc1 = (char *)sdwaptrs->sdwasrvp;
      if (sdwarc1 != NULL) {
        rsn = *(int * __ptr32)(sdwarc1 + 44);
      }
    }
  }

  *completionCode = cc;
  *reasonCode = rsn;

}

#endif /* __ZOWE_OS_ZOS */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

