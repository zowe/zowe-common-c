#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/stdio.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "rexx2le.h"

/*
  
  Build instuctions in build/build_rexxintf.sh


*/

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

/*  Calling LE64REXXHandler thru PPTRMP64 (hahahaha!)
    
    - SINGLE Metal Module ZWECFGRX(functionName,args...)
        has a global with state shared with ZWEMT2LE
        include "mt2le.h"
    - EFPL goes "allTheWay" has all the info
    - Calling IRXEXCOM needs a OS (R13) stack
    - not multithreaded
    - REXXInvocation
        EFPL
        EnvBlock
	OS Stack
	EP of IRXEXCOM
	LE64 INFO Bootstrap Name
	MT2LEState 
    - all Targets in LE64 have "int f(REXXInvocation *inv)" prototype
    - mechanism is in PPTRMP64 (Metal64)
                      REXXSHIM (
    - map
      REXX Function Name(STR,STR...) - Metal31FunctionName(EFPL)
        ZWEMT2LE
          has a global
          configure PIPIContext (with bootstrap name)
      
      ZWECFGST (mk,addCOnfig,loadSchemas,loadConfig)
            MK
            AC
	    LC
	    LS
	    VL
	    GT getSchema as stemVars
	    GV getValue with dotted path
      ZWECFG64 top level LE64
          single, well-known symbol 
          main() gives the ZWE64.. symbol set 
      ZWEC64MK
            AC
	    ..
 */


/* Writable Static */
static REXXEnv *theREXXEnv = NULL;

static int traceLevel = 0;

static REXXEnv *getOrMakeREXXEnv(IRXENVB *envBlock){
  /*
    allocate env
    load the program
    call it with BASR and R13 bumped to a safe amount
    */
  if (envBlock->userfield == NULL){
    if (traceLevel >= 1){
      printf("Start making REXX ENV\n");
    }
    int loadStatus = 0;
    printf("JOE.0\n");
    int initEP = (int)loadByName("ZWECFG64",&loadStatus);   
    printf("loadByName ep=0x%x\n",initEP);
    if (initEP == 0 || loadStatus){
      printf("Failed to load 'ZWECFG64', status=%d\n",loadStatus);
      return NULL;
    } else{
      printf("initEP=0x%p\n",initEP);
      initEP &= 0x7FFFFFFE; /* remove DOO-DOO */
    }
    loadStatus = 0;
    printf("before second load\n");
    int irxexcomEP = (int)loadByName("IRXEXCOM",&loadStatus);
    printf("after second load, status=%d\n");
    if (irxexcomEP == 0 || loadStatus){
      printf("Failed to load 'IRXEXCOM', status=%d\n",loadStatus);
      return NULL;
    } else{
      if (traceLevel >= 1){
	printf("irxexcomEP=0x%p\n",irxexcomEP);
      }
      irxexcomEP &= 0x7FFFFFFE; /* remove DOO-DOO */
    }
    REXXEnv *env = (REXXEnv*)safeMalloc31(sizeof(REXXEnv),"REXXEnv");
    memset(env,0,sizeof(REXXEnv));
    memcpy(env->eyecatcher,"REXXENV_",8);
    memcpy(env->le64Name,"ZWERXCFG",8);
    env->stackSize = 0x100000; /* that's a megabyte */
    env->metalStack = (Addr31*)safeMalloc31(env->stackSize,"REXX2LE Stack");

    env->invocation = (REXXInvocation*)safeMalloc31(sizeof(REXXInvocation),"REXXInvocation");
    env->metal64Init = (Addr31)initEP;
    env->irxexcom = (Addr31)irxexcomEP;
    if (traceLevel >= 1){
      printf("Made REXX ENV at 0x%p, invocation at 0x%p\n",env,env->invocation);
      dumpbuffer((char*)env,sizeof(REXXEnv));
    }
    int r13 = 0;
    __asm(" ST 13,%0 ":"=m"(r13)::);
    int initStatus;
    __asm(ASM_PREFIX
	  " XGR 15,15 \n"
	  " XGR 1,1   \n"
	  " L 1,%1 \n"
	  " L 15,%0 \n"
	  " LR 5,13 \n"  /* squirrel hides the nut */
	  " LGF 0,8(,13) \n" /* the nab */
	  " AGHI 0,X'100' \n" /* go up 0x100 */
	  /* here's the magic! 
	     the R13 see by metal64 is up and away from trashing our data */
	  " AHI 13,X'100' \n"  
	  " STG 0,X'88'(,13) \n"  /* proper NAB setup for Metal 64 */
	  " SAM64   \n"
	  " BASR 14,15 \n"
	  " LR  13,5 \n"  /* the squirrel finds the nut */
	  " SAM31   \n"
	  " ST 15,%2"
	  :
	  :"m"(initEP),"m"(env),"m"(initStatus)
	  :"r1","r14");
    envBlock->userfield = env;
    if (traceLevel >= 1){
      printf("back from Metal64 main()\n");
    }
  }
  return (REXXEnv*)envBlock->userfield;
}

