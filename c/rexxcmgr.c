#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "logging.h"
#include "json.h"
#include "jsonschema.h"
#include "configmgr.h"
#include "rexx2le.h"

/* 

   Building info under builds/build_rexxcmgr.sh

 */

#pragma linkage(REXXCMGR,fetchable)

#define MAX_VAR 1024
#define MAX_VALUE 1024
#define EXCOM_SAVE_SIZE 256 /* That covers everything */

typedef struct EXCOMContext_tag{
  char eyecatcher[8]; /* for IRXEXCOM in 31 bit */
  Addr31 envBlock;
  Addr31 reserved;
  char varName[MAX_VAR];
  char value[MAX_VALUE];
  char excomSave[EXCOM_SAVE_SIZE];
  REXXInvocation *invocation;
  ConfigManager *mgr;
  IRXEXCOMParms parms;
  SHVBLOCK shvblock;
} EXCOMContext;

static EXCOMContext *makeEXCOMContext(REXXInvocation *invocation, ConfigManager *mgr){
  EXCOMContext *context = (EXCOMContext*)safeMalloc31(sizeof(EXCOMContext),"EXCOMContext");
  memset(context,0,sizeof(EXCOMContext));
  memcpy(context->eyecatcher,"IRXEXCOM",8);
  context->invocation = invocation;
  context->envBlock = invocation->envBlock;
  context->mgr = mgr;
  return context;
}

static void freeEXCOMContext(EXCOMContext *context){
  safeFree31((char*)context,sizeof(EXCOMContext));
}

static bool shouldUpcase = true;

static void upcase(char *s, int len){
  for (int i=0; i<len; i++){
    s[i] = (char)toupper((int)s[i]);
  }
}


int storeREXXVariable(EXCOMContext *excomContext,
		      int varLength,
		      int valueLength){
  REXXInvocation *invocation = excomContext->invocation;
  FILE *out = excomContext->mgr->traceOut;
  ConfigManager *mgr = excomContext->mgr;
  REXXEnv *env = invocation->env;
  IRXEXCOMParms *p = &excomContext->parms;
  memset(p,0,sizeof(IRXEXCOMParms));
  SHVBLOCK *shvblock = &excomContext->shvblock;
  memset(shvblock,0,sizeof(SHVBLOCK));
  p->eyecatcher = excomContext->eyecatcher;
  p->parm2 = 0;
  p->parm3 = 0;
  p->shvblock = (  SHVBLOCK *__ptr32)shvblock;
  p->envblock = &invocation->envBlock;
  shvblock->shvnext = NULL;
  shvblock->shvuse = 0;
  shvblock->shvcode = EXCOM_SET_SYMBOLIC_NAME;
  if (shouldUpcase){
    upcase(excomContext->varName,varLength);
  }
  shvblock->shvnama = &excomContext->varName[0];
  shvblock->shvnaml = varLength;
  shvblock->shvvala = &excomContext->value[0];
  shvblock->shvvall = valueLength;
  int retcodeAddr = (int)&p->retcode;
  p->retcodePtr = retcodeAddr|0x80000000;  /* high bit setting needed for this crufty API */
  int status = 0;
  char *osStack = excomContext->excomSave;
  uint64_t r13Save;
  if (mgr->traceLevel >= 1){
    fprintf(mgr->traceOut,"Before IRXEXCOM parms at 0x%p osStack at 0x%p\n",p,osStack);
    dumpBufferToStream((char*)p,sizeof(IRXEXCOMParms),mgr->traceOut);
    fprintf(mgr->traceOut,"eyecatcher at 0x%p\n",p->eyecatcher);
    dumpBufferToStream(p->eyecatcher,8,mgr->traceOut);
    fprintf(mgr->traceOut,"SHVBLOCK at 0x%p\n",shvblock);
    dumpBufferToStream((char*)shvblock,sizeof(SHVBLOCK),mgr->traceOut);
    fprintf(mgr->traceOut,"var '%s' value '%s'\n",excomContext->varName,excomContext->value);
  }
  Addr31 irxexcom = env->irxexcom;
  __asm(ASM_PREFIX
	" LG  1,%0  \n"
	" LGF 15,%1 \n"
	" STG 13,%2 \n"
	" LG  13,%3 \n"
	/* " DC XL2'0000' \n" */
	" SAM31 \n"
        " BALR 14,15 \n"
	" SAM64 \n"
	" LG  13,%2 \n"
	" ST 15,%4 "
	:
	:"m"(p),"m"(irxexcom),"m"(r13Save),"m"(osStack),"m"(status)
	:"r1","r14","r15");
  if (mgr->traceLevel >= 1){
    fprintf(out,"excom status= %d, and shvblock after\n",status);
    dumpBufferToStream((char*)shvblock,sizeof(SHVBLOCK),mgr->traceOut);
  }
  return status;
}

