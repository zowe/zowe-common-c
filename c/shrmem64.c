

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#else
#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#endif

#include "shrmem64.h"
#include "zos.h"

#ifndef _LP64
/* #error ILP32 is not supported */
#endif

typedef uint64_t MemObj;

#define IARV64_V4PLIST_SIZE 160

static bool isIARV64OK(int iarv64RC) {
  return iarv64RC < 8;
}

static int makeRSN(int shrmem64RC, int iarv64RC, int iarv64RSN) {

  int rc = ((unsigned)shrmem64RC << 24) |
           ((unsigned)iarv64RC << 16) |
           ((iarv64RSN >> 8) & 0x0000FFFF);

  return rc;
}

#pragma pack(packed)

#define IARV64_REQUEST_GETSTOR   1
#define IARV64_REQUEST_GETSHARED 2
#define IARV64_REQUEST_DETACH  3
#define IARV64_REQUEST_PAGEFIX 4 
#define IARV64_REQUEST_PAGEUNFIX 5
#define IARV64_REQUEST_PAGEOUT 6
#define IARV64_REQUEST_DISCARDDATA 7
#define IARV64_REQUEST_PAGEIN 8
#define IARV64_REQUEST_PROTECT 9
#define IARV64_REQUEST_SHAREMEMOBJ 10
#define IARV64_REQUEST_CHANGEACCESS 11
#define IARV64_REQUEST_UNPROTECT 12
#define IARV64_REQUEST_CHANGEGUARD 13
#define IARV64_REQUEST_LIST 14             /* wtf is this */
#define IARV64_REQUEST_GETCOMMON 15
#define IARV64_REQUEST_COUNTPAGES 16
#define IARV64_REQUEST_PCIEFIX    17
#define IARV64_REQUEST_PCIEUNFIX  18
#define IARV64_REQUEST_CHANGEATTRIBUTE 19

#define IARV64_FLAGS0_MTOKNSOURCE_SYSTEM 0x80
#define IARV64_FLAGS0_MTOKNSOURCE_CREATOR 0x40
#define IARV64_FLAGS0_MATCH_MOTOKEN 0x20

#define IARV64_FLAGS1_KEYUSED_KEY 0x80
#define IARV64_FLAGS1_KEYUSED_USERTKN 0x40
#define IARV64_FLAGS1_KEYUSED_TTOKEN  0x20
#define IARV64_FLAGS1_KEYUSED_CONVERTSTART 0x10
#define IARV64_FLAGS1_KEYUSED_GUARDSIZE64 0x08
#define IARV64_FLAGS1_KEYUSED_CONVERTSIZE64 0x04
#define IARV64_FLAGS1_KEYUSED_MOTKN 0x02
#define IARV64_FLAGS1_KEYUSED_OWNERJOBNAME 0x01

#define IARV64_FLAGS2_COND_YES 0x80
#define IARV64_FLAGS2_FPROT_NO 0x40
#define IARV64_FLAGS2_CONTROL_AUTH 0x20
#define IARV64_FLAGS2_GUARDLOC_HIGH 0x10
#define IARV64_FLAGS2_CHANGEACCESS_GLOBAL 0x08
#define IARV64_FLAGS2_PAGEFRAMESIZE_1MEG 0x04
#define IARV64_FLAGS2_PAGEFRAMESIZE_MAX 0x02
#define IARV64_FLAGS2_PAGEFRAMESIZE_ALL 0x01

#define IARV64_FLAGS3_X 0


#define IARV64_FLAGS4_XX 1

#define IARV64_FLAGS5_XX 1  /* dump flags */


