

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
#include "qsam.h"
#else
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "zos.h"
#include "le.h"
#include "recovery.h"
#include "scheduling.h"
#include "openprims.h"
#include "logging.h"

#ifndef __ZOWE_OS_ZOS
#error non-z/OS environments are not supported
#endif

#define SCHEDULING_TRACE 0

ZOWE_PRAGMA_PACK

#define ATTACH_OPTION_NO_DISP     0x80
#define ATTACH_OPTION_JSCB_GIVEN  0x40
#define ATTACH_OPTION_GIVEJPQ_YES 0x20
#define ATTACH_OPTION_KEY_ZERO    0x10
#define ATTACH_OPTION_SZERO_NO    0x08
#define ATTACH_OPTION_SVAREA      0x04
#define ATTACH_OPTION_JSTCB       0x02
#define ATTACH_OPTION_SM_SUPV     0x01

/* Learn to use macro IDENTIFY to build Load module shim for ATTACH
 */

typedef struct AttachParms_tag{
  Addr31  ep;                           /* really a pointer */
  Addr31  dcb;                          /* for reading code from module */
  int     ecbAddress;                   /* to notify what */
  Addr31  giveSubpoolValueOrListAddress;
  Addr31  shareSubpoolValueOrListAddress;
  Addr31  etxrAddress;                  /* 0x14 is the exit routine and 31 bit pointer */
  /* OFFSET 0x18 */
  short  dispatchingPriority;
  char   limitPriority;
  char   options;            /* == 1 ?  */
  char   programName[8];
  /* OFFSET 0x24 */
  Addr31 jscbAddress;
  Addr31 staiParameterList;  /* address */
  Addr31 staiExitRoutine;
  /* OFFSET 0x30 */
  Addr31 tasklibDCB;
  char   flags1;
  char   taskID;
  short  parmListLength;
  Addr31 parmList;           /* Rob:  NSHSPV or NSHSPL parameter list.  */
  /* offset 0x3C */
  char   flags2;
  char   formatNumber;       /* 1 MVS, 2 ATTACHX */
  short  reserved3E;
  int    epDeAlet;           /* 0x40 */
  int    dcbAlet;
  int    gsplAlet;
  int    shsplAlet;
  int    jscbAlet;           /* 50 */
  int    staiAlet;
  int    tasklibAlet;
  int    nslsplAlet;
} AttachParms;

