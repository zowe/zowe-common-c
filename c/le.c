

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
#include <stdarg.h>

#ifndef __ZOWE_OS_WINDOWS
#include <strings.h>
#include <unistd.h>
#endif

#include <sys/stat.h>

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
   /*   { "memchr",  (void*)memchr,   NULL, NULL}, */
   /*   { "memcmp",  (void*)memcmp,   NULL, NULL}, */
   /*   { "memcpy",  (void*)memcpy,   SRBCLEAN, SRBCLEAN }, */
   { "memmove", (void*)memmove,  NULL, NULL},
   /* { "memset",  (void*)memset,   SRBCLEAN, SRBCLEAN }, */
   { "printf",  (void*)printf,   NULL, NULL},
   { "qsort",   (void*)qsort,    NULL, NULL},
   /* { "strcmp",  (void*)strcmp,   NULL, NULL}, */
   /* { "strcpy",  (void*)strcpy,   NULL, NULL}, */
   /* { "strlen",  (void*)strlen,   SRBCLEAN, SRBCLEAN },*/
   { "strspn",   (void*)strspn,   NULL, NULL},
   { "strstr",  (void*)strstr,   NULL, NULL},
   { "strtok",  (void*)strtok,   NULL, NULL},
   { "strtol",  (void*)strtol,   NULL, NULL},
   { "tan",     (void*)tan,      NULL, NULL},
#endif
};

char *getCAA(void){
  char *realCAA = NULL;

#if !defined(METTLE) && defined(_LP64)
  char *laa = *(char * __ptr32 * __ptr32)0x04B8;
  char *lca = *(char **)(laa + 88);
  realCAA = *(char **)(lca + 8);
#else
  __asm(
      ASM_PREFIX
      "         LA    %0,0(,12)\n"
      : "=r"(realCAA)
      :
      :
  );
#endif

  return realCAA;
}

/* ---- Language Environment control blocks beyond the CAA ------------------

   The runtime options in effect for this enclave live in LE's Options
   Control Block (OCB): CAA -> EDB (CEECAAEDB) -> OCB (CEEEDBOPTCB). The
   offsets below were computed by HLASM from the CEE.SCEEMAC mappings
   (CEECAA, CEEEDB, CEEOCB; SYSSTATE AMODE64=YES selects the 64-bit CAA and
   EDB forms, the OCB layout is the same in both modes) and verified on
   z/OS 3.1 against live heap behaviour by tests/le-options. The OCB is
   described in the LE Vendor Interfaces book.

   Three things the mappings do not say:
   - the 64-bit LE's OCB eyecatcher is 'CELQOCB', not 'CEEOCB';
   - CEEOCB_*_SUB_OPTIONS is declared `DS A` but holds an offset from the
     start of the OCB, not an address (following it as one takes an 0C4);
   - HEAPZONES has no ON bit; its flag byte reads X'01' regardless, and the
     option is in effect when the size in its sub-options is nonzero.

   Eyecatchers are compared as EBCDIC bytes so this works in a source file
   compiled in ASCII char mode too. */

#ifndef METTLE

#ifdef _LP64
#define LE_CAA_EDB                0x388  /* CEECAAEDB    AD(EDB)      */
#define LE_CAA_SELF               0x3A0  /* CEECAAPTR    AD(this CAA) */
#define LE_EDB_OPTCB              0x110  /* CEEEDBOPTCB  AD(OCB)      */
#else
#define LE_CAA_EDB                0x2F0  /* CEECAAEDB    A(EDB)       */
#define LE_CAA_SELF               0x2FC  /* CEECAAPTR    A(this CAA)  */
#define LE_EDB_OPTCB              0x010  /* CEEEDBOPTCB  A(OCB)       */
#endif
#define LE_EDB_EYE                0x000  /* CEEEDBEYE          XL8 'CEEEDB  ' */
#define LE_OCB_EYE                0x000  /* CEEOCB_EYECATCHER  CL8            */
#define LE_OCB_LENGTH             0x00A  /* CEEOCB_LENGTH      H              */
#define LE_OCB_HEAPPOOLS          0x1E4  /* CEEOCB_HEAPPOOLS_BIT_FLAG         */
#define LE_OCB_HEAPPOOLS64        0x20C  /* CEEOCB_HEAPPOOLS64_BIT_FLAG       */
#define LE_OCB_HEAPZONES          0x24C  /* CEEOCB_HEAPZONES_BIT_FLAG         */
#define LE_OCB_HEAPZONES_SUBOPTS  0x250  /* CEEOCB_HEAPZONES_SUB_OPTIONS      */
#define LE_OCB_OPTION_ON          0x80   /* CEEOCB_*_ON                       */
#define LE_OCB_WHERE_SET          2      /* CEEOCB_*_WHERE_SET: H, two bytes after the flag */
#define LE_OCB_HEAPZONES_SIZE31   0x04   /* CEEOCB_HEAPZONES_SIZE31, in the sub-options */
#define LE_OCB_HEAPZONES_SIZE64   0x0C   /* CEEOCB_HEAPZONES_SIZE64             */