static char *getSeparator(int varLength, char *separator, int separatorCount){
  if (varLength == 0){
    return "";
  } else if (separatorCount == 0){
    return ".";
  } else{
    return separator;
  }
}

static void writeJSONToREXX1(EXCOMContext *context, Json *json, int varLength,
			     char *separator, int separatorCount){
  FILE *out = context->mgr->traceOut;
  switch (json->type) {
  case JSON_TYPE_DOUBLE:
    {
      int valueLength = snprintf(context->value,MAX_VALUE,"%f",jsonAsDouble(json));
      storeREXXVariable(context,varLength,valueLength);
      break;
    }  
  case JSON_TYPE_INT64:
    {
      int valueLength = snprintf(context->value,MAX_VALUE,"%lld",jsonAsInt64(json));
      storeREXXVariable(context,varLength,valueLength);
      break;
    }
  case JSON_TYPE_NUMBER:
    {
      int valueLength = snprintf(context->value,MAX_VALUE,"%d",jsonAsNumber(json));
      storeREXXVariable(context,varLength,valueLength);
      break;
    }
  case JSON_TYPE_STRING:
    {
      int valueLength = snprintf(context->value,MAX_VALUE,"%s",jsonAsString(json));
      storeREXXVariable(context,varLength,valueLength);
      break;
    }
  case JSON_TYPE_BOOLEAN:
    {
      int valueLength = snprintf(context->value,MAX_VALUE,"%s",(jsonAsBoolean(json) ? "TRUE" : "FALSE"));
      storeREXXVariable(context,varLength,valueLength);
      break;
    }
  case JSON_TYPE_NULL:
    {
      int valueLength = snprintf(context->value,MAX_VALUE,"<NULL>");
      storeREXXVariable(context,varLength,valueLength);
      break;
    }
  case JSON_TYPE_OBJECT:
    {
      JsonObject *jsonObject = jsonAsObject(json);
      JsonProperty *property;
      
      for (property = jsonObjectGetFirstProperty(jsonObject);
	   property != NULL;
	   property = jsonObjectGetNextProperty(property)) {
	char *key = jsonPropertyGetKey(property);
	int subLength = varLength + snprintf(context->varName+varLength,MAX_VAR-varLength,"%s%s",
					     getSeparator(varLength,separator,separatorCount),
					     key);
	writeJSONToREXX1(context,jsonPropertyGetValue(property),subLength,separator,separatorCount+1);
      }
      break;
    }
  case JSON_TYPE_ARRAY:
    {
      JsonArray *jsonArray = jsonAsArray(json);
      int count = jsonArrayGetCount(jsonArray);
      
      for (uint32_t i = 0; i < count; i++) {
	int subLength = varLength + snprintf(context->varName+varLength,MAX_VAR-varLength,"%s%d",
					     getSeparator(varLength,separator,separatorCount),
					     i);
        writeJSONToREXX1(context,jsonArrayGetItem(jsonArray,i),subLength,separator,separatorCount+1);
      }
      break;
    }
  default:
    fprintf(out,"*** PANIC *** unknown JSON type %d\n",json->type);
    break;
  }
  fprintf(out,"back from write REXX\n");
}

static void writeJSONToREXX(REXXInvocation *invocation, ConfigManager *mgr, Json *json, char *stem, char *separator){
  EXCOMContext *context = makeEXCOMContext(invocation,mgr);
  int stemLen = strlen(stem);
  memcpy(context->varName,stem,stemLen);
  writeJSONToREXX1(context,json,stemLen,separator,0);
  freeEXCOMContext(context);
}

static void copyValidityException(EXCOMContext *context, ValidityException *exception, int depth, int varLength){
  int messageLen = strlen(exception->message);
  int extendedVarLength = snprintf(context->varName+varLength,MAX_VAR-varLength,"%s%dmessage",
				   (varLength == 0 ? "" : "."),
				   depth);
  memcpy(context->value,exception->message,messageLen);
  storeREXXVariable(context,extendedVarLength,messageLen);
  ValidityException *child = exception->firstChild;
  int childCount = 0;
  while (child){
    child = child->nextSibling;
    childCount++;
  }
  extendedVarLength = snprintf(context->varName+varLength,MAX_VAR-varLength,"%s%dsubMessageCount",
			       (varLength == 0 ? "" : "."),
			       depth);
  int countLength = snprintf(context->value,MAX_VALUE,"%d",childCount);
  storeREXXVariable(context,extendedVarLength,countLength);
  child = exception->firstChild;
  while (child){
    int newStemLength = varLength + snprintf(context->varName+varLength,MAX_VAR-varLength,"%s%dsubMessageCount",
					     (varLength == 0 ? "" : "."),
					     depth);
    copyValidityException(context,child,depth+1,newStemLength);
    child = child->nextSibling;
  }
}