#ifdef METTLE
static Addr31 getTrampoline() {

  Addr31 trampolineAddress = NULL;

  __asm(
      ASM_PREFIX
      "         LARL  1,L$TRM                                                  \n"
      "         ST    1,%0                                                     \n"
      "         J     L$TRMEX                                                  \n"
      "L$TRM    DS    0H                                                       \n"

      "&KGX(2)  SETC   'G'               Set 'G' for index #2                  \n"
      "&KF      SETA   %1/4              Set 'G'index value                    \n"
      "&KG      SETC   '&KGX(&KF)'       Set 'G' value                         \n"
      "&KL      SETC   'L&KG'            Set Load instruction                  \n"
      "&KLR     SETC   'L&KG.R'          Set Load-register instruction         \n"
      "&KST     SETC   'ST&KG'           Set Store instruction                 \n"

      /* establish addressability */
#ifdef _LP64
      "         SAM64                     go AMODE64 in case we're in SRB      \n"
#endif
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  10,L$TRM                                                 \n"
      "         USING L$TRM,10                                                 \n"
      /* update RLE task */
      "         &KL   4,0(,1)             RLETask from first arg in R1 list    \n"
      "         USING RLETASK,4                                                \n"
      "         LA    3,7                 load status 7 to R3                  \n"
      "         ST    3,RLETSTS           update status in RLETask             \n"
      /* handle FRR if SRB */
      "         USING PSA,0                                                    \n"
      "         LT    3,PSATOLD           are we TCB?                          \n"
      "         BNZ   L$TRMSA             yes, skip FRR handling               \n"
      "         DROP  0                                                        \n"
      "         LR    5,2                 load FRR into R5                     \n"
      "* Save FRR parm area address in case we ever need to modify it later    \n"
      "         ST    5,RLETSTS           store FRR parm into status indicator \n"
      "         IPK   ,                   put current key in R2                \n"
      "         SPKA  0                   key 0                                \n"
      "         ST    4,8(,5)             put RLETask into 3rd word of FRR parm\n"
      "         SPKA  0(2)                restore key                          \n"
      /* allocate stack if needed */
      "L$TRMSA  DS    0H                                                       \n"
      "         L     3,RLETSSZ           load stack size                      \n"
      "         TM    RLETFLG1,RF1@DSPB   is a disposable task?                \n"
      "         BZ    L$TRMSI             no, stack is already allocated       \n"
      "         STORAGE OBTAIN,LENGTH=(3),BNDRY=PAGE                           \n"
      "         &KST  1,RLETSTK           store stack in RLE                   \n"
      /* init save area */
      "L$TRMSI  DS    0H                                                       \n"
      "         &KL   13,RLETSTK          load new stack into R13              \n"
      "         USING SKDSA,13                                                 \n"
#ifdef _LP64
      "         MVC   SKDSAEYE,=C'F1SA'   stack eyecatcher                     \n"
#endif
      "         LA    3,SKDSANSA          address of next save area            \n"
      "         &KST  3,SKDSANXT          save next save area address          \n"
#ifndef _LP64
      "         ST    3,SKDSANAB          save NAB for LE 31-bit               \n"
#endif
      "         MVC   SKDSASSZ,RLETSSZ    save stack size on new stack         \n"
      "         MVC   SKDSAFLG,RLETFLG    save flags on new stack              \n"
      "         &KST  4,SKDSAPRM          save RLETask for trampoline function \n"
      /* update fake CAA */
      "         L     3,RLETSSZ           load stack size                      \n"
      "         LA    3,0(3,13)           top of stack                         \n"
      "         LLGT  12,RLETFCAA         load fake CAA to R12                 \n"
      "         ST    3,X'314'(,12)       note top of stack in CEECAAESS       \n"
      /* call trampoline function */
      "         &KL   15,RLETTRM          load trampoline function to R15      \n"
      "         LA    1,SKDSAPRM          load pamlist to R1                   \n"
      "         BASR  14,15               call trampoline function             \n"
      /* release stack if needed */
      "         TM    SKDSAFL1,RF1@DSPB   is a disposable task?                \n"
      "         BZ    L$TRMRT             no, stack will be released later     \n"
      "         &KLR  2,15                load RC to R2                        \n"
      "         L     3,SKDSASSZ          load stack size                      \n"
      "         &KLR  1,13                load stack to R1                     \n"
      "         STORAGE RELEASE,LENGTH=(3),ADDR=(1)                            \n"
      "         &KLR  15,2                restore RC in R15                    \n"
      /* return to caller */
      "L$TRMRT  DS    0H                                                       \n"
      "         PR    ,                   return                               \n"
      /* non executable code */
      "         LTORG                                                          \n"
      "         POP   USING                                                    \n"

      "L$TRMEX  DS    0H                                                       \n"
      : "=m"(trampolineAddress)
      : "i"(sizeof(void *))
      : "r1"
  );

  return trampolineAddress;
}
#endif

static int executeRLETask(RLETask *task) {

  abortIfUnsupportedCAA();
  CAA *caa = (CAA *)getCAA();
  caa->rleTask = task;
  caa->loggingContext = ((CAA *)task->fakeCAA)->loggingContext;

  bool recoveryRequired =
      (task->flags & RLE_TASK_RECOVERABLE)
      && !(task->flags & (RLE_TASK_SRB_CAPABLE | RLE_TASK_SRB_START));

  if (recoveryRequired) {
    int establishRC = recoveryEstablishRouter(RCVR_ROUTER_FLAG_NONE);
    if (establishRC != RC_RCV_OK) {
      printf("error: recovery not established, rc=%d\n", establishRC);
    }
  }

  int taskRC = task->userFunction(task);

  if (recoveryRequired) {
    recoveryRemoveRouter();
  }

  if (task->flags & RLE_TASK_DISPOSABLE) {
    deleteRLETask(task);
    task = NULL;
  }

  return taskRC;
}

