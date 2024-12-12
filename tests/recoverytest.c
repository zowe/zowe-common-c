#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include "stdbool.h"
#include "stddef.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#endif

/*

Metal C 31-bit:

xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o alloc.s ../c/alloc.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm -o alloc.o alloc.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o collections.s ../c/collections.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=collections.asm -o collections.o collections.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o le.s ../c/le.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=le.asm -o le.o le.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o logging.s ../c/logging.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=logging.asm -o logging.o logging.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o metalio.s ../c/metalio.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm -o metalio.o metalio.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o qsam.s ../c/qsam.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm -o qsam.o qsam.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o recovery.s ../c/recovery.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=recovery.asm -o recovery.o recovery.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o scheduling.s ../c/scheduling.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=scheduling.asm -o scheduling.o scheduling.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o timeutls.s ../c/timeutls.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm -o timeutls.o timeutls.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o utils.s ../c/utils.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm -o utils.o utils.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o zos.s ../c/zos.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm -o zos.o zos.s

cp recoverytest.c rcvrtest.c
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I h -I ../h -o rcvrtest.s rcvrtest.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=rcvrtest.asm -o rcvrtest.o rcvrtest.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"
ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main -o "//'$USER.DEV.LOADLIB(RECVRTST)'" \
rcvrtest.o alloc.o collections.o le.o logging.o metalio.o qsam.o recovery.o scheduling.o timeutls.o utils.o zos.o > rcvrtest.link

Metal C 64-bit:

xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o alloc.s ../c/alloc.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm -o alloc.o alloc.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o collections.s ../c/collections.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=collections.asm -o collections.o collections.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o le.s ../c/le.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=le.asm -o le.o le.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o metalio.s ../c/metalio.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm -o metalio.o metalio.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o qsam.s ../c/qsam.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm -o qsam.o qsam.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o recovery.s ../c/recovery.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=recovery.asm -o recovery.o recovery.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o scheduling.s ../c/scheduling.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=scheduling.asm -o scheduling.o scheduling.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o timeutls.s ../c/timeutls.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm -o timeutls.o timeutls.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o utils.s ../c/utils.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm -o utils.o utils.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o zos.s ../c/zos.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm -o zos.o zos.s

cp recoverytest.c rcvrtest.c
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I h -I ../h -o rcvrtest.s rcvrtest.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=rcvrtest.asm -o rcvrtest.o rcvrtest.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"
ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main -o "//'$USER.DEV.LOADLIB(RECVRTST)'" \
rcvrtest.o alloc.o collections.o le.o logging.o metalio.o qsam.o recovery.o scheduling.o timeutls.o utils.o zos.o > rcvrtest.link

LE 31-bit:

xlc -D_OPEN_THREADS=1 "-Wa,goff" "-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),ASMLIB('CEE.SCEEMAC')" '-Wl,ac=1' \
-I ../h -o recoverytest recoverytest.c ../c/alloc.c ../c/collections.c ../c/le.c ../c/logging.c ../c/recovery.c ../c/scheduling.c ../c/timeutls.c ../c/utils.c ../c/zos.c

LE 31-bit XPLINK:

xlc -D_OPEN_THREADS=1 "-Wa,goff" "-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),ASMLIB('CEE.SCEEMAC')" '-Wl,ac=1' \
-I ../h -o recoverytest recoverytest.c ../c/alloc.c ../c/collections.c ../c/le.c ../c/logging.c ../c/recovery.c ../c/scheduling.c ../c/timeutls.c ../c/utils.c ../c/zos.c

LE 64-bit:

xlc -D_OPEN_THREADS=1 "-Wa,goff" "-Wc,LP64,XPLINK,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),ASMLIB('CEE.SCEEMAC')" '-Wl,ac=1' \
-I ../h -o recoverytest recoverytest.c ../c/alloc.c ../c/collections.c ../c/le.c ../c/logging.c ../c/recovery.c ../c/scheduling.c ../c/timeutls.c ../c/utils.c ../c/zos.c

AIX 32-bit:

/tools/AIX-7.2/xlc-13.1.3.0/opt/IBM/xlC/13.1.3/bin/xlc_r -qtls -qlist -I ../h -o recoverytest recoverytest.c ../c/alloc.c ../c/recovery.c ../c/utils.c

AIX 64-bit:

/tools/AIX-7.2/xlc-13.1.3.0/opt/IBM/xlC/13.1.3/bin/xlc_r -q64 -qtls -qlist -I ../h -o recoverytest recoverytest.c ../c/alloc.c ../c/recovery.c ../c/utils.c

Linux GCC:

gcc -pthread  -I ../h -o recoverytest recoverytest.c ../c/alloc.c ../c/recovery.c ../c/utils.c

*/

