

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include "zowetypes.h"

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include "metalio.h"
#include "qsam.h"
#else
#include <stdio.h>
#ifdef __ZOWE_OS_ZOS
#include <ctest.h>
#include <dynit.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#endif

#include "copyright.h"
#include "utils.h"
#include "collections.h"
#include "alloc.h"
#include "le.h"

#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif

#ifdef __ZOWE_OS_ZOS

LibraryFunction libraryFunctionTable[LIBRARY_FUNCTION_COUNT]
={
#ifndef METTLE
   { "atof",    (void*)atof,     NULL, NULL},
   { "atoi",    (void*)atoi,     NULL, NULL},
   { "bsearch", (void*)bsearch,  NULL, NULL},
   { "chmod",   (void*)chmod,    NULL, NULL},
   { "chown",   (void*)chown,    NULL, NULL},
   { "dynalloc",(void*)dynalloc, NULL, NULL},
   { "fclose",  (void*)fclose,   NULL, NULL},
   { "feof",    (void*)feof,     NULL, NULL},
   { "fgetc",   (void*)fgetc,    NULL, NULL},
   { "fgets",   (void*)fgets,    NULL, NULL},
   { "fopen",   (void*)fopen,    NULL, NULL},
   { "fprintf", (void*)fprintf,  NULL, NULL},
   { "fread",   (void*)fread,    NULL, NULL},
   { "fseek",   (void*)fseek,    NULL, NULL},
   { "fwrite",  (void*)fwrite,   NULL, NULL},
   { "malloc",  (void*)malloc,   NULL, NULL},
   { "memchr",  (void*)memchr,   NULL, NULL},
   { "memcmp",  (void*)memcmp,   NULL, NULL},
   { "memcpy",  (void*)memcpy,   SRBCLEAN, SRBCLEAN },
   { "memmove", (void*)memmove,  NULL, NULL},
   { "memset",  (void*)memset,   SRBCLEAN, SRBCLEAN },
   { "printf",  (void*)printf,   NULL, NULL},
   { "qsort",   (void*)qsort,    NULL, NULL},
   { "strcmp",  (void*)strcmp,   NULL, NULL},
   { "strcpy",  (void*)strcpy,   NULL, NULL},
   { "strlen",  (void*)strlen,   SRBCLEAN, SRBCLEAN },
   { "strspn",   (void*)strspn,   NULL, NULL},
   { "strstr",  (void*)strstr,   NULL, NULL},
   { "strtok",  (void*)strtok,   NULL, NULL},
   { "strtol",  (void*)strtol,   NULL, NULL},
   { "tan",     (void*)tan,      NULL, NULL},
#endif
};

char *getCAA(){
  char *realCAA = NULL;

#if !defined(METTLE) && defined(_LP64)
  char *laa = *(char * __ptr32 * __ptr32)0x04B8;
  char *lca = *(char **)(laa + 88);
  realCAA = *(char **)(lca + 8);
#else
  __asm(
      ASM_PREFIX
      "         LA    %0,0(,12) \n"
      : "=r"(realCAA)
      :
      :
  );
#endif

  return realCAA;
}

#ifndef LE_MAX_SUPPORTED_ZOS
#define LE_MAX_SUPPORTED_ZOS 0x01020500u
#endif

void abortIfUnsupportedCAA() {
#ifdef __ZOWE_OS_ZOS
  ECVT *ecvt = getECVT();
  unsigned int zosVersion = ecvt->ecvtpseq;
#ifndef METTLE
  if (zosVersion > LE_MAX_SUPPORTED_ZOS) {
    printf("error: z/OS version = 0x%08X, max supported version = 0x%08X - "
           "CAA fields require verification\n", zosVersion, LE_MAX_SUPPORTED_ZOS);
    abort();
  }
#else
  /* Metal uses its own copy of CAA, reserved fields will always be available */
#endif /* METTLE */
#endif /* __ZOWE_OS_ZOS */
}

char *makeFakeCAA(char *stackArea, int stackSize){
  char *fakeCAA = safeMalloc(CAA_SIZE,"Fake CAA");
  char *realCAA = getCAA();

  /* includes run time library and other stuff */
  int copyStart = 0x1F0;
  int copyEnd = 0x220;
  void **runtimeLibraryTable = ((CAA*)realCAA)->runtimeLibraryVectorTable;
  void *mallocCode = runtimeLibraryTable[0x1EC/4];
  memset(fakeCAA,0,CAA_SIZE);

  memcpy(fakeCAA+copyStart,realCAA+copyStart,copyEnd-copyStart);
  /* move the top of stack indicator */
  *((int*)(fakeCAA+0x314)) = (int)(stackArea + stackSize);
  return fakeCAA;
}