static int identify(Addr31 name, Addr31 code){
#if SCHEDULING_TRACE > 0
  printf("identify(2) name=0x%x code=0x%x\n",name,code);
  dumpbuffer((char*)name,8);
  printf("----\n");
  dumpbuffer((char*)code,8);
  fflush(stdout);
#endif

  int status = 0;
  __asm(" XGR 0,0 \n"
        " XGR 1,1 \n"
        " L 0,%1 \n"
        " L 1,%2 \n"
        " SVC 41 \n"
        " LR %0,15 "
        :
        "=r"(status)
        :
        "m"(name), "m"(code)
        :
        "r15");
  return status;
}

static int attach(Addr31 attachParms, Addr31 newTaskParms){
  int status = 0;
  /* attachCode */
  __asm(" XGR 15,15 \n"
        " XGR 1,1   \n"
        " L 15,%1   \n"
        " L 1,%2    \n"
        " SVC 42    \n"
        " LR %0,15 "
        :
        "=r"(status)
        :
        "m"(attachParms),"m"(newTaskParms)
        :
        "r15");
  return status;
}

#ifdef METTLE
static int startRLETaskInTCB(RLETask *ctcb, int *completionECB){

  Addr31 attachTrampolineCode = getTrampoline();
#if SCHEDULING_TRACE > 0
  printf("startCTCB.start attachTrampolineAt 0x%p\n",attachTrampolineCode);
  fflush(stdout);
#endif

  int status = identify((Addr31)"CTCBTRMP",attachTrampolineCode);
#if SCHEDULING_TRACE > 0
  printf("identify status=0x%x\n",status);
  printf("sCTCB 0\n");
  fflush(stdout);
#endif
  AttachParms *parms = (AttachParms*)safeMalloc31(sizeof(AttachParms),"AttachParms");
#if SCHEDULING_TRACE > 0
  printf("sCTCB 1\n");
  fflush(stdout);
#endif

  memset(parms,0,sizeof(AttachParms));
  /* gotten from macro expansion */
  parms->parmListLength = 72;
  if (completionECB){
    parms->ecbAddress = 0x80000000 | ((int)completionECB);
  } else{
    parms->ecbAddress = 0x80000000; /* this is stupid */
  }
  parms->flags2 = 0x80; /* new format, other bits cleared */
  parms->formatNumber = 1;

#if SCHEDULING_TRACE > 0
  printf("sCTCB 2\n");
  fflush(stdout);
#endif
  parms->ep = (Addr31)"CTCBTRMP";
  ctcb->selfParmList = (Addr31)ctcb;
#if SCHEDULING_TRACE > 0
  printf("sCTCB 3 task at 0x%p\n",ctcb);
  dumpbuffer((char*)ctcb,sizeof(RLETask));
  printf("Attach Parms at 0x%p\n",parms);
  dumpbuffer((char*)parms,sizeof(AttachParms));
  fflush(stdout);
#endif

  Addr31 newTaskParmList = (char*)&(ctcb->selfParmList);
#if SCHEDULING_TRACE > 0
  printf("newTaskParmList at 0x%p\n",newTaskParmList);
  dumpbuffer((char*)newTaskParmList,0x10);
  fflush(stdout);
#endif

  status = attach((char*)parms,newTaskParmList);
#if SCHEDULING_TRACE > 0
  printf("after attach status=0x%x\n",status);
  fflush(stdout);
#endif
  safeFree((char*)parms,sizeof(AttachParms));
  return 0;
}
#endif

