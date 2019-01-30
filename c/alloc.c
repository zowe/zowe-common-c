

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
#include <metal/stdlib.h>
#include <metal/string.h>
#include "qsam.h"
#include "metalio.h"
#else 
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#endif

#include "copyright.h"
#include "zowetypes.h"
#include "alloc.h"
/* #include "utils.h" */

static int MALLOC_TRACE_LEVEL = 0;

#ifdef MALLOC_ABEND_ENABLED

typedef void abend_os_fn(int, int);
#pragma linkage(abend_os_fn, OS)

static unsigned short abendCode[] = {
#if __64BIT__
  0xEBEC, 0xD008, 0x0024,  /* STMG 14,12,8(13) */
  0xE3F0, 0x1008, 0x0004,  /* LG  15,8(0,R1)  */
  0xE310, 0x1000, 0x0004,  /* LG  1,0(0,R1)  */
#else
  0x90EC, 0xD00C, /* STM 14,12,12(13) */
  0x58F0, 0x1004, /* L R15,4(0,R1)    */
  0x5810, 0x1000, /* L R1,0(0,R1)     */
#endif
  0x58F0, 0xF000, /* L R15,0(0,R15)   */
  0x5810, 0x1000, /* L R1,0(0,R1)     */
  0x4100, 0x0000, /* LA R0,0          */
  0x0A0D};        /* SVC X'0D'        */

static void abend(int requestedAbendCode, int requestedReasonCode){
  ((abend_os_fn*)abendCode)((0x04 << 24 /* reasonCode is specified */) | requestedAbendCode, requestedReasonCode);
}

#define MALLOC_ABEND_CODE 1000
#define MALLOC_ABEND_REASON 0
#endif 

#ifdef __ZOWE_OS_ZOS
 
static char *getmain31(int size, int subpool){
  char *data;
  __asm(ASM_PREFIX
        " STORAGE OBTAIN,LENGTH=(%1),SP=(%2),LOC=31,CALLRKY=YES\n"  /* ,KEY=8 */
	" LGR %0,1" :
	"=r"(data) :
	"r"(size), "r"(subpool));
  return data;
}

static char *getmain31Key8(int size, int subpool){
  char *data;
  __asm(ASM_PREFIX
        " STORAGE OBTAIN,LENGTH=(%1),SP=(%2),LOC=31,KEY=8\n"
	" LGR %0,1" :
	"=r"(data) :
	"r"(size), "r"(subpool));
  return data;
}

static char *getmain31Key0(int size, int subpool){
  char *data;
  __asm(ASM_PREFIX
        " STORAGE OBTAIN,LENGTH=(%1),SP=(%2),LOC=31,KEY=0\n"
	" LGR %0,1" :
	"=r"(data) :
	"r"(size), "r"(subpool));
  return data;
}

static char *getmain31Key2(int size, int subpool){
  char *data;
  __asm(ASM_PREFIX
        " STORAGE OBTAIN,LENGTH=(%1),SP=(%2),LOC=31,KEY=2\n"
	" LGR %0,1" :
	"=r"(data) :
	"r"(size), "r"(subpool));
  return data;
}

static int freemain31(char *data, int size, int subpool){
  __asm(ASM_PREFIX
        " STORAGE RELEASE,LENGTH=(%0),SP=(%1),ADDR=(%2),CALLRKY=YES\n"  /* ,KEY=8 */
	: :
	"r"(size), "r"(subpool), "r"(data));
  return 1;
}

static int freemain31Key8(char *data, int size, int subpool){
  __asm(ASM_PREFIX
        " STORAGE RELEASE,LENGTH=(%0),SP=(%1),ADDR=(%2),KEY=8"
	: :
	"r"(size), "r"(subpool), "r"(data));
  return 1;
}

static int freemain31Key0(char *data, int size, int subpool){
  __asm(ASM_PREFIX
        " STORAGE RELEASE,LENGTH=(%0),SP=(%1),ADDR=(%2),KEY=0"
	: :
	"r"(size), "r"(subpool), "r"(data));
  return 1;
}