static LibraryFunction *findLibraryFunction(int rtlVectorOffset){
  for (int i=0; i<LIBRARY_FUNCTION_COUNT; i++){
    LibraryFunction *function = &libraryFunctionTable[i];
    int *intArray = (int*)function->pointer;
    int offset = 0xFFFFFFFF;
    if ((intArray[0] == 0x58F0C210) &&
        ((intArray[1]&0xFFFFF000) == 0x58F0F000) &&
        (intArray[2] == 0x07FF0000)){
      offset = 0xfff & intArray[1];
      if (offset == rtlVectorOffset){
        return function;
      }
    }
  }
  return NULL;
}

#define ESTIMATED_RTL_VECTOR_SIZE 0xB00

void showRTL(){
  CAA *caa = (CAA*)getCAA();
  void **rtlVector = caa->runtimeLibraryVectorTable;
  printf("RTL Vector at 0x%x\n",rtlVector);
  dumpbuffer((char*)rtlVector,ESTIMATED_RTL_VECTOR_SIZE);
  int estimatedEntries = ESTIMATED_RTL_VECTOR_SIZE / 4;
  for (int i=2; i<estimatedEntries; i++){
    char *stub = rtlVector[i];
    printf("i = %d offset=0x%03x at 0x%x\n",i,i*sizeof(int),stub);
    dumpbuffer(stub,0x40);

    int offset = i * 4;
    LibraryFunction *function = findLibraryFunction(offset);
    if (function){
      printf("FOUND %s\n",function->name);
    }
  }
}

#endif /* __ZOWE_OS_ZOS */


RLEAnchor *makeRLEAnchor(){
  RLEAnchor *anchor = (RLEAnchor*)safeMalloc31(sizeof(RLEAnchor),"RLEAnchor");
  memset(anchor,0,sizeof(RLEAnchor));
  memcpy(anchor->eyecatcher,"RLEANCHR",8);

#ifdef __ZOWE_OS_ZOS

  /* non METTLE/METAL assumuption here */

#ifdef _LP64
  anchor->flags = RLE_RTL_64 | RLE_RTL_XPLINK;
#else
  anchor->flags = 0;
#endif

#ifdef METTLE
  CAA *caa = (CAA*)safeMalloc31(sizeof(CAA),"METTLE CAA");
  anchor->mainTaskCAA = caa;
  anchor->masterRTLVector = NULL; /* we don't have one of these, yet */
#else
  CAA *caa = (CAA*)getCAA();
  anchor->mainTaskCAA = caa;
  anchor->masterRTLVector = caa->runtimeLibraryVectorTable;
#endif

#endif /* __ZOWE_OS_ZOS */

  return anchor;
}

void deleteRLEAnchor(RLEAnchor *anchor) {

#ifdef METTLE
  safeFree31((char *)anchor->mainTaskCAA, sizeof(CAA));
  anchor->mainTaskCAA = NULL;
#endif

  safeFree31((char *)anchor, sizeof(RLEAnchor));
  anchor = NULL;

}

#ifndef METTLE
void establishGlobalEnvironment(RLEAnchor *anchor){
  /* do nothing */
}
void returnGlobalEnvironment(void){
  /* do nothing */
}
#endif


#ifndef __ZOWE_OS_ZOS /* The z/OS implementation is in scheduling.c */
RLETask *makeRLETask(RLEAnchor *anchor,
                     int taskFlags,
                     int functionPointer(RLETask *task)){
  RLETask *task = (RLETask*)safeMalloc31(sizeof(RLETask),"RTSK");
  memset(task,0,sizeof(RLETask));
  memcpy(task->eyecatcher,"RTSK",4);
  task->flags = taskFlags;
  task->anchor = anchor;
  return task;
}

void deleteRLETask(RLETask *task) {
  safeFree31((char *)task, sizeof(RLETask));
  task = NULL;
}
#endif

#ifdef __ZOWE_OS_ZOS
void initRLEEnvironment(RLEAnchor *anchor) {

  establishGlobalEnvironment(anchor);
  RLETask *task = makeRLETask(anchor, 0, NULL);
  returnGlobalEnvironment();
  abortIfUnsupportedCAA();
  CAA *caa = (CAA *)getCAA();
  caa->rleTask = task;

  int recoveryRC = recoveryEstablishRouter(RCVR_ROUTER_FLAG_NONE);
  if (recoveryRC != RC_RCV_OK) {
    printf("le.c: error - recovery router not established\n");
  }

}

void termRLEEnvironment() {

  recoveryRemoveRouter();

  CAA *caa = (CAA *)getCAA();
  deleteRLETask(caa->rleTask);

}
#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