#ifndef METTLE
static int startRLETaskInThread(RLETask *task, OSThread *threadData) {
  OSThread localThreadData;
  OSThread *osThread = threadData ? threadData : &localThreadData;
  int createStatus = threadCreate(osThread,(void * (*)(void *))task->trampolineFunction, task);
  if (createStatus != 0) {
// Removed __ZOWE_OS_WINDOWS code - only __ZOWE_OS_ZOS is supported
    perror("pthread_create() error");
  } else {
    task->threadData = *osThread;
    task->statusIndicator |= RLE_TASK_STATUS_THREAD_CREATED;
#if SCHEDULING_TRACE > 0
    printf("thread create succeeded!\n");
    fflush(stdout);
#endif
  }
  return createStatus;
}
#endif

// When compiled for LE, only thread are supported
// When compiled for Metal, only SRBs or TCBs are supported

int startRLETask(RLETask *task, int *completionECB){
#ifdef METTLE
  return startRLETaskInTCB(task,completionECB);
#else
  return startRLETaskInThread(task, NULL);
#endif
}


/* makeRLETask builds a C-language SRB | TCB switchable thing

   needs more parameters for target address space,etc

   makeRLETask returns a RLETas struct that wraps all the tracing and recovery gubbins that are need to manage it.
 */

#define RLE_TASK_STACK_SIZE   0x10000

RLETask *makeRLETask(RLEAnchor *anchor,
                     int taskFlags,
                     int functionPointer(RLETask *task)){
  RLETask *task = (RLETask*)safeMalloc31(sizeof(RLETask),"RTSK");
  memset(task,0,sizeof(RLETask));
  memcpy(task->eyecatcher,"RTSK",4);
  task->flags = taskFlags;
  task->anchor = anchor;
  task->stackSize = RLE_TASK_STACK_SIZE;
  if (!(taskFlags & RLE_TASK_DISPOSABLE)) {
    task->stack = safeMalloc31(task->stackSize,"RTSK Stack");
    memset(task->stack,0,task->stackSize);
  }

  task->trampolineFunction = executeRLETask;
  task->userFunction = functionPointer;
  char *fakeCAAData = safeMalloc31(sizeof(CAA),"Fake CAA");
  CAA *fakeCAA = (CAA*)fakeCAAData;
  CAA *realCAA = (CAA*)realCAA;
#if SCHEDULING_TRACE > 0
  printf("fake CAA placed at 0x%x\n",fakeCAA);
#endif
  char *realCAAData = getCAA();

  /* These are some wild, hacky guesses about what to copy.
     See le.h for offsets (and LE Vendor Book) */
  /* TODO we might want to copy more data, e.g. when printf is called, the address at R12+0x504 is used
   * this doesn't seem to be documented */
  int copyStart = 0x000; /* 0x1F0 */
  int copyEnd = 0x310;
  memset(fakeCAAData,0,sizeof(CAA));

#ifdef METTLE
  /* R12 not really valid, so don't copy anything */
#else
  memcpy(fakeCAAData+copyStart,realCAAData+copyStart,copyEnd-copyStart);
#endif

#if SCHEDULING_TRACE > 0
  printf("CAA at 0x%x\n",realCAAData);
  dumpbuffer(realCAAData,sizeof(CAA));
#endif
  /* Since we own the stack, we had better note its top properly */
  fakeCAA->ceecaaess = task->stack + task->stackSize;
  fakeCAA->ceecaaptr = fakeCAA; /* point back to self, really */

  fakeCAA->loggingContext = (Addr31)getLoggingContext();

  task->fakeCAA = fakeCAAData;
  task->messageQueue = makeQueue(QUEUE_ALL_BELOW_BAR);

#if SCHEDULING_TRACE > 0
  printf("RLETask before start at 0x%x\n",task);
  printf("addr of statusIndicator = 0x%x\n",&(task->statusIndicator));
  printf("addr of pauseElement = 0x%x\n",&(task->pauseElement));
  printf("addr of userPointer = 0x%x\n",&(task->userPointer));
  dumpbuffer((char*)task,sizeof(RLETask));
#endif

  return task;
}