typedef struct IARV64_V4Parms_tag{
  char       version; /* 4 is the ly answer */
  char       request; /* 2 is GETSHARED */
  char       flags0;          
  char       key;
  char       flags1;
  char       flags2;
  char       flags3;
  char       flags4;          /* this one seems to have changeaccess.view=sharedwrite=0x10 */
  uint64_t   segmentCount;
  char       ttoken[16];      /* i don't know the usage of this, but it is not filled in the calls in shrmem64 */
  /* offset 0x20 (32) */
  uint64_t   token;           /* input (literal token or address ??, probably address)*/
  uint64_t   origin;          /* output address */
  /* offset 0x30 (48) */
  uint64_t   rangelistAddress;   
  uint64_t   memobjStart;        /* input */
  /* offset 0x40 (64) */
  uint32_t   guardSize;
  uint32_t   convertSize;
  uint32_t   aletValue;
  uint32_t   numRange;           /* number of ranges in range list */
  /* offset 0x50 (80) */
  uint32_t   v64ListPtr;         /* what is this? */
  uint32_t   v64ListLength;      /* what is this? */
  uint64_t   convertStart;       /* an address, i think */
  /* offset 0x60 (96) */
  uint64_t   convertSize64;
  uint64_t   guardSize64;       
  /* offset 0x70 (112) */
  uint64_t   userToken;          /* how does this differ from token at offset 32?? */
  /* offset 0x78 (120) */
  char       dumpPriority;
  char       flags5;
  char       flags6; 
  char       flags7;  
  char       dump;               /* some constant */
  char       flags8;       
  uint16_t   ownerASID;          
  /* offset 0x80 (128) */
  char       optionValue;
  char       unknown[8];
  char       ownerJobName[8];
  char       unknown145[7];
  /* offset 0x98 */
  uint64_t   mapPageTable;
  /* this next field is only in plist version = 5 and later */
  /* uint64_t   units;  
     char       flags9; 
     char       flags10; 
     char       flags11; 
   */
} IARV64_V4Parms;

#pragma pack(reset)

static MemObj getSharedMemObject(uint64_t segmentCount,
                                 MemObjToken token,
				 int key,
				 int alet, /* must be 0 (primary) or 2 (home) */
                                 uint32_t *iarv64RC,
                                 uint32_t *iarv64RSN) {

  MemObj result = 0;
  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  if (key == SHRMEM64_USE_CALLER_KEY){
    if (alet == 0){
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",ALETVALUE=0"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList)
	    : "r0", "r1", "r14", "r15"
	    );
    } else{
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",ALETVALUE=2"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList)
	    : "r0", "r1", "r14", "r15"
	    );
    }
  } else{
    char keyByte = key & 0xF;
    keyByte = (keyByte << 4);  /* because there's always one more thing in MVS */
    if (alet == 0){
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",KEY=%[key]"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",ALETVALUE=0"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList),[key]"m"(keyByte)
	    : "r0", "r1", "r14", "r15"
	    );
    } else{
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",KEY=%[key]"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",ALETVALUE=2"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList),[key]"m"(keyByte)
	    : "r0", "r1", "r14", "r15"
	    );
    }

  }

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

  return result;
}

static MemObj getSharedMemObjectNoFPROT(uint64_t segmentCount,
					MemObjToken token,
					int key,
					int alet, /* must be 0 (primary) or 2 (home) */
					uint32_t *iarv64RC,
					uint32_t *iarv64RSN) {
  
  MemObj result = 0;
  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  if (key == SHRMEM64_USE_CALLER_KEY){
    if (alet == 0){
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",FPROT=NO"
	    ",ALETVALUE=0"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList)
	    : "r0", "r1", "r14", "r15"
	    );
    } else{
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",FPROT=NO"
	    ",ALETVALUE=2"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList)
	    : "r0", "r1", "r14", "r15"
	    );
    }
  } else{
    char keyByte = key & 0xF;
    keyByte = (keyByte << 4);  /* because there's always one more thing in MVS */
    if (alet == 0){
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",KEY=%[key]"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",FPROT=NO"
	    ",ALETVALUE=0"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList),[key]"m"(keyByte)
	    : "r0", "r1", "r14", "r15"
	    );
    } else{
      __asm(
	    ASM_PREFIX
	    "         IARV64 REQUEST=GETSHARED"
	    ",USERTKN=(%[token])"
	    ",COND=YES"
	    ",KEY=%[key]"
	    ",SEGMENTS=(%[size])"
	    ",ORIGIN=(%[result])"
	    ",FPROT=NO"
	    ",ALETVALUE=2"
	    ",RETCODE=%[rc]"
	    ",RSNCODE=%[rsn]"
	    ",PLISTVER=4"
	    ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	    : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	    : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
	    [parm]"r"(&parmList),[key]"m"(keyByte)
	    : "r0", "r1", "r14", "r15"
	    );
    }

  }

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

  return result;
}