static int freemain31Key2(char *data, int size, int subpool){
  __asm(ASM_PREFIX
        " STORAGE RELEASE,LENGTH=(%0),SP=(%1),ADDR=(%2),KEY=2"
	: :
	"r"(size), "r"(subpool), "r"(data));
  return 1;
}

char *allocECSA(int size, int key){
  switch (key){
  case 0:
    return getmain31Key0(size,241);
  case 2:
    return getmain31Key2(size,241);
  default:
    printf("PANIC cannot allocate ECSA in key=%d\n",key);
    return NULL;
  }
}

int freeECSA(char *data, int size, int key){
  switch (key){
  case 0:
    return freemain31Key0(data,size,241);
  case 2:
    return freemain31Key2(data,size,241);
  default:
    printf("PANIC cannot free ECSA in key=%d\n",key);
    return 12;
  }

  return freemain31Key0(data,size,241);
}

#endif /* ZOWE_OS_ZOS */


#ifdef METTLE

/* MVS Programming: Extended Addressability Guide: http://publibz.boulder.ibm.com/epubs/pdf/iea2a540.pdf
 * GETMAIN/FREEMAIN and STORAGE do not work on virtual storage above the bar.
 * Using the IARV64 macro, a program can create and free a memory object and manage the physical frames that back the virtual storage. */
/* Size is specified in megabytes. */
static char* __ptr64 getmain64(long long sizeInMegabytes, int *returnCode, int *reasonCode){
  
  char* __ptr64 data = NULL;
  
  int macroRetCode = 0;
  int macroResCode = 0;

  __asm("IARVPLST IARV64 MF=L\n" : "XL:DS:1000"(IARVPLST));

  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SEGMENTS=(%3),ORIGIN=(%2),TTOKEN=NO_TTOKEN,"
        "RETCODE=%0,RSNCODE=%1,MF=(E,(%4))\n" :
        "=m"(macroRetCode), "=m"(macroResCode): 
        "r"(&data),"r"(&sizeInMegabytes),"r"(&IARVPLST));

   if (returnCode) *returnCode = macroRetCode;
   if (reasonCode) *reasonCode = macroResCode;
  
   return data;

}

#ifdef _LP64
static char* __ptr64 getmain64ByToken(long long sizeInMegabytes, long long token, int *returnCode, int *reasonCode){
  
  char* __ptr64 data = NULL;
  
  int macroRetCode = 0;
  int macroResCode = 0;
  
   __asm("IARVPLST IARV64 MF=L\n" : "XL:DS:1000"(IARVPLST));
  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SEGMENTS=(%3),ORIGIN=(%2),TTOKEN=NO_TTOKEN,"
        "RETCODE=%0,USERTKN=(%5),RSNCODE=%1,MF=(E,(%4))\n" :
        "=m"(macroRetCode), "=m"(macroResCode): 
        "r"(&data),"r"(&sizeInMegabytes),"r"(&IARVPLST), "r"(&token));

  if (returnCode) *returnCode = macroRetCode;
  if (reasonCode) *reasonCode = macroResCode;
  
  return data;
  
}

static void freemain64(char* __ptr64 data, int *returnCode, int *reasonCode){
  
  int macroRetCode = 0;
  int macroResCode = 0;
  
  long long address = (long long)&data;

  __asm("IARVPLST IARV64 MF=L\n" : "XL:DS:1000"(IARVPLST));

  __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),RETCODE=%0,RSNCODE=%1,"
        "MF=(E,(%3))\n":
        "=m"(macroRetCode), "=m"(macroResCode) :
        "r"(&data), "r"(&IARVPLST));
  if (returnCode) *returnCode = macroRetCode;
  if (reasonCode) *reasonCode = macroResCode;
  
}
#endif /* END OF LP64 */