static void writeValidityExceptionToRexx(REXXInvocation *invocation, ConfigManager *mgr,
					 ValidityException *exception, char *stem){
  EXCOMContext *context = makeEXCOMContext(invocation,mgr);
  int stemLen = strlen(stem);
  memcpy(context->varName,stem,stemLen);
  copyValidityException(context,exception,0,0);
  freeEXCOMContext(context);
}

static int traceLevel = 1;
static ConfigManager *theConfigManager = NULL;

static int rexxSetTraceLevel(ConfigManager *mgr,
			     REXXInvocation *invocation,
			     int argCount,
			     char **args){ /* null terminated */
  if (argCount != 2){
    return ZCFG_BAD_REXX_ARG_COUNT;
  }
  int level = strtol((const char *)args[1],NULL,10);
  if (level >= 0 && level <= 10){
    cfgSetTraceLevel(mgr,level);
    return ZCFG_SUCCESS;
  } else{
    return ZCFG_BAD_TRACE_LEVEL;
  }
}

static int rexxAddConfig(ConfigManager *mgr,
			 REXXInvocation *invocation,
			 int argCount,
			 char **args){ /* null terminated */
  if (argCount != 2){
    return ZCFG_BAD_REXX_ARG_COUNT;
  }
  char *configName = args[1];
  cfgAddConfig(mgr,configName);
  return 0;
}

static int rexxSetConfigPath(ConfigManager *mgr,
			     REXXInvocation *invocation,
			     int argCount,
			     char **args){ /* null terminated */
  if (argCount != 3){
    return ZCFG_BAD_REXX_ARG_COUNT;
  }
  char *configName = args[1];
  return cfgSetConfigPath(mgr,configName,args[2]);
}

static int rexxLoadSchemas(ConfigManager *mgr,
			   REXXInvocation *invocation,
			   int argCount,
			   char **args){ /* null terminated */
  if (argCount != 3){
    return ZCFG_BAD_REXX_ARG_COUNT;
  }
  char *configName = args[1];
  return cfgLoadSchemas(mgr,configName,args[2]);
}

static int rexxLoadConfiguration(ConfigManager *mgr,
				 REXXInvocation *invocation,
				 int argCount,
				 char **args){ /* null terminated */
  /* string arg 1 */
  char *configName = args[1];
  return cfgLoadConfiguration(mgr,configName);
}

static int rexxSetParmlibMemberName(ConfigManager *mgr,
				    REXXInvocation *invocation,
				    int argCount,
				    char **args){ /* null terminated */
  if (argCount != 3){
    return ZCFG_BAD_REXX_ARG_COUNT;
  }
  char *configName = args[1];
  return cfgSetParmlibMemberName(mgr,configName,args[2]);
}

static int rexxValidate(ConfigManager *mgr,
			REXXInvocation *invocation,
			int argCount,
			char **args){ /* null terminated */
  if (argCount != 3){
    return ZCFG_BAD_REXX_ARG_COUNT;
  }
  bool ok = false;
  char *configName = args[1];
  char *stem = args[2];
  JsonValidator *validator = makeJsonValidator();
  validator->traceLevel = mgr->traceLevel;
  int validateStatus = cfgValidate(mgr,validator,configName);
  fprintf(mgr->traceOut,"validateStatus=%d\n",validateStatus);
  if (validateStatus == JSON_VALIDATOR_HAS_EXCEPTIONS){
    writeValidityExceptionToRexx(invocation,mgr,validator->topValidityException,stem);
  } else if (validateStatus == JSON_VALIDATOR_INTERNAL_FAILURE){
    fprintf(mgr->traceOut,"validation internal failure: %s",validator->errorMessage);
  }
  freeJsonValidator(validator);
  return validateStatus;
}

