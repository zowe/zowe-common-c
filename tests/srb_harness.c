#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdarg.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#endif

/*

Metal C 31-bit:

xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o alloc.s ../c/alloc.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm -o alloc.o alloc.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o metalio.s ../c/metalio.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm -o metalio.o metalio.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o qsam.s ../c/qsam.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm -o qsam.o qsam.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o timeutls.s ../c/timeutls.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm -o timeutls.o timeutls.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o utils.s ../c/utils.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm -o utils.o utils.s
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h -o zos.s ../c/zos.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm -o zos.o zos.s

cp srb_harness.c srb_harness.c
xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I h -I ../h -o srb_harness.s srb_harness.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=srb_harness.asm -o srb_harness.o srb_harness.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"
ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main -o "//'$USER.DEV.LOADLIB(SRBTEST)'" \
srb_harness.o alloc.o metalio.o qsam.o timeutls.o utils.o zos.o > srb_harness.link

Metal C 64-bit:

xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o alloc.s ../c/alloc.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm -o alloc.o alloc.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o metalio.s ../c/metalio.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm -o metalio.o metalio.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o qsam.s ../c/qsam.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm -o qsam.o qsam.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o timeutls.s ../c/timeutls.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm -o timeutls.o timeutls.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o utils.s ../c/utils.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm -o utils.o utils.s
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h -o zos.s ../c/zos.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm -o zos.o zos.s

xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 -Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I h -I ../h -o srb_harness.s srb_harness.c
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=srb_harness.asm -o srb_harness.o srb_harness.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"
ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main -o "//'$USER.DEV.LOADLIB(SRBTEST)'" \
srb_harness.o alloc.o metalio.o qsam.o timeutls.o utils.o zos.o > srb_harness.link


*/

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "zos.h"

#ifdef _LP64

#pragma prolog(srb_routine,\
"        PUSH  USING                              \n\
SRBPRLG  DS    0H                                 \n\
         BAKR  14,0                               \n\
         SAM64                                    \n\
         LARL  10,SRBPRLG                         \n\
         USING SRBPRLG,10                         \n\
         LA    2,128                              \n\
         SPKA  0(2)                               \n\
         STORAGE OBTAIN,COND=YES,LENGTH=8192,KEY=8,SP=230,LOC=31,BNDRY=PAGE \n\
         LTR   15,15                              \n\
         JNZ   SRBRET                             \n\
SRBINIT  DS    0H                                 \n\
         LLGFR 13,1                               \n\
         USING SRBSTACK,13                        \n\
         EREGG 0,1                                \n\
         LA    13,SRBSA                           \n\
\
         J     SRBHNDLR                           \n\
\
         LTORG                                    \n\
         DROP  10                                 \n\
SRBHNDLR DS    0H                                 \n\
         DROP                                     \n\
         POP   USING                               ")

#else

#pragma prolog(srb_routine,\
"        PUSH  USING                              \n\
SRBPRLG  DS    0H                                 \n\
         BAKR  14,0                               \n\
         SAM31                                    \n\
         LARL  10,SRBPRLG                         \n\
         USING SRBPRLG,10                         \n\
         LA    2,128                              \n\
         SPKA  0(2)                               \n\
         STORAGE OBTAIN,COND=YES,LENGTH=8192,KEY=8,SP=230,LOC=31,BNDRY=PAGE \n\
         LTR   15,15                              \n\
         JNZ   SRBRET                             \n\
SRBINIT  DS    0H                                 \n\
         LLGFR 13,1                               \n\
         USING SRBSTACK,13                        \n\
         EREGG 0,1                                \n\
         LA    13,SRBSA                           \n\
\
         J     SRBHNDLR                           \n\
\
         LTORG                                    \n\
         DROP  10                                 \n\
SRBHNDLR DS    0H                                 \n\
         DROP                                     \n\
         POP   USING                               ")

#endif

#pragma epilog(srb_routine,\
"        PUSH  USING                              \n\
SRBEPLG  DS    0H                                 \n\
         LARL  10,SRBEPLG                         \n\
         USING SRBEPLG,10                         \n\
         SLFI  13,SRBPARML                        \n\
         LR    1,13                               \n\
         STORAGE RELEASE,COND=NO,LENGTH=8192,KEY=8,SP=230,ADDR=(1) \n\
SRBRET   DS    0H                                 \n\
         DROP  10                                 \n\
         LR    15,0                               \n\
         EREGG 0,1                                \n\
         SPKA  0                                  \n\
         PR                                       \n\
         DROP                                     \n\
         POP   USING                               ")