static void freemain64ByToken(long long token, int *returnCode, int *reasonCode){
  
  int macroRetCode = 0;
  int macroResCode = 0;
  
  __asm("IARVPLST IARV64 MF=L\n" : "XL:DS:1000"(IARVPLST));

  __asm(" IARV64 REQUEST=DETACH,COND=YES,MATCH=USERTOKEN,USERTKN=(%2),RETCODE=%0,RSNCODE=%1,"
        "MF=(E,(%3))\n":
        "=m"(macroRetCode), "=m"(macroResCode) :
        "r"(&token), "r"(&IARVPLST));
 
  if (returnCode) *returnCode = macroRetCode;
  if (reasonCode) *reasonCode = macroResCode;
  
}

#endif /* END of METTLE */

/* The following is a memory tracking facility for hardcore memory debugging.
   This code needs to be recompiled to turn it on */


#if ((!METTLE || WRITSTAT) && TRACK_MEMORY)
#define ALLOCATIONS_TO_TRACK 25000


int safeBytes = 0;
int rawBytes = 0;
int k8Bytes = 0;
static int allocationsTracked = 0;

static char *allocations[ALLOCATIONS_TO_TRACK];
static char *allocationNames[ALLOCATIONS_TO_TRACK];
static int   allocationLengths[ALLOCATIONS_TO_TRACK];

int showOutstanding(){
  int i;

  printf("total allocations done = %d\n",allocationsTracked);
/*  dumpbuffer ((char *) allocations, 1024);*/
  for (i=0; i<allocationsTracked; i++){
    char *ptr = allocations[i];
    if ((ptr != NULL) &&
        (ptr != (char*)(-1)))
    {
      printf("never freed allocNum=%d, size=%d at=0x%x '%s'\n",i,allocationLengths[i],ptr,allocationNames[i]);
    } else if (0 != allocationLengths[i])
    {
      printf("%d outstanding bytes in NULL or free'd slot alloc=%d at=0x%x '%s'\n",
              allocationLengths[i],i,ptr,allocationNames[i]);
    }
  }
  return allocationsTracked;
}

static void trackAllocation(char *ptr, char *name, int length){
  if (allocationsTracked >= ALLOCATIONS_TO_TRACK) {
    return;
  }
  if (allocationsTracked == 0){
    int i;
    for (i=0; i<ALLOCATIONS_TO_TRACK; i++){
      allocations[i] = NULL;
      allocationNames[i] = NULL;
      allocationLengths[i] = 0;
    }
  }

  allocations[allocationsTracked] = ptr;
  allocationNames[allocationsTracked] = name;
  allocationLengths[allocationsTracked] = length;

  allocationsTracked++;
}

static void trackFree(char *ptr, int length){
  int i;
  for (i=0; i<allocationsTracked; i++){
    if (allocations[i] == ptr) {
      if ((length != allocationLengths[i]) &&
          ((272 == length) || (296 == length)))
      {
        printf("bad attempt to free 272 or 296 at addr: %p\n", ptr);
        /* memset((void*)0, 0x0, 1);*/ /* cause a crash */
      }
      allocations[i] = (char*)(-1);
      allocationLengths[i] = allocationLengths[i] - length;
    }
  }
}
#endif

char *malloc31(int size){
#ifdef TRACK_MEMORY
  rawBytes += size;
#endif
  char *res = NULL;
#if defined( METTLE )
  res = getmain31(size,SUBPOOL);
#elif defined(__ZOWE_OS_WINDOWS) || defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  res = (char*)malloc(size);
#else 
  res = __malloc31(size);
#endif
  
#ifdef TRACK_MEMORY
  trackAllocation(res, "not named", size);
#endif
  return res;
}

void free31(void *data, int size){
#ifdef TRACK_MEMORY
  rawBytes -= size;
  trackFree((char*)data, size);
#endif
#if defined( METTLE )
  freemain31(data,size,SUBPOOL);
#elif defined ( __ZOWE_OS_WINDOWS ) || defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  free(data);
#else 
  free(data);
#endif
}