static int rexxGetConfigData(ConfigManager *mgr,
			     REXXInvocation *invocation,
			     int argCount,
			     char **args){ /* null terminated */
  /* string arg 1, return or place under stem */
  if (argCount != 4){
    return ZCFG_BAD_REXX_ARG_COUNT;
  }
  char *configName = args[1];
  char *stem = args[2];
  char *separator = args[3];
  Json *json = cfgGetConfigData(mgr, (const char *)configName);
  if (json){
    writeJSONToREXX(invocation,mgr,json,stem,separator);
    return 0;
  } else{
    return ZCFG_NO_CONFIG_DATA;
  }
}


#define MAX_ARGS 30

static int rexxEntry(ConfigManager *mgr, REXXInvocation *invocation){
  FILE *out = mgr->traceOut;
  if (mgr->traceLevel >= 1){
    fprintf(out,"Eval Block (Mk2) at 0x%p\n",invocation->evalBlock);
    dumpBufferToStream((char*)invocation->evalBlock,0x20,out);
  }
  IRXEFPL *efpl = invocation->efpl;
  char *args = (char*)efpl->rexxArgs;
  ShortLivedHeap *argHeap = makeShortLivedHeap(0x10000,100);
  int index = 0;
  char *copiedArgs[MAX_ARGS];
  do{
    REXXArg *arg = (REXXArg*)(args+(index*sizeof(REXXArg)));
    if (arg->address == (void*)REXX_LAST_ARG){
      break;
    }
    char *argCopy = SLHAlloc(argHeap,arg->length+1);
    copiedArgs[index] = argCopy;
    memcpy(argCopy,arg->address,arg->length);
    argCopy[arg->length] = 0;
    if (mgr->traceLevel >= 1){
      fprintf(out,"--- Arg %d of args=0x%p and arg=0x%p--- \n",index,args,arg);
      dumpBufferToStream((char*)arg,8,out);
      fprintf(out,"arg data\n");
      dumpBufferToStream((char*)arg->address,arg->length,out);
    }
    index++;
  } while (index < invocation->argCount && index < 30);
  char *arg0 = copiedArgs[0];
  int status = 0;
  if (!strcmp(arg0,"setTraceLevel")){
    status = rexxSetTraceLevel(mgr,invocation,invocation->argCount,copiedArgs);
  } else if (!strcmp(arg0,"addConfig")){
    fprintf(out,"rexxcmgr chose addConfig\n");
    status = rexxAddConfig(mgr,invocation,invocation->argCount,copiedArgs);
  } else if (!strcmp(arg0,"setConfigPath")){
    status = rexxSetConfigPath(mgr,invocation,invocation->argCount,copiedArgs);
  } else if (!strcmp(arg0,"loadSchemas")){
    status = rexxLoadSchemas(mgr,invocation,invocation->argCount,copiedArgs);
  } else if (!strcmp(arg0,"loadConfiguration")){
    status = rexxLoadConfiguration(mgr,invocation,invocation->argCount,copiedArgs);
  } else if (!strcmp(arg0,"setParmlibMemberName")){
    status = rexxSetParmlibMemberName(mgr,invocation,invocation->argCount,copiedArgs);
  } else if (!strcmp(arg0,"getConfigData")){
    status = rexxGetConfigData(mgr,invocation,invocation->argCount,copiedArgs);
  } else if (!strcmp(arg0,"validate")){
    status = rexxValidate(mgr,invocation,invocation->argCount,copiedArgs);
  } else{
    status = ZCFG_UNKNOWN_REXX_FUNCTION;
  }
  SLHFree(argHeap);
  IRXEVALB *evalb = (IRXEVALB*)invocation->evalBlock;
  evalb->evlen = sprintf(evalb->evdata,"%d",status);
  if (mgr->traceLevel >= 1){
    fprintf(out,"returning evalblock at 0x%p\n",evalb);
    dumpBufferToStream((char*)evalb,0x40,out);
  }
  return status;
}

int REXXCMGR(REXXInvocation *invocation){
  if (!theConfigManager){
    LoggingContext *logContext = makeLoggingContext();
    logConfigureStandardDestinations(logContext);
    theConfigManager = makeConfigManager();
    if (invocation->traceLevel >= 1){
      printf("configmgr made at 0x%p\n",theConfigManager);
    }
    theConfigManager->traceOut = fopen("/u/zossteam/jdevlin/git2022/zss/deps/zowe-common-c/tests/rexxcmgr.txt","w");
  }
  FILE *out = theConfigManager->traceOut;
  if (invocation->traceLevel >= 1){
    printf("called rexxcmgr, cmgr=0x%p invocation=0x%p\n",
	   theConfigManager,invocation);
    fprintf(out,"REXXCMGR call (Mk2)\n");
  }
  return rexxEntry(theConfigManager,invocation);
}