static void wto(const char *format, ...) {

  __packed struct {
    __packed struct {
      uint8_t reply_buff_len;
      uint8_t length;
      uint8_t flags1;
      uint8_t flags2;
    } header;
    char msg_buff[126];
  } wto_parm = {0};

  va_list args;
  va_start(args, format);
  int print_rc = vsnprintf(wto_parm.msg_buff, sizeof(wto_parm.msg_buff),
                           format, args);
  va_end(args);

  if (print_rc >= 0) {
    wto_parm.header.length =
        sizeof(wto_parm.header) + strlen(wto_parm.msg_buff);
  } else {
    return;
  }

  __asm(
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         WTO     LINKAGE=BRANCH,MF=(E,%[parm])                          \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      :
      : [parm]"m"(wto_parm)
      : "r0", "r1", "r14", "r15"
  );

}

 typedef struct stoken_t {
   char value[8];
 } stoken_t;

static int sched_sync_srb(void * __ptr32 ep,
                          void * __ptr32 parm,
                          stoken_t token,
                          void * __ptr32 frr_addr,
                          int * __ptr32 comp_code,
                          int * __ptr32 srb_rc,
                          int * __ptr32 srb_rsn) {

  char macro_parm_list[512];

  int rc = 0;

  unsigned int local_ep = ((int)ep) | 0x80000000;
  unsigned int local_parm = (int)parm;
  unsigned int local_frr_addr =
      (frr_addr != NULL) ? (((int)frr_addr) | 0x80000000) : 0;
  stoken_t local_stoken = token;

  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         IEAMSCHD EPADDR=(%[ep])"
      ",ENV=STOKEN"
      ",PRIORITY=LOCAL"
      ",SYNCH=YES"
      ",PARM=(%[parm])"
      ",TARGETSTOKEN=(%[stoken])"
      ",FRRADDR=(%[frr])"
      ",SYNCHCOMPADDR=%[cc]"
      ",SYNCHCODEADDR=%[srb_rc]"
      ",SYNCHRSNADDR=%[srb_rsn]"
      ",RETCODE=%[rc]"
      ",MF=(E,(%[macro_parm_list]),COMPLETE)                                   \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif

      : [rc]"=m"(rc),
        [cc]"=m"(comp_code),
        [srb_rc]"=m"(srb_rc),
        [srb_rsn]"=m"(srb_rsn)

      : [ep]"r"(&local_ep),
        [parm]"r"(&local_parm),
        [stoken]"r"(&local_stoken),
        [frr]"r"(&local_frr_addr),
        [macro_parm_list]"r"(&macro_parm_list)

      : "r0", "r1", "r14", "r15"

  );

  return rc;
}

static stoken_t get_my_stoken(void) {

  ASSB *assb = getASSB(getASCB());

  stoken_t token = *(stoken_t *)&assb->assbstkn;

  return token;
}

typedef struct srb_context_t {
  void (*func)(uint32_t parm);
  uint32_t parm;
} srb_context;



static void srb_routine(void) {

  srb_context * __ptr32 cntx = 0;
  __asm(" ST 1,%[cntx] " : [cntx]"=m"(cntx));

  cntx->func(cntx->parm);

}

int run_in_srb(void (*func)(uint32_t parm), uint32_t parm) {

  int was_prob = supervisorMode(TRUE);

  stoken_t my_stoken = get_my_stoken();

  int32_t sched_rc = 0;
  int32_t cc = 0;
  int32_t srb_rc = 0;
  int32_t srb_rsn = 0;

  srb_context cntx = {.func = func, .parm = parm};

  printf("cntx=%p\n", &cntx);

  sched_rc = sched_sync_srb((void *)srb_routine, &cntx, my_stoken, NULL,
                            &cc, &srb_rc, &srb_rsn);

  printf("testSRB: sched_rc=%d, cc=%d, srb_rc=0x%08x, srb_rsn=0x%08x\n",
         sched_rc, cc, srb_rc, srb_rsn);

  if (was_prob) {
    supervisorMode(FALSE);
  }

  return cc ? -1 : 0;

}

void srb_test_dsect() {

  __asm(
      "         DS    0F                                                       \n"
      "SRBSTACK DSECT ,                                                        \n"
      /* parameters on stack */
      "SRBPARM  DS    0F                   SRB exit parm list                  \n"
      "SRBTPRM1 DS    A                    parm #1                             \n"
      "SRBTPRM  DS    A                    parm #1                             \n"
      "SRBPLPAD DS    0F                   padding                             \n"
      "SRBPARML EQU   *-SRBPARM                                                \n"
      /* 31-bit save area */
      "SRBSA    DS    0F                                                       \n"
      "SRBSARSV DS    CL4                                                      \n"
      "SRBSAPRV DS    A                    previous save area                  \n"
      "SRBSANXT DS    A                    next save area                      \n"
      "SRBSAGPR DS    15F                  GPRs                                \n"
      "SRBSTCKL EQU   *-SRBSTACK                                               \n"
      "         EJECT ,                                                        \n"
#ifdef METTLE
      "         CSECT ,                                                        \n"
      "         RMODE ANY                                                      \n"
#endif

  );

}