#include "zowetypes.h"
#include "alloc.h"
#include "openprims.h"
#include "recovery.h"
#include "utils.h"

#ifdef __ZOWE_OS_ZOS
#include "le.h"
#endif

#if defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)
#include "unistd.h"
#endif

#ifdef __ZOWE_OS_ZOS

#define INIT_ZOS_ENV_IF_NEEDED()\
do {\
  CAA *caa = (CAA *)getCAA();\
  caa->rleTask = (RLETask *)safeMalloc31(sizeof(RLETask), "dummy RLE task");\
} while (0)

static void testAnalysisFunction(RecoveryContext * __ptr32 context,
                                 SDWA * __ptr32 sdwa,
                                 void * __ptr32 userData) {
  printf("testAnalysisFunction: context=0x%p\n", context);
  dumpbuffer((char *)context, sizeof(RecoveryContext));
  printf("testAnalysisFunction: SDWA=0x%p\n", sdwa);
  dumpbuffer((char *)sdwa, sizeof(SDWA));
  sprintf(userData, "hello from testAnalysisFunction, SDWA address is 0x%p",
          sdwa);
}

static void testCleanupFunction(RecoveryContext * __ptr32 context,
                                SDWA * __ptr32 sdwa,
                                void * __ptr32 userData) {
  printf("testCleanupFunction: context=0x%p\n", context);
  dumpbuffer((char *)context, sizeof(RecoveryContext));
  printf("testCleanupFunction: SDWA=0x%p\n", sdwa);
  dumpbuffer((char *)sdwa, sizeof(SDWA));
  sprintf(userData, "hello from testCleanupFunction, SDWA address is 0x%p", sdwa);
}

static void printMessage(RecoveryContext * __ptr32 context,
                         SDWA * __ptr32 sdwa,
                         void * __ptr32 userData) {
  printf("%s\n", userData);
}

static void testRecoveryOnNewLSFrame(int level, bool optinLSQuery) {

  if (level == 5) return;

  __asm(
#ifdef _LP64
      "         OILL    %[retAddr],X'0001'                                     \n"
#else
      "         OILH    %[retAddr],X'8000'                                     \n"
#endif
      "         BAKR    %[retAddr],0                                           \n"
      :
      : [retAddr]"r"(&&exit_func)
      :
  );

  printf("testRecoveryOnNewLSFrame entered (level=%d, optinLSQuery=%d)\n",
          level, optinLSQuery);

  int flags = RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY;
  if (optinLSQuery) flags |= RCVR_FLAG_QUERY_LSTACK;

  int pushRC = recoveryPush("testBAKR()", flags,
                            "Recovery dump",
                            NULL, NULL,
                            NULL, NULL);

  if (pushRC == RC_RCV_OK) {
    testRecoveryOnNewLSFrame(level + 1, optinLSQuery);
    printf("about to abend at level %d\n", level);
    int i = 9 / 0;
  } else {
    printf("testRecoveryOnNewLSFrame recovered from an ABEND, pushRC=%d\n", pushRC);
  }

  if (pushRC == RC_RCV_OK) {
    recoveryPop();
  }

  char regs[128];

  __asm volatile (
      "         STMG    1,15,0(%[regs])                                        \n"
      "         PR      ,                                                      \n"
      :
      : [regs]"NR:r15"(&regs)
      : "r15"
  );

  exit_func:

  __asm volatile (
      "         LMG     1,15,0(15)                                             \n"
  );

  printf("testRecoveryOnNewLSFrame ended (level=%d, optinLSQuery=%d)\n",
          level, optinLSQuery);

}

