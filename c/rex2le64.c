#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/stdio.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "metalio.h"
#include "qsam.h"
#include "zos.h"

#include "lepipi.h"
#include "rexx2le.h"

/*
  export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'://'CBC.SCCNOBJ'"

  LE 64 compile
  export _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0"

  // Metal 64
  xlc -qmetal -q64 -DMSGPREFIX=\"JOE\" -DMETAL_PRINTF_TO_STDOUT=1 -DSUBPOOL=132 -DMETTLE=1 "-Wc,longname,langlvl(extc99),goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -I ../h -S pipitest.c ../c/metalio.c ../c/zos.c ../c/qsam.c ../c/timeutls.c ../c/utils.c ../c/alloc.c

  export STEPLIB="CBC.SCCNCMP:JDEVLIN.DEV.LOADLIB"

  // metal 31
  xlc -qmetal -DMSGPREFIX=\"JOE\" -DMETAL_PRINTF_TO_STDOUT=1 -DSUBPOOL=132 -DMETTLE=1 "-Wc,longname,langlvl(extc99),goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -I ../h -S pipitest.c ../c/metalio.c ../c/zos.c ../c/qsam.c ../c/timeutls.c ../c/utils.c ../c/alloc.c

  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=pipitest.asm pipitest.s
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm alloc.s
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm utils.s
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm timeutls.s
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm zos.s
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm qsam.s
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm metalio.s

  ld -b rent -b case=mixed -b map -b xref -b reus -e main -o pipitest pipitest.o metalio.o qsam.o zos.o timeutls.o utils.o alloc.o

*/

/* Function Codes */



PIPIContext *makePIPIContext(bool is64, int entryCount, char *runtimeOptions){
  int allocSize = sizeof(PIPIContext)+(entryCount*PIPI_ENTRY_SIZE);
  PIPIContext *context = (PIPIContext*)safeMalloc31(allocSize,"PIPIContext");
  memset(context,0,allocSize);
  memcpy(context->eyecatcher,"PIPI",4);
  char *epName = NULL;
  if (is64){
    context->flags = PIPI_CONTEXT_64;
    memcpy(context->header._64.eyecatcher,"CELQPTBL",8);
    context->header._64.entryCount = entryCount;
    context->header._64.entrySize = PIPI_ENTRY_SIZE;
    context->header._64.version = 100;
    epName = "CELQPIPI";
  } else{
    memcpy(context->header._31.eyecatcher,"CEEXPTBL",8);
    context->header._31.entryCount = entryCount;
    context->header._31.entrySize = PIPI_ENTRY_SIZE;
    context->header._31.version = 2;
    epName = "CEEPIPI ";
  }
  int loadStatus = 0;
  int epAddress = (int)loadByName(epName,&loadStatus);
  if ((epAddress == 0) || (loadStatus != 0)){
    printf("Failed to get %s status=%d\n",epName,loadStatus);
    safeFree((char*)context,allocSize);
    return NULL;
  } else{
    printf("Loaded %s loadStatus=%d, ep=0x%p\n",epName,loadStatus,epAddress);
    context->pipiEP = (void*)(epAddress&0x7FFFFFFE); /* strip out the magic bits */
  }
  printf("pipiContext=0x%p\n",context);
  return context;
}

int pipiSetEntry(PIPIContext *context, int index, char *name, void *ep){    /* ep can be null */
  int len = strlen(name);
  if (len > 8){
    printf("name too long '%s'\n",name);
    return 8;
  }
  char paddedName[8];
  memset(paddedName,' ',8);
  memcpy(paddedName,name,len);
  if (context->flags & PIPI_CONTEXT_64){
    PIPI64Header *header = &context->header._64;
    PIPI64Entry *entry = &(header->entries[index]);
    entry->address = (uint64_t)ep;
    memcpy(entry->name,name,8);
  } else{
    PIPI31Header *header = &context->header._31;
    PIPI31Entry *entry = &(header->entries[index]);
    memcpy(entry->name,name,8);
    entry->address = (Addr31)ep;
  }
}

int pipiInit(PIPIContext *context){
  pipiINITSUB *init = (pipiINITSUB*)context->pipiEP;
  int functionCode = PIPI_INIT_SUB;
  int zero = 0;
  if (context->flags & PIPI_CONTEXT_64){
    printf("before init64, init=0x%p\n",init);
    void *header = &context->header._64;
    int status = init(&functionCode,
		      &header,
		      &zero,
		      &context->runtimeOptions,
		      &context->token);
    printf("CELQPIPI Status = %d\n",status);
    fflush(stdout);
    printf("PIPIContext\n");
    dumpbuffer((char*)context,sizeof(PIPIContext));
    printf("PIPIHeader/Entries\n");
    dumpbuffer((char*)header,sizeof(PIPI64Header)+0x18);
    return status;
  } else{
    printf("before init, init=0x%p\n",init);
    void *header = &context->header._31;
    int status = init(&functionCode,
		      &header,
		      &zero,
		      &context->runtimeOptions,
		      &context->token);
    printf("CEEPIPI Status = %d\n",status);
    fflush(stdout);
    printf("PIPIContext\n");
    dumpbuffer((char*)context,sizeof(PIPIContext));
    printf("PIPIHeader/Entries\n");
    dumpbuffer((char*)header,sizeof(PIPI31Header)+0x18);
    return status;
  }
  
}