#ifdef __ZOWE_OS_ZOS
static void *systemStorageObtain31(int size, int subpool){
  char *data;
  __asm(ASM_PREFIX
        " STORAGE OBTAIN,LENGTH=(%1),SP=(%2),LOC=31,CALLRKY=YES,LINKAGE=SYSTEM\n"  /* ,KEY=8 */
	" LGR %0,1" :
	"=r"(data) :
	"r"(size), "r"(subpool));
  return data;
}
#endif

#ifndef BIG_MALLOC_THRESHOLD
#define BIG_MALLOC_THRESHOLD   0x00A00000  /* approximately  10 megs */
#endif
#ifndef BIG_MALLOC64_THRESHOLD
#define BIG_MALLOC64_THRESHOLD 0x20000000  /* approximately 512 megs */
#endif

#define TCB_EXISTS (*((void**)0x21C))

char *safeMalloc(int size, char *site){
  char *res = NULL;
  if (size > BIG_MALLOC_THRESHOLD){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: big alloc coming %d from %s\n",size,site);
    }
  } else if (size == 0){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: zero alloc from %s\n",site);
    }
  } else if (size < 0){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: negative malloc %d from %s\n",size,site);
    }
  }
#if defined ( METTLE )
  res = getmain31(size,SUBPOOL);
#elif defined ( __ZOWE_OS_WINDOWS ) || defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  res = (char*)malloc(size);
#else
  if (TCB_EXISTS){
    res = malloc31(size);
  } else{
    int subpool = 0;
    res = systemStorageObtain31(size,subpool);
  }
#endif
  if (res != NULL){
    memset(res,0,size);
  } else{
#ifdef MALLOC_ABEND_ENABLED
    if (size > 0) {
      abend(MALLOC_ABEND_CODE, MALLOC_ABEND_REASON);
    }
#endif
    printf("MALLOC failed, got NULL for size %d at site %s\n",size, site);
  }
#ifdef TRACK_MEMORY
  safeBytes += size;
  trackAllocation(res,site,size);
#endif
  return res;
}

char *safeMalloc2(int size, char *site, int *indicator){
  char *res = NULL;
  if (size > BIG_MALLOC_THRESHOLD){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: big alloc coming %d from %s\n",size,site);
    }
  } else if (size == 0){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: zero alloc from %s\n",site);
    }
  } else if (size < 0){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: negative malloc %d from %s\n",size,site);
    }
  }
#if defined ( METTLE )
  res = getmain31(size,SUBPOOL);
#elif defined (__ZOWE_OS_WINDOWS) || defined (__ZOWE_OS_LINUX)|| defined(__ZOWE_OS_AIX)
  res = (char*)malloc(size);
#else
  if (TCB_EXISTS){
    res = malloc31(size);
  } else{
    int subpool = 0;
    *indicator = 0x881;
    res = systemStorageObtain31(size,subpool);
    *indicator = 0x882; 
  }
#endif
  if (res != NULL){
    *indicator = 0x883;
    memset(res,0,size);
    *indicator = 0x884;
  } else{
    printf("MALLOC failed, got NULL for size %d at site %s\n",size, site);
  }
#ifdef TRACK_MEMORY
  safeBytes += size;
  trackAllocation(res,site,size);
#endif
  *indicator = 0x886;
  return res;
}

char *safeMalloc31(int size, char *site){
  /* printf("allocsize=%d site=%s\n",size,site); */
  char *res = NULL;
  if (size > BIG_MALLOC_THRESHOLD){ /* 1 meg, roughly */
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: big alloc coming %d from %s\n",size,site);
    }
  } else if (size == 0){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: zero alloc from %s\n",site);
    }
  } else if (size < 0){
    if (MALLOC_TRACE_LEVEL >= 1){
      printf("MALLOC: negative malloc %d from %s\n",size,site);
    }
  }
#ifdef METTLE 
  res = getmain31(size,SUBPOOL);
#else
  res = malloc31(size);
#endif
  if (res != NULL){
    memset(res,0,size);
  } else{
    printf("MALLOC failed, got NULL for size %d at site %s\n",size, site);
  }