#ifdef METTLE

#include "srb_harness.h"

#define SRB_TEST_EXPECTED_RETURN_VALUE 0xABCD9876

static void srbFunction(uint32_t parm) {

  uint32_t *returnValue = (uint32_t *)parm;

#ifdef METTLE
  establishGlobalEnvironment(makeRLEAnchor());
#endif
  INIT_ZOS_ENV_IF_NEEDED();

  recoveryEstablishRouter(0);

  int pushRC = recoveryPush("srb_routine()",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "Recovery dump",
                            NULL, NULL,
                            NULL, NULL);

  if (pushRC == RC_RCV_OK) {
    int i = 9 / 0;
  } else {
    *returnValue = SRB_TEST_EXPECTED_RETURN_VALUE;
  }

  if (pushRC == RC_RCV_OK) {
    recoveryPop();
  }

  recoveryRemoveRouter();

}

static int testRecoveryInSRB(void) {

  uint32_t srbReturnValue = 0;

  int runRC = run_in_srb(srbFunction, (uint32_t)&srbReturnValue);

  if (runRC != 0) {
    printf("error: SRB not run, rc = %d\n", runRC);
    return 8;
  }

  if (srbReturnValue != SRB_TEST_EXPECTED_RETURN_VALUE) {
    printf("error: unexpected value from SRB test - %u\n", srbReturnValue);
    return 8;
  }

  return 0;
}

#endif /* METTLE */

#else /* zOS targets */

#define INIT_ZOS_ENV_IF_NEEDED()\
do {\
} while (0)

static void testAnalysisFunction(RecoveryContext *context,
                                 int signal,
                                 siginfo_t *info,
                                 void *userData) {
  sprintf(userData, "hello from testAnalysisFunction, "
          "signal=%d, si_errno=%d, si_code=%d, si_addr=%p",
          signal, info->si_errno, info->si_code, info->si_addr);
}

static void testCleanupFunction(RecoveryContext *context,
                                int signal,
                                siginfo_t *info,
                                void *userData) {
  sprintf(userData, "hello form testCleanupFunction, "
          "signal=%d, si_errno=%d, si_code=%d, si_addr=%p",
          signal, info->si_errno, info->si_code, info->si_addr);
}

static void printMessage(RecoveryContext *context,
                         int signal,
                         siginfo_t *info,
                         void *userData) {
  printf("%s\n", (char *)userData);
}

#endif /* Unix */

static void testRecursiveFunction(int depth) {

  if (depth > 10) {
    return;
  }

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "calling a recursive function, depth = %d",
           depth);
  printf("%s (%d)\n", buffer, depth);

  testRecursiveFunction(depth + 1);
}

static void callBadInstruction() {

#ifdef __ZOWE_OS_ZOS
  __asm(" DS    H'0'");
#elif defined(__ZOWE_OS_AIX)
  char buff[1024];
  memset(buff, 0, sizeof(buff));
  ((void (*)())buff)();
#elif defined(__ZOWE_OS_LINUX)
  asm("ud2");
#endif

}