int pipiCallSub(PIPIContext *context, int ptabIndex, void **args){
  pipiCALLSUB *call = (pipiCALLSUB*)context->pipiEP;
  int functionCode = PIPI_CALL_SUB;
  int zero = 0;
  int returnCode = 0;
  int reasonCode = 0;
  if (context->flags & PIPI_CONTEXT_64){
    char feedback[16];
    void *header = &context->header._64;
    if (context->traceLevel >= 1){
      printf("before call64, call=0x%p, arg0(invoc)=0x%p\n",call,args[0]);
    }
    int status = call(&functionCode,
		      &ptabIndex,
		      &context->token,
		      &args,
		      &returnCode,
		      &reasonCode,
		      &feedback);
    if (context->traceLevel >= 1){
      printf("CELQPIPI call Status = %d ret=%d reason=%d\n",status,returnCode, reasonCode);
      fflush(stdout);
      dumpbuffer((char*)header,sizeof(PIPI64Header)+0x10);
      printf("CEE000 feedback\n");
      dumpbuffer(feedback,16);
    }
    return status;
  } else{
    printf("before call, call=0x%p\n",call);
    /* 
       https://www.ibm.com/docs/en/zos/2.1.0?topic=invocation-call-sub-subroutines
     */
    void *header = &context->header._31;
    int feedback[3];
    char argData[256];
    memset(argData,0,256);
    int status = call(&functionCode,
		      &ptabIndex,
		      &context->token,
		      argData,
		      &returnCode,
		      &reasonCode,
		      feedback);
    /* returnCode has the return value of targetRoutine */
    if (context->traceLevel >= 1){
      printf("CEEPIPI call Status = %d ret=%d reason=%d\n",status,returnCode, reasonCode);
      fflush(stdout);
      dumpbuffer((char*)header,sizeof(PIPI31Header)+0x10);
      printf("feedbag\n");
      dumpbuffer((char*)feedback,12);
    }
    return status;
  }
  
}

int pipiTerm(PIPIContext *context){

}

static uint64_t getR1(void) {
  uint64_t value;
  __asm(ASM_PREFIX
#ifdef _LP64
        " STG 1,%0 \n"
#else
        " LLGTR 0,1 \n"
        " STG 0,%0 \n"
#endif
        :"=m"(value)
        :
        :"r0");
  return value;
}

static int callFromREXX(REXXInvocation *invocation){
  printf("callFromREXX invocation=0x%p\n",invocation);
  REXXEnv *env = invocation->env;
  PIPIContext *pipi = env->pipiContext;
  pipi->traceLevel = invocation->traceLevel;
  if (pipi->traceLevel >= 1){
    printf("round-trip thru MTL2LE64 start pipi=0x%p\n",pipi);
  }
  void *args[1] ={ invocation};
  int callStatus = pipiCallSub(pipi,0,args);
  if (pipi->traceLevel >= 1){
    printf("round-trip thru MTL2LE64 end, callStatus=%d\n",callStatus);
  }
  return callStatus;
}

int main(int argc, char *argv){
  uint64_t r1 = getR1();
  printf("in MTL2LE64, r1 is 0x%llx\n",r1);
  bool is64 = true;
  REXXEnv *env = (REXXEnv*)r1;
  printf("MTL2LE64 Start env=0x%p\n",env);
  int loadStatus = 0;
  /* 
     void *testEP = loadByName("LETARGET",&loadStatus);
     wtoPrintf("Testing load status=%d, ep=0x%p\n",loadStatus,testEP);
     */
  char *options = safeMalloc31(255,"CEE Runtime Options");
  memset(options,' ',255);
  PIPIContext *pipi = makePIPIContext(is64,1,options); /* 31 bit alloc */
  env->pipiContext = pipi;
  env->metal64Call = (void*)callFromREXX;
  printf("pipi=0x%p\n",pipi);
  pipiSetEntry(pipi,0,env->le64Name,NULL);
  int initStatus = pipiInit(pipi);
  /* 
     HERE
     1 make REXXCMGR (le64 wrapper for configmgr)
       1.5 call it once to get the service point that receives REXXInvocation
     2 populate call and term entry points
     3 test
     4 build subdispatch in rexxcmgr.c
     5 pass log file in from TSO REXX layer
   */
  return 0;
}