static MemObj getCommonMemObject(uint64_t segmentCount,
                                 MemObjToken token,
				 int key,
                                 uint32_t *iarv64RC,
                                 uint32_t *iarv64RSN){

  MemObj result = 0;
  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};
  char keyByte = key & 0xF;
  keyByte = (keyByte << 4);  /* because there's always one more thing in MVS */

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=GETCOMMON"
      ",MOTKN=(%[token])"
      ",COND=YES"
      ",KEY=%[key]"
      ",FPROT=NO"
      ",SEGMENTS=(%[size])"
      ",ORIGIN=(%[result])"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
        [parm]"r"(&parmList),[key]"m"(keyByte)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

  return result;
}

static void shareMemObject(MemObj object,
                           MemObjToken token,
			   int aletValue,
                           uint32_t *iarv64RC,
                           uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  struct {
    MemObj vsa;
    uint64_t reserved;
  } rangeList = {object, 0};

  uint64_t rangeListAddress = (uint64_t)&rangeList;
  
  if (aletValue == 0){
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=SHAREMEMOBJ"
	  ",USERTKN=(%[token])"
	  ",RANGLIST=(%[range])"
	  ",NUMRANGE=1"
	  ",COND=YES"
	  ",ALETVALUE=0"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [token]"r"(&token), [range]"r"(&rangeListAddress), [parm]"r"(&parmList)
	  : "r0", "r1", "r14", "r15"
	  );
  } else{
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=SHAREMEMOBJ"
	  ",USERTKN=(%[token])"
	  ",RANGLIST=(%[range])"
	  ",NUMRANGE=1"
	  ",COND=YES"
	  ",ALETVALUE=2"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [token]"r"(&token), [range]"r"(&rangeListAddress), [parm]"r"(&parmList)
	  : "r0", "r1", "r14", "r15"
	  );
  }

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}

static void makeSharedWritable(MemObj object,
			       uint64_t segmentCount,
			       /* MemObjToken token, */
			       int aletValue,
			       uint32_t *iarv64RC,
			       uint32_t *iarv64RSN) {
  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  struct {
    MemObj vsa;
    uint64_t numsegments;
  } rangeList = {object, segmentCount };

  uint64_t rangeListAddress = (uint64_t)&rangeList;

  if (aletValue == 0){
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=CHANGEACCESS"
	  ",VIEW=SHAREDWRITE"
	  /* ",USERTKN=(%[token])" */
	  ",RANGLIST=(%[range])"
	  ",NUMRANGE=1"
	  ",ALETVALUE=0"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [range]"r"(&rangeListAddress), [parm]"r"(&parmList)
	  : "r0", "r1", "r14", "r15"
	  );
  } else{
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=CHANGEACCESS"
	  ",VIEW=SHAREDWRITE"
	  /* ",USERTKN=(%[token])" */
	  ",RANGLIST=(%[range])"
	  ",NUMRANGE=1"
	  ",ALETVALUE=2"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [range]"r"(&rangeListAddress), [parm]"r"(&parmList)
	  : "r0", "r1", "r14", "r15"
	  );
  }

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}

static void detachSingleSharedMemObject(MemObj object,
                                        MemObjToken token,
					int aletValue,
                                        uint32_t *iarv64RC,
                                        uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  if (aletValue == 0){
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=DETACH"
	  ",MATCH=SINGLE"
	  ",MEMOBJSTART=(%[mobj])"
	  ",MOTKN=(%[token])"
	  ",MOTKNCREATOR=USER"
	  ",AFFINITY=LOCAL"
	  ",OWNER=YES"
	  ",COND=YES"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [mobj]"r"(&object), [parm]"r"(&parmList), [token]"r"(&token)
	  : "r0", "r1", "r14", "r15"
	  );
  } else{
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=DETACH"
	  ",MATCH=SINGLE"
	  ",MEMOBJSTART=(%[mobj])"
	  ",MOTKN=(%[token])"
	  ",MOTKNCREATOR=USER"
	  ",AFFINITY=LOCAL"
	  ",OWNER=YES"
	  ",COND=YES"
	  ",ALETVALUE=2"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [mobj]"r"(&object), [parm]"r"(&parmList), [token]"r"(&token)
	  : "r0", "r1", "r14", "r15"
	  );
  }

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}