static void *thread1(void *data) {
  INIT_ZOS_ENV_IF_NEEDED();

  printf("hello from thread #1\n");
  int establishRC = recoveryEstablishRouter(RCVR_ROUTER_FLAG_NONE);
  if (establishRC == RC_RCV_OK) {

    int pushRC1 = recoveryPush(NULL, RCVR_FLAG_RETRY, NULL,
                               NULL, NULL,
                               NULL, NULL);
    if (pushRC1 != RC_RCV_OK) {
      printf("warn: something went wrong in thread #1\n");
    }
    else {
      sleep(3);
      testRecursiveFunction(0);
      char *badPtr = (char *)0xFFCC;
      badPtr[3] = 3;
    }
    recoveryPop();

    printf("thread #1 after the first ABEND\n");

    int pushRC2 = recoveryPush(NULL, RCVR_FLAG_RETRY, NULL,
                               NULL, NULL,
                               NULL, NULL);
    if (pushRC2 != RC_RCV_OK) {
      printf("warn: something went wrong in thread #1\n");
    }
    else {
      sleep(3);
      testRecursiveFunction(0);
      char *badPtr = (char *)0xFFCC;
      badPtr[3] = 3;
    }
    recoveryPop();

    printf("thread #1 after the second ABEND\n");

    for (int i = 0; i < 7; i ++) {
      printf("hello from thread #1\n");
      sleep(2);
    }

    recoveryRemoveRouter();
  }
  else {
    printf("error: recovery router not established\n");
  }

  return NULL;
}

static void *thread2(void *data) {
  INIT_ZOS_ENV_IF_NEEDED();

  printf("hello from thread #2\n");
  sleep(15);

  int establishRC = recoveryEstablishRouter(RCVR_ROUTER_FLAG_NONE);
  if (establishRC == RC_RCV_OK) {

    int pushRC = recoveryPush(NULL, RCVR_FLAG_RETRY, NULL,
                              NULL, NULL,
                              NULL, NULL);
    if (pushRC != RC_RCV_OK) {
      printf("warn: something went wrong in thread #2\n");
    }
    else {
      testRecursiveFunction(0);
      char *badPtr = (char *)0xFFCC;
      badPtr[3] = 3;
    }
    recoveryPop();


    for (int i = 0; i < 5; i ++) {
      printf("hello from thread #2\n");
      sleep(2);
    }

    char *message = safeMalloc31(1024, "message");
    pushRC = recoveryPush(NULL, RCVR_FLAG_RETRY, NULL,
                          testAnalysisFunction, message,
                          NULL, NULL);
    if (pushRC == RC_RCV_OK) {
      callBadInstruction();
    } else {
      printf("warn: executed a bad instruction but recovered (\'%s\')\n",
             message);
    }
    recoveryPop();

    recoveryRemoveRouter();
  }
  else {
    printf("error: recovery router not established\n");
  }

  return NULL;
}

static int testXPLINKStackInterrupt(void) {
  // this should trigger LE's stack interrupt, so we're making sure our
  // recovery doesn't interfere with it
#define STACK_BUFFER_SIZE (32 * 1024 * 1024)
  int stackBuffer[STACK_BUFFER_SIZE] = {0};
  stackBuffer[STACK_BUFFER_SIZE - 1] = (int)stackBuffer;
  stackBuffer[STACK_BUFFER_SIZE - 1]++;
  return stackBuffer[STACK_BUFFER_SIZE - 1];
}