void deleteRLETask(RLETask *task) {

#ifndef METTLE
  /* TODO when we implement a wrapper for join, we'll need to check if the
   * thread has been joined before trying to detach it here. */
  if (task->statusIndicator & RLE_TASK_STATUS_THREAD_CREATED) {
    int detachStatus = threadDetach(&task->threadData);
    if (detachStatus != 0) {
      perror("thread detach error");
    }
  }
#endif

  if (task->messageQueue != NULL) {
    destroyQueue(task->messageQueue);
    task->messageQueue = NULL;
  }

  if (task->fakeCAA != NULL) {
    safeFree31(task->fakeCAA, sizeof(CAA));
    task->fakeCAA = NULL;
  }

  if (!(task->flags & RLE_TASK_DISPOSABLE)) {
    safeFree31(task->stack, task->stackSize);
    task->stack = NULL;
  }

  safeFree31((char *)task, sizeof(RLETask));
  task = NULL;

}

int zosWait(void *ecb, int clearFirst){
  int *ecbAsInteger = (int*)ecb;
#ifdef DEBUG
  printf("ecb ?? 0x%x\n",ecb);
  fflush(stdout);
#endif
  if (clearFirst){
    *ecbAsInteger = 0;
  }
  /*  __asm(" WAIT ECB=(%0),LONG=YES " */
  __asm(" LA  0,1  \n"
        " LR  1,%0 \n"
        " SVC 1 "
        :
        : "r"(ecbAsInteger)
        : "r15");
}

int zosWaitList(void *ecbList, int numberToWait){
  int *ecbListAsInteger = (int*)ecbList;
#ifdef DEBUG
  printf("ecbList ?? 0x%x\n",ecbList);
  fflush(stdout);
#endif
  __asm(ASM_PREFIX
        " LR  0,%1 \n"
        " LR  1,%0 \n"
        " WAIT (0),ECBLIST=(1),LONG=YES "
        :
        : "r"(ecbListAsInteger), "r"(numberToWait)
        : "r15");
}

int zosPost(void *ecb, int completionCode){
  int *ecbAsInteger = (int*)ecb;
  __asm(" POST (%0),%1,LINKAGE=SYSTEM "
        :
        : "r"(ecbAsInteger), "r"(completionCode)
        : "r15");
}