static void detachSingleSharedMemObjectNotOwner(MemObj object,
						MemObjToken token,
						int aletValue,
						uint32_t *iarv64RC,
						uint32_t *iarv64RSN) {
  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  if (aletValue == 0){
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=DETACH"
	  ",MATCH=SINGLE"
	  ",MEMOBJSTART=(%[mobj])"
	  ",MOTKN=(%[token])"
	  ",MOTKNCREATOR=USER"
	  ",AFFINITY=LOCAL"
	  ",OWNER=NO"
	  ",COND=YES"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [mobj]"r"(&object), [parm]"r"(&parmList), [token]"r"(&token)
	  : "r0", "r1", "r14", "r15"
	  );
  } else{
    __asm(
	  ASM_PREFIX
	  "         IARV64 REQUEST=DETACH"
	  ",MATCH=SINGLE"
	  ",MEMOBJSTART=(%[mobj])"
	  ",MOTKN=(%[token])"
	  ",MOTKNCREATOR=USER"
	  ",AFFINITY=LOCAL"
	  ",OWNER=NO"
	  ",ALETVALUE=2"
	  ",COND=YES"
	  ",RETCODE=%[rc]"
	  ",RSNCODE=%[rsn]"
	  ",PLISTVER=4"
	  ",MF=(E,(%[parm]),COMPLETE)                                              \n"
	  : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
	  : [mobj]"r"(&object), [parm]"r"(&parmList), [token]"r"(&token)
	  : "r0", "r1", "r14", "r15"
	  );

  }

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}