#ifdef TRACK_MEMORY
  safeBytes += size;
  trackAllocation(res,site,size);
#endif
  return res;
}

static char *safeMalloc64Internal(int size, char *site, long long token){
  /* printf("allocsize=%d site=%s\n",size,site); */
  char *res = NULL;
  int sizeInMegabytes = 0;

  if (size > BIG_MALLOC64_THRESHOLD){ /* 1 meg, roughly */
    printf("MALLOC: big alloc coming %d from %s\n",size,site);
  } else if (size == 0){
    printf("MALLOC: zero alloc from %s\n",site);
  } else if (size < 0){
    printf("MALLOC: negative malloc %d from %s\n",size,site);
  }


  if ((size & 0xFFFFF) == 0){
    sizeInMegabytes = size >> 20;
  } else{
    sizeInMegabytes = (size >> 20) + 1;
  }
#if defined(METTLE) && defined(_LP64)
  res = getmain64((long long)sizeInMegabytes,NULL,NULL); 
#elif defined(_LP64)    /* LE case */
  res = malloc(size);   /* According to Redpaper http://www.redbooks.ibm.com/redpapers/pdfs/redp9110.pdf allocated above bar */
#else
  res = NULL;  /* Invalid if not compiled _LP64 */
#endif
  if (res != NULL){
    memset(res,0,size);
  } else{
    printf("MALLOC failed, got NULL for size %d at site %s\n",size, site);
  }
#ifdef TRACK_MEMORY
  safeBytes += size;
  trackAllocation(res,site,size);
#endif
  return res;
}

char *safeMalloc64(int size, char *site){
  return safeMalloc64Internal(size,site,0);
}

char *safeMalloc64ByToken(int size, char *site, long long token){
  return safeMalloc64Internal(size,site,token);
}

char *safeMalloc31Key8(int size, char *site){
  /* printf("allocsize=%d site=%s\n",size,site); */
#ifdef TRACK_MEMORY
  k8Bytes += size;
#endif
  char *res = NULL;
  if (size > BIG_MALLOC_THRESHOLD){ /* 10 meg, roughly */
    printf("MALLOC: big alloc coming %d from %s\n",size,site);
  } else if (size == 0){
    printf("MALLOC: zero alloc from %s\n",site);
  } else if (size < 0){
    printf("MALLOC: negative malloc %d from %s\n",size,site);
  }
#ifdef METTLE 
  res = getmain31Key8(size,SUBPOOL);
#else
  res = malloc31(size);
#endif
  if (res != NULL){
    memset(res,0,size);
  } else{
    printf("MALLOC failed, got NULL for size %d at site %s\n",size, site);
  }
  return res;
}


void safeFree31(char *data, int size){
/*   printf("safeFree31: data 0x%p, size %d\n",data, size); */
#ifdef TRACK_MEMORY
  safeBytes -= size;
  trackFree(data,size);
#endif

#ifdef METTLE 
  freemain31(data,size,SUBPOOL);
#else
  free(data);
#endif
}

static void safeFree64Internal(char *data, int size, long long token){
/*   printf("safeFree31: data 0x%p, size %d\n",data, size); */
#ifdef TRACK_MEMORY
  safeBytes -= size;
  trackFree(data,size);
#endif

#if defined(METTLE) && defined(_LP64)
  freemain64(data,NULL,NULL);
#else
  /* do nothing - because 64 bit allocation in LE is not ready */
#endif
}

void safeFree64(char *data, int size){
  safeFree64Internal(data,size,0);
}

void safeFree64ByToken(char *data, int size, long long token){
  safeFree64Internal(data,size,token);
}

void safeFree31Key8(char *data, int size){
#ifdef TRACK_MEMORY
  k8Bytes -= size;
#endif

/*   printf("safeFree31: data 0x%p, size %d\n",data, size); */
#ifdef METTLE 
  freemain31(data,size,SUBPOOL);
#else
  free(data);
#endif
}

void safeFree(char *data, int size){
  safeFree31(data,size);
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