int main() {
#ifdef METTLE
  establishGlobalEnvironment(makeRLEAnchor());
#endif
  INIT_ZOS_ENV_IF_NEEDED();

  printf("info: hello from recoverytest.c\n");

  int establishRC = recoveryEstablishRouter(RCVR_ROUTER_FLAG_NONE);
  if (establishRC == RC_RCV_OK) {

#ifdef __ZOWE_OS_ZOS
    recoveryUpdateRouterServiceInfo("RSRCVSTS", "MAINFUNC", "TSTRCVR",
                                    NULL, NULL,
                                    "09/09/17", "1.0RC1", "1357",
                                    NULL);
#endif

    int pushRC = RC_RCV_OK;

    /* recovery #1 */
    pushRC = recoveryPush("state01", RCVR_FLAG_RETRY, "Recovery test #1",
                          printMessage, "top level recovery",
                          NULL, NULL);

#ifdef __XPLINK__
    printf("xplink call result value = %d\n", testXPLINKStackInterrupt());
#endif

    if (pushRC != RC_RCV_OK) {
      printf("warn: something went wrong #1\n");
    }
    else {

      testRecursiveFunction(0);

      recoveryPush("nstate01", RCVR_FLAG_RETRY, NULL,
                   printMessage, "nested recovery #1",
                   NULL, NULL);

      printf("about to push another recovery state...\n");

      recoveryPush("nstate02", RCVR_FLAG_RETRY, NULL,
                   printMessage, "nested recovery #2",
                   NULL, NULL);

      printf("about to ABEND...\n");

      int i = 9 / 0;

    }
    recoveryPop();

    /* recovery #2 */
    char *messageBuffer1 = safeMalloc31(1024, "messageBuffer1");
    char *messageBuffer2 = safeMalloc31(1024, "messageBuffer2");
    pushRC = recoveryPush("state2", RCVR_FLAG_RETRY | RCVR_FLAG_SDWA_TO_LOGREC,
                          "Recovery test dump",
                          testAnalysisFunction, messageBuffer1,
                          testCleanupFunction, messageBuffer2);
    if (pushRC != RC_RCV_OK) {
      printf("warn: something went wrong #2, "
          "messageBuffer1=\'%s\', messageBuffer2=\'%s\'\n",
          messageBuffer1, messageBuffer2);
    }
    else {
      recoverySetDumpTitle("brand new dump title for our recovery test");

#ifdef __ZOWE_OS_ZOS
      recoveryUpdateStateServiceInfo(NULL, NULL, NULL,
                                     "2", "rcvr test component #2",
                                     NULL, NULL, NULL,
                                     "newState2");
#endif

      recoveryPush("state3", RCVR_FLAG_DISABLE, NULL, NULL, NULL, NULL, NULL);
      recoveryPush("state4", RCVR_FLAG_DISABLE, NULL, NULL, NULL, NULL, NULL);
      recoveryPush("state5", RCVR_FLAG_DISABLE, NULL, NULL, NULL, NULL, NULL);

      testRecursiveFunction(0);
      void *nullPointer = NULL;
      memset(nullPointer, 0, 1);
    }
    recoveryPop();

    /* recovery #3 */
    pushRC = recoveryPush(NULL, RCVR_FLAG_RETRY, NULL, NULL, NULL, NULL, NULL);
    if (pushRC != RC_RCV_OK) {
      printf("warn: something went wrong #3\n");
    }
    else {
      printf("info: recovery #3 should not be triggered...\n");
    }
    recoveryPop();

    printf("info: we just survived a couple of ABENDs, continue..\n");

  }
  else {
    printf("error: recovery router not established\n");
    return 8;
  }

#ifndef METTLE
  printf("info: about to start a couple of threads\n");
  OSThread osThreadData1, osThreadData2;
  OSThread *osThread1 = &osThreadData1, *osThread2 = &osThreadData2;
  threadCreate(osThread1, thread1, NULL);
  threadCreate(osThread2, thread2, NULL);

  pthread_join(osThread1->threadID, NULL);
  pthread_join(osThread2->threadID, NULL);
#endif

#ifdef __ZOWE_OS_ZOS

  recoveryEstablishRouter(RCVR_ROUTER_FLAG_NONE);
  testRecoveryOnNewLSFrame(0, false);
  recoveryRemoveRouter();

  recoveryEstablishRouter(RCVR_ROUTER_FLAG_SKIP_LSTACK_QUERY);
  testRecoveryOnNewLSFrame(0, true);
  recoveryRemoveRouter();

#ifdef METTLE
  int srbTestRC = testRecoveryInSRB();
  if (srbTestRC != 0) {
    return 8;
  }
#endif

#endif

  printf("info: recovery test successful\n");

  return 0;
}