static const unsigned char LE_EYE_CEEEDB[6]  = {0xC3,0xC5,0xC5,0xC5,0xC4,0xC2};      /* CEEEDB  */
static const unsigned char LE_EYE_CEEOCB[6]  = {0xC3,0xC5,0xC5,0xD6,0xC3,0xC2};      /* CEEOCB  */
static const unsigned char LE_EYE_CELQOCB[7] = {0xC3,0xC5,0xD3,0xD8,0xD6,0xC3,0xC2}; /* CELQOCB */

char *getEDB(void){
  char *caa = getCAA();
  if (caa == NULL || *(char **)(caa + LE_CAA_SELF) != caa) {
    return NULL;
  }
  char *edb = *(char **)(caa + LE_CAA_EDB);
  if (edb == NULL ||
      memcmp(edb + LE_EDB_EYE, LE_EYE_CEEEDB, sizeof(LE_EYE_CEEEDB)) != 0) {
    return NULL;
  }
  return edb;
}

char *getOCB(void){
  char *edb = getEDB();
  if (edb == NULL) {
    return NULL;
  }
  char *ocb = *(char **)(edb + LE_EDB_OPTCB);
  if (ocb == NULL) {
    return NULL;
  }
  if (memcmp(ocb + LE_OCB_EYE, LE_EYE_CELQOCB, sizeof(LE_EYE_CELQOCB)) != 0 &&
      memcmp(ocb + LE_OCB_EYE, LE_EYE_CEEOCB,  sizeof(LE_EYE_CEEOCB))  != 0) {
    return NULL;
  }
  return ocb;
}

/* A CEEOCB_*_SUB_OPTIONS field: an offset within the OCB when it is smaller
   than the OCB's own length, otherwise taken as a 31-bit address. */
static char *getOCBSubOptions(char *ocb, int fieldOffset){
  unsigned int ocbLength = *(unsigned short *)(ocb + LE_OCB_LENGTH);
  unsigned int ref = *(unsigned int *)(ocb + fieldOffset);
  if (ref == 0) {
    return NULL;
  } else if (ref < ocbLength) {
    return ocb + ref;
  } else {
    return (char *)(unsigned long)(ref & 0x7FFFFFFFu);
  }
}

int getLEHeapOptions(LEHeapOptions *options){
  memset(options, 0, sizeof(*options));
  char *ocb = getOCB();
  if (ocb == NULL) {
    return -1;
  }
  unsigned char heapPoolsFlag   = *(unsigned char *)(ocb + LE_OCB_HEAPPOOLS);
  unsigned char heapPools64Flag = *(unsigned char *)(ocb + LE_OCB_HEAPPOOLS64);
  options->heapPools   = (heapPoolsFlag   & LE_OCB_OPTION_ON) != 0;
  options->heapPools64 = (heapPools64Flag & LE_OCB_OPTION_ON) != 0;
  options->heapPoolsWhereSet   = *(unsigned short *)(ocb + LE_OCB_HEAPPOOLS   + LE_OCB_WHERE_SET);
  options->heapPools64WhereSet = *(unsigned short *)(ocb + LE_OCB_HEAPPOOLS64 + LE_OCB_WHERE_SET);
  options->heapZonesWhereSet   = *(unsigned short *)(ocb + LE_OCB_HEAPZONES   + LE_OCB_WHERE_SET);
  char *zones = getOCBSubOptions(ocb, LE_OCB_HEAPZONES_SUBOPTS);
  if (zones != NULL) {
    options->heapZonesSize31 = *(unsigned int *)(zones + LE_OCB_HEAPZONES_SIZE31);
    options->heapZonesSize64 = *(unsigned int *)(zones + LE_OCB_HEAPZONES_SIZE64);
  }
  return 0;
}