static int countREXXArgs(IRXEFPL *efpl){
  char *args = (char*)efpl->rexxArgs;
  int index = 0;
  do{
    REXXArg *arg = (REXXArg*)(args+(index*sizeof(REXXArg)));
    if (arg->address == (void*)REXX_LAST_ARG){
      break;
    }
    index++;
  } while (index < 30);
  return index;
}

static char *getREXXArg(IRXEFPL *efpl, int index, int *lengthPtr){
  char *args = (char*)efpl->rexxArgs;
  REXXArg *arg = (REXXArg*)(args+(index*sizeof(REXXArg)));
  *lengthPtr = arg->length;
  return arg->address;
}

/* errors */
#define RXX2LE64_ZERO_ARGS 12

static int invokeLE64(IRXENVB *envBlock, IRXEFPL *efpl, Addr31 evalBlock){
  int argCount = countREXXArgs(efpl);
  if (argCount == 0){
    return RXX2LE64_ZERO_ARGS;
  }
  REXXEnv *rexxEnv = getOrMakeREXXEnv(envBlock);
  if (traceLevel >=2 ){
    printf("rexxEnv at 0x%p\n",rexxEnv);
  }
  int arg0Len = 0;
  char *arg0 = getREXXArg(efpl,0,&arg0Len);
  if (arg0Len == 4 && !memcmp(arg0,"INIT",4)){
    return 0; /* we successfully initted (a testing mode) */
  } else{
    REXXInvocation *invocation = rexxEnv->invocation;
    if (traceLevel >= 2){
      printf("invocation at 0x%p\n",invocation);
    }
    memset(invocation,0,sizeof(REXXInvocation));
    invocation->env = rexxEnv;
    invocation->efpl = efpl;
    invocation->traceLevel = traceLevel;
    invocation->envBlock = envBlock;
    invocation->evalBlock = evalBlock;
    invocation->functionName = arg0;
    invocation->functionNameLength = arg0Len;
    invocation->argCount = argCount;
    int invocationStatus = 0;
    
    if (traceLevel >= 1){
      printf("ZWECFG31 before ASM call invocation at 0x%p\n");
      dumpbuffer((char*)invocation,sizeof(REXXInvocation));
      printf("ZWECFG31 evalBlock at 0x%p\n",evalBlock);
      dumpbuffer((char*)evalBlock,sizeof(IRXEVALB));
    }
    __asm(ASM_PREFIX
	  " XGR 15,15 \n"
	  " XGR 1,1   \n"
	  " LA 1,X'98'(,13)  \n"
	  " LGF 0,%1         \n"  /* the invocation */
	  " STG 0,X'98'(,13) \n"  /* becomes first arg */
	  " L 15,%0          \n"
	  " LR 5,13          \n"  /* squirrel hides the nut */
	  " LGF 0,8(,13)     \n" /* the nab */
	  " AGHI 0,X'100'    \n" /* go up 0x100 */
	  /* here's the magic! 
	     the R13 see by metal64 is up and away from trashing our data */
	  " AHI 13,X'100'    \n"  
	  " STG 0,X'88'(,13) \n"  /* proper NAB setup for Metal 64 */
	  " SAM64            \n"
	  " BASR 14,15       \n"
	  " LR  13,5         \n"  /* the squirrel finds the nut */
	  " SAM31            \n"
	  " ST 15,%2"
	  :
	  :"m"(rexxEnv->metal64Call),"m"(invocation),"m"(invocationStatus)
	  :"r0","r1","r14");
    if (traceLevel){
      printf("ZWECFG31 after ASM call sequence\n");
    }
    return invocationStatus;
  }
}

int main(int argc, char *argv){
  /* ignore prolog STORAGE OBTAIN cost for first drop */
  uint32_t r0 = 0;
  uint32_t r1 = 0;
  __asm(" L 8,4(,13)  \n"    /* previous frame */
	" L 15,20(,8) \n"  /* saved r0 */
	" ST 15,%0    \n"
	" L 15,24(,8) \n"   /* saved r1 */
	" ST 15,%1 "
	:"=m"(r0),"=m"(r1)
	::"r8");
  Addr31 envBlock = (Addr31)r0;
  printf("ZWECFG31 REXX Call Start\n");
  IRXEFPL *efpl = (IRXEFPL*)INT2PTR(r1);
  if (traceLevel >= 1){
    printf("EFPL at 0x%p, r1 = 0x%x\n",efpl,r1);
    dumpbuffer((char*)efpl,0x20); /* sizeof(IRXEFPL)); */
    printf("ENVBLOCK at 0x%p\n",envBlock);
    dumpbuffer(envBlock,0x20);
  }
  Addr31 evalBlock = *(efpl->evalBlockHandle);
  return invokeLE64((IRXENVB*)envBlock, efpl, evalBlock);
}