#define schedulingDSECTs SKDDSECT
void schedulingDSECTs(void){

  __asm(

#ifndef _LP64
      "         DS    0D                                                       \n"
      "SKDSA    DSECT ,                                                        \n"
      /* 31-bit save area */
      "SKDSARSV DS    CL4                                                      \n"
      "SKDSAPRV DS    A                    previous save area                  \n"
      "SKDSANXT DS    A                    next save area                      \n"
      "SKDSAGPR DS    15F                  GPRs                                \n"
      /* parameters on stack */
      "SKDSALWS DS    F                    CEEDSALWS for LE 31-bit             \n"
      "SKDSANAB DS    F                    NAB for LE 31-bit                   \n"
      "SKDSASSZ DS    F                    stack size                          \n"
      "SKDSAFLG DS    0F                   RLE task flags                      \n"
      "SKDSAFL4 DS    X                                                        \n"
      "SKDSAFL3 DS    X                                                        \n"
      "SKDSAFL2 DS    X                                                        \n"
      "SKDSAFL1 DS    X                                                        \n"
      "SKDSAPRM DS    A                    parm handle for callee              \n"
      "SKDSALEN EQU   *-SKDSA                                                  \n"
      "SKDSANSA DS    0F                   top of next save area               \n"
      "         EJECT ,                                                        \n"
#else
      "         DS    0D                                                       \n"
      "SKDSA    DSECT ,                                                        \n"
      /* 64-bit save area */
      "SKDSARSV DS    CL4                                                      \n"
      "SKDSAEYE DS    CL4                  eyecatcher F1SA                     \n"
      "SKDSAGPR DS    15D                  GPRs                                \n"
      "SKDSAPRV DS    AD                   previous save area                  \n"
      "SKDSANXT DS    AD                   next save area                      \n"
      /* parameters on stack */
      "SKDSASSZ DS    F                    stack size                          \n"
      "SKDSAFLG DS    0F                   RLE task flags                      \n"
      "SKDSAFL4 DS    X                                                        \n"
      "SKDSAFL3 DS    X                                                        \n"
      "SKDSAFL2 DS    X                                                        \n"
      "SKDSAFL1 DS    X                                                        \n"
      "SKDSAPRM DS    AD                   parm handle for callee              \n"
      "SKDSALEN EQU   *-SKDSA                                                  \n"
      "SKDSANSA DS    0F                   top of next save area               \n"
      "         EJECT ,                                                        \n"
#endif
      /* Warning: must be kept in sync with RLETask in le.h */
      "         DS    0D                                                       \n"
      "RLETASK  DSECT ,                                                        \n"
      "RLETEYE  DS    CL4                                                      \n"
      "RLETSTS  DS    F                                                        \n"
      "RLETFLG  DS    0F                                                       \n"
      "RLETFLG4 DS    X                                                        \n"
      "RLETFLG3 DS    X                                                        \n"
      "RLETFLG2 DS    X                                                        \n"
      "RLETFLG1 DS    X                                                        \n"
      "RF1@TCBC EQU   X'01'                                                    \n"
      "RF1@SRBC EQU   X'02'                                                    \n"
      "RF1@SRBS EQU   X'04'                                                    \n"
      "RF1@NZOS EQU   X'08'                                                    \n"
      "RF1@RCVR EQU   X'10'                                                    \n"
      "RF1@DSPB EQU   X'20'                                                    \n"
      "RLETSSZ  DS    F                                                        \n"
#ifdef _LP64
      "RLETSTK  DS    AD                                                       \n"
      "RLETUPT  DS    AD                                                       \n"
      "RLETUFN  DS    AD                                                       \n"
      "RLETTRM  DS    AD                                                       \n"
      "RLETSP   DS    AD                                                       \n"
#else
      "RLETSTPD DS    F                                                        \n"
      "RLETSTK  DS    A                                                        \n"
      "RLETUPPD DS    F                                                        \n"
      "RLETUPT  DS    A                                                        \n"
      "RLETUFPD DS    F                                                        \n"
      "RLETUFN  DS    A                                                        \n"
      "RLETTRPD DS    F                                                        \n"
      "RLETTRM  DS    A                                                        \n"
      "RLETSPPD DS    F                                                        \n"
      "RLETSP   DS    A                                                        \n"
#endif
      "RLETANCH DS    A                                                        \n"
      "RLETFCAA DS    A                                                        \n"
      "RLETRCTX DS    A                                                        \n"
      "RLETMQ   DS    A                                                        \n"
      "RLETPEL  DS    A                                                        \n"
      "RLETAECB DS    A                                                        \n"
      "RLETHRDT DS    CL32                                                     \n"
      "RLETSDWB DS    A                                                        \n"
      "RLETSDWA DS    CL8192                                                   \n"
      "RLETSKL  EQU   *-RLETASK                                                \n"
      "         EJECT ,                                                        \n"

      "         CVT   DSECT=YES,LIST=NO                                        \n"
      "         EJECT ,                                                        \n"

      "         IWMYCON                                                        \n"
      "         EJECT ,                                                        \n"

      "         IHAPSA                                                         \n"
      "         EJECT ,                                                        \n"

      /* We need to add the following, because of the way the compiler generates CSECTs for Metal C */
#ifdef METTLE
      "         CSECT ,                                                        \n"
      "         RMODE ANY                                                      \n"
#endif
  );

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