static void detachSharedMemObjects(MemObjToken token,
                                   uint32_t *iarv64RC,
                                   uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=DETACH"
      ",MATCH=MOTOKEN"
      ",MOTKN=(%[token])"
      ",MOTKNCREATOR=USER"
      ",AFFINITY=LOCAL"
      ",OWNER=YES"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [token]"r"(&token), [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}


static void removeSystemInterestForAllObjects(MemObjToken token,
                                              uint32_t *iarv64RC,
                                              uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=DETACH"
      ",MATCH=MOTOKEN"
      ",MOTKN=(%[token])"
      ",MOTKNCREATOR=USER"
      ",AFFINITY=SYSTEM"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [token]"r"(&token), [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}

static void removeSystemInterestForSingleObject(MemObj object,
                                                MemObjToken token,
                                                uint32_t *iarv64RC,
                                                uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=DETACH"
      ",MATCH=SINGLE"
      ",MEMOBJSTART=(%[mobj])"
      ",MOTKN=(%[token])"
      ",MOTKNCREATOR=USER"
      ",AFFINITY=SYSTEM"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [mobj]"r"(&object), [token]"r"(&token), [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}

static ASCB *localGetASCB(){
  int *mem = (int*)0;
  return (ASCB*)(mem[CURRENT_ASCB/sizeof(int)]&0x7FFFFFFF);
}

MemObjToken shrmem64GetAddressSpaceToken(void) {

  union {
    MemObj token;
    struct {
      ASCB * __ptr32 ascb;
      uint32_t asid;
    };
  } result = { .ascb = localGetASCB(), .asid = result.ascb->ascbasid };

  return result.token;
}

int shrmem64Alloc2(MemObjToken userToken, size_t size, int key, int aletValue, bool fetchProtect, void **result, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  /*
   * Convert size in bytes into segments (megabytes), round up if necessary.
   */
  uint64_t segmentCount = 0;
  if ((size & 0xFFFFF) == 0) {
    segmentCount = size >> 20;
  } else{
    segmentCount = (size >> 20) + 1;
  }

  MemObj mobj = (fetchProtect ?
		 getSharedMemObject(segmentCount, userToken, key, aletValue, 
				    &iarv64RC, &iarv64RSN) :
		 getSharedMemObjectNoFPROT(segmentCount, userToken, key, aletValue, 
					   &iarv64RC, &iarv64RSN));
  if (!isIARV64OK(iarv64RC)) {
    *rsn = makeRSN(RC_SHRMEM64_GETSHARED_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_GETSHARED_FAILED;
  }

  *result = (void *)mobj;

  return RC_SHRMEM64_OK;
}

int shrmem64Alloc(MemObjToken userToken, size_t size, void **result, int *rsn) {
  return shrmem64Alloc2(userToken,size,SHRMEM64_USE_CALLER_KEY,0,true,result,rsn);
}

int shrmem64CommonAlloc(MemObjToken userToken, size_t size, void **result, int *rsn) {
  return shrmem64CommonAlloc2(userToken,size,0,result,rsn);
}

int shrmem64CommonAlloc2(MemObjToken userToken, size_t size, int key, void **result, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  /*
   * Convert size in bytes into segments (megabytes), round up if necessary.
   */
  uint64_t segmentCount = 0;
  if ((size & 0xFFFFF) == 0) {
    segmentCount = size >> 20;
  } else{
    segmentCount = (size >> 20) + 1;
  }

  MemObj mobj = getCommonMemObject(segmentCount, userToken, key,
                                   &iarv64RC, &iarv64RSN);
  if (!isIARV64OK(iarv64RC)) {
    *rsn = makeRSN(RC_SHRMEM64_GETCOMMON_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_GETSHARED_FAILED;
  }

  *result = (void *)mobj;

  return RC_SHRMEM64_OK;
}

int shrmem64Release(MemObjToken userToken, void *target, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  MemObj mobj = (MemObj)target;

  removeSystemInterestForSingleObject(mobj, userToken, &iarv64RC, &iarv64RSN);
  if (!isIARV64OK(iarv64RC)) {
    *rsn = makeRSN(RC_SHRMEM64_SINGLE_SYS_DETACH_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_SINGLE_SYS_DETACH_FAILED;
  }

  return RC_SHRMEM64_OK;
}

int shrmem64ReleaseAll(MemObjToken userToken, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  removeSystemInterestForAllObjects(userToken, &iarv64RC, &iarv64RSN);
  if (!isIARV64OK(iarv64RC)) {
    *rsn = makeRSN(RC_SHRMEM64_ALL_SYS_DETACH_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_ALL_SYS_DETACH_FAILED;
  }

  return RC_SHRMEM64_OK;
}

int shrmem64GetAccess2(MemObjToken userToken, void *target, bool makeWritable, int aletValue, uint64_t size, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;
  uint64_t segmentCount = 0;
  if ((size & 0xFFFFF) == 0) {
    segmentCount = size >> 20;
  } else{
    segmentCount = (size >> 20) + 1;
  }

  MemObj mobj = (MemObj)target;

  shareMemObject(mobj, userToken, aletValue, &iarv64RC, &iarv64RSN);
  if (!isIARV64OK(iarv64RC)) {
    *rsn = makeRSN(RC_SHRMEM64_SHAREMEMOBJ_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_SHAREMEMOBJ_FAILED;
  }
  if (makeWritable){
    makeSharedWritable(mobj, segmentCount, aletValue, &iarv64RC, &iarv64RSN);
    if (!isIARV64OK(iarv64RC)) {
      *rsn = makeRSN(RC_SHRMEM64_CHANGEACCESS_FAILED, iarv64RC, iarv64RSN);
      return RC_SHRMEM64_CHANGEACCESS_FAILED;
    }
  }

  return RC_SHRMEM64_OK;
}

int shrmem64GetAccess(MemObjToken userToken, void *target, int *rsn) {
  /* it's ok to not know the number of segments if not making the MemObj writable */
  return shrmem64GetAccess2(userToken,target,false,0,0,rsn);
}

int shrmem64RemoveAccess(MemObjToken userToken, void *target, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  MemObj mobj = (MemObj)target;

  detachSingleSharedMemObject(mobj, userToken, 0, &iarv64RC, &iarv64RSN);
  if (!isIARV64OK(iarv64RC)) {
    *rsn = makeRSN(RC_SHRMEM64_DETACH_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_DETACH_FAILED;
  }

  return RC_SHRMEM64_OK;
}

int shrmem64RemoveAccess2(MemObjToken userToken, void *target, int aletValue, bool isOwner, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  MemObj mobj = (MemObj)target;

  if (isOwner){
    detachSingleSharedMemObject(mobj, userToken, aletValue, &iarv64RC, &iarv64RSN);
  } else{
    detachSingleSharedMemObjectNotOwner(mobj, userToken, aletValue, &iarv64RC, &iarv64RSN);
  }
  if (!isIARV64OK(iarv64RC)) {
    *rsn = makeRSN(RC_SHRMEM64_DETACH_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_DETACH_FAILED;
  }

  return RC_SHRMEM64_OK;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/