#else /* METTLE: no Language Environment, so no EDB or OCB */

char *getEDB(void){
  return NULL;
}

char *getOCB(void){
  return NULL;
}

int getLEHeapOptions(LEHeapOptions *options){
  memset(options, 0, sizeof(*options));
  return -1;
}

#endif /* METTLE */

#ifndef LE_MAX_SUPPORTED_ZOS
#define LE_MAX_SUPPORTED_ZOS 0x01030200u
#endif

void abortIfUnsupportedCAA() {
#ifdef __ZOWE_OS_ZOS
  ECVT *ecvt = getECVT();
  unsigned int zosVersion = ecvt->ecvtpseq;
#ifndef METTLE
  if (zosVersion > LE_MAX_SUPPORTED_ZOS) {
    const char *continueWithWarning = getenv("ZWE_zowe_launcher_unsafeDisableZosVersionCheck");
    if (!strcmp(continueWithWarning, "true")) {
      /*
       * This code is context-free and sometimes the callers are expecting silent output on STDOUT
       * So, regrettably, if we want such a warning, we need to kick it up to calling code to manage.
       *
      printf("warning: z/OS version = 0x%08X, max supported version = 0x%08X - "
             "CAA fields require verification\n", zosVersion, LE_MAX_SUPPORTED_ZOS);
      */
    } else {
      printf("error: z/OS version = 0x%08X, max supported version = 0x%08X - "
             "CAA fields require verification\n", zosVersion, LE_MAX_SUPPORTED_ZOS);
      abort();
    }
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
  *((int*)(fakeCAA+0x314)) = (int)(uint64)(stackArea + stackSize);
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

void showRTL(void){
  CAA *caa = (CAA*)getCAA();
  void **rtlVector = caa->runtimeLibraryVectorTable;
  printf("RTL Vector at 0x%p\n",rtlVector);
  dumpbuffer((char*)rtlVector,ESTIMATED_RTL_VECTOR_SIZE);
  int estimatedEntries = ESTIMATED_RTL_VECTOR_SIZE / 4;
  for (int i=2; i<estimatedEntries; i++){
    char *stub = rtlVector[i];
    printf("i = %d offset=0x%03x at 0x%p\n",i,(int)(i*sizeof(int)),stub);
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
  memcpy(anchor->eyecatcher,RLE_ANCHOR_EYECATCHER,8);

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
#else
  CAA *caa = (CAA*)getCAA();
  anchor->mainTaskCAA = caa;
#endif

#endif /* __ZOWE_OS_ZOS */

  anchor->flags |= RLE_FLAGS_VERSIONED;
  anchor->version = RLE_ANCHOR_VERSION;
  anchor->size = sizeof(RLEAnchor);

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
  RLETask *task = (RLETask*)safeMalloc31(sizeof(RLETask),RLE_TASK_EYECATCHER);
  memset(task,0,sizeof(RLETask));
  memcpy(task->eyecatcher,RLE_TASK_EYECATCHER,4);
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

int setRLEApplicationAnchor(RLEAnchor *anchor, void *applicationAnchor) {
  if (!(anchor->flags & RLE_FLAGS_VERSIONED)) {
    return -1;
  }
  if (anchor->version < RLE_ANCHOR_VERSION_USER_APPL_ANCHOR_SUPPORT) {
    return -1;
  }
  anchor->userApplicationAnchor = applicationAnchor;
  return 0;
}

int getRLEApplicationAnchor(const RLEAnchor *anchor, void **applicationAnchor) {
  if (!(anchor->flags & RLE_FLAGS_VERSIONED)) {
    return -1;
  }
  if (anchor->version < RLE_ANCHOR_VERSION_USER_APPL_ANCHOR_SUPPORT) {
    return -1;
  }
  *applicationAnchor = anchor->userApplicationAnchor;
  return 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

