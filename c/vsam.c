

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
#include <metal/stdarg.h>
#include "metalio.h"
#else
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h" 
#include "idcams.h"
#include "utils.h" 
#include "qsam.h"  
#include "vsam.h"
#include "logging.h"

#define TRACE 0

/**************************************************************
* This function takes the parameters and generates an ACB in memory based off of them.
*  It does this by mallocing the data needed and then emulating the memory changes that
*  take place in the ACB and GENCB macros.
* The return value is the location to the ACB that is generated, starting at the 8 byte plist.
* NOTE: this function does not support VTAM extensions.
***************************************************************/
char *makeACB(char *ddname,
              int acbLen,
              int macrf1,
              int macrf2,
              int rplSub,
              int rplType,
              int rplLen,
              int keyLen,
              int opCode1,
              int opCode2,
              int recLen,
              int bufLen){
  char *acb = NULL;
  char *acbWithPList = NULL;
  ACBCommon *common = NULL;
  RPLCommon *rpl = NULL;
  int i = 0;
  int ddnameLen = strlen(ddname);

  /* find space */
  acbWithPList = (char *)safeMalloc31(acbLen+rplLen+8, "ACB");
  acb = acbWithPList + 8;

  /* format the space */
  *((int*)acbWithPList) = 0x80000000;                           /* first bit set is last entry indicator. */
  *((int*)(acbWithPList+4)) = ((int)acb);                       /* bytes 4-7 are address.                 */
  memset((void*)acb,0,acbLen+rplLen);                       
  /* fill the ACB parameters                                    */
  common = (ACBCommon *) (acb+ACB_COMMON_OFFSET);        
  common->acbId = 0xA0;                                         /* Always A0 */
  common->acbSubtype = 0x10;                                    /* Always 10 */
  common->acbLen = acbLen & 0xff;                                      
  common->macrf12 = macrf1 & 0xffff;   
  common->numStr = 1;
  common->macrf3 = macrf2 & 0xff;
  common->recfm = 0x80;                                         /* JES format     */
  common->dsorg = 0x0008;                                       /* ACB Indicator  */
  common->openFlags = 0x02;                                     /* ACB is locked  */
  for (i=0; i<8; i++)                                           
    common->ddname[i] = (i<ddnameLen) ? ddname[i] : ' ';        
  /* fill the RPL parameters */
  rpl = (RPLCommon *) (acb+RPL_COMMON_OFFSET);           
  rpl->rplSubtype = rplSub & 0xff;
  rpl->rplType = rplType & 0xff;
  rpl->rplLen = rplLen & 0xff;
  rpl->keyLen = keyLen & 0xffff;
  rpl->acb = ((int)acb+ACB_COMMON_OFFSET);
  rpl->bufLen = bufLen;                                         
  rpl->workArea = 0;  
  rpl->optcd1 = opCode1;
  rpl->optcd2 = opCode2;
  rpl->recLen = recLen;
  rpl->exitDefn = 0x40;                                         /* No exit specified */
                                                                
  return acbWithPList;
}

/**************************************************************
* This function handles opening and closing of an ACB, using the OPEN/CLOSE macros.
*  It does so by modifying a plist and passing it to the macro to handle everything.
* The return value is the status returned by the macro.
***************************************************************/
int opencloseACB(char *acb, int mode, int svc){
  int status = 0;
  int realACBaddr = (int)acb;
  *((int*)acb) = 0x80000000 | mode;
  *((int*)(acb+4)) = realACBaddr+8;

  if (svc == 19){
#ifdef _LP64
    __asm(ASM_PREFIX
    " XR  1,1   \n"
	  " LG  0,%1  \n"
          " SAM31    \n"
          " SYSSTATE AMODE64=NO \n"
	  " SVC 19    \n"
          " SAM64 \n"
          " SYSSTATE AMODE64=YES \n"
	  " ST  15,%0 \n":
	  "=m"(status):
	  "m"(acb):
	  "r1","r0", "r15");
    if (TRACE) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nOPEN64 at %08x returned %d with reason %x", acb, status, ((ACBCommon *)realACBaddr)->erFlags); 
    }
#else
    __asm(ASM_PREFIX
          " XR 1,1\n"
	  " L 0,%1\n"
	  " SVC 19\n"
	  " ST 15,%0":
	  "=m"(status):
	  "m"(acb):
	  "r1","r0", "r15");
    if (TRACE && status) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nOPEN32 at %08x returned %d with reason %x", acb, status, ((ACBCommon *)realACBaddr)->erFlags);
    }
#endif
  } else{
#ifdef _LP64
    __asm(ASM_PREFIX
	  " XR 1,1 \n"
	  " LG 0,%1 \n"
          " SAM31 \n"
          " SYSSTATE AMODE64=NO \n"
	  " SVC 20 \n"
	  " SAM64 \n"
          " SYSSTATE AMODE64=YES \n"
	  " ST 15,%0 \n":
	  "=m"(status):
	  "m"(acb):
	  "r1","r0", "r15");
    if (TRACE) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nCLOSE64 at %08x returned %d with reason %x", acb, status, ((ACBCommon *)realACBaddr)->erFlags);
    }
#else
    __asm(ASM_PREFIX
          " XR 1,1 \n"
	  " L 0,%1 \n"
	  " SVC 20 \n"
	  " ST 15,%0" :
	  "=m"(status):
	  "m"(acb):
	  "r1","r0", "r15");
    if (TRACE && status) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nCLOSE32 at %08x returned %d with reason %x", acb, status, ((ACBCommon *)realACBaddr)->erFlags);
    }
#endif
  }
  return status;
}

/**************************************************************
* This function creates, opens, returns an ACB with no VTAM extension
*  It does so by calling makeDCB with familiar parameters, and returning its output.
*  Then, it calls opencloseACB to handle the OPEN macro.
* The return value is the location to the ACB that is generated, starting at the 8 byte plist.
* NOTE: this function does not support VTAM extensions.
***************************************************************/
char *openACB(char *ddname,
              int mode,
              int macrf1,
              int macrf2,
              int opCode1,
              int recLen,
              int bufLen){
  char* acb = NULL;
  int status = 0;
  acb = makeACB(ddname, 0x4C, macrf1, macrf2, 0x10, 0, 0x4C, 0, opCode1, 0, recLen, bufLen);

  status = opencloseACB(acb, mode, 19);
  return acb;
  /* note that status returns the success rate of the OPEN command but is not returned anywhere */
}

/**************************************************************
* This function closes an ACB with no VTAM extension.
*  It does so by calling opencloseACB to handle the CLOSE macro.
*  Then, it frees the bytes it malloced prior.
* The return value is the status returned by the macro.
***************************************************************/
int closeACB(char *acb, int mode){
  if (TRACE) { 
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "closing ACB %p\n", acb);
  }

  ACBCommon *realACB = (ACBCommon*)(acb+ACB_COMMON_OFFSET+8);
  RPLCommon *realRPL = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  int size = realACB->acbLen + realRPL->rplLen + 8;
  int status = opencloseACB(acb, mode, 20);
  safeFree31(acb, size);
  return status;
}

/**************************************************************
* This function modifies the parameters in the RPL.
*  It does so by modifying the memory directly, emulating the RPL macro.
* NOTE:  The onus is on the programmer to ensure these parameters are valid.                                                                    .
***************************************************************/
void modRPL(char *acb,
            int reqType,
            int keyLen,
            int workArea,
            int searchArg,
            int optcd1,
            int optcd2,
            int nextRPL,
            int recLen,
            int bufLen){
  ACBCommon *realACB = (ACBCommon*)(acb+ACB_COMMON_OFFSET+8);
  RPLCommon *realRPL = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  realRPL->rplType = reqType;
  realRPL->keyLen = keyLen;
  realRPL->workArea = workArea;
  realRPL->arg = searchArg;
  realRPL->optcd1 = optcd1;
  realRPL->optcd2 = optcd2;
  realRPL->nextRPL = nextRPL;
  realRPL->recLen = recLen;
  realRPL->bufLen = bufLen;
}


/**************************************************************
* This function places the "head" to a specific record in the data set.
*  It does so by adding the provided RBA to the RPL parameters,
*  then calling the point method.
* The return value is the status returned by the macro.
* NOTE:  This method assumes you have modified the RPL prior to the call.
* NOTE:  This method is designed for ESDS point.
***************************************************************/
int pointByRBA(char *acb, int *rba) {
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  char *arg = (char *)rba;
  return point(acb, arg);
}

/**************************************************************
* This function places the "head" to a specific record in the data set.
*  It does so by adding the provided Key to the RPL parameters,
*  then calling the point method.
* The return value is the status returned by the macro.
* NOTE:  This method assumes you have modified the RPL prior to the call.
* NOTE:  This method is designed for KSDS point.
***************************************************************/
int pointByKey(char *acb, char *key, int len) {
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  return point(acb, key);
}

/**************************************************************
* This function places the "head" to a specific record in the data set.
*  It does so by adding the provided relative record number to the RPL parameters,
*  then calling the point method.
* The return value is the status returned by the macro.
* NOTE:  This method assumes you have modified the RPL prior to the call.
* NOTE:  This method is designed for RRDS point.
***************************************************************/
int pointByRecord(char *acb, int *record) {
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  char *arg = (char *)record;
  return point(acb, arg);
}

/**************************************************************
* This function places the "head" to a specific record in the data set.
*  It does so by adding the provided RBA to the RPL parameters,
*  then calling the point method.
* The return value is the status returned by the macro.
* NOTE:  This method assumes you have modified the RPL prior to the call.
* NOTE:  This method is designed for LDS point and CI access.
***************************************************************/
int pointByCI(char *acb, int *rba) {
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  char *arg = (char *)rba;
  /*
    if (rba % 4096) {
      return 8;
    }
    if (rpl->recLen != 4096) {
      rpl->recLen = 4096;
    }
  */
  return point(acb, arg);
}

/**************************************************************
* This function places the "head" to a specific record in the data set.
*  It does so by adding the search argument provided to the RPL parameters,
*  then calling the POINT macro.
* The return value is the status returned by the macro.
* NOTE:  This method assumes you have modified the RPL prior to the call.
***************************************************************/
int point(char *acb, char *arg){
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  int status = 0;
  rpl->arg = (int)arg;

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint64 r13;
      char callerSaveArea[72];
    )
  );

#ifdef _LP64
  __asm(ASM_PREFIX
	" LG 1,%1\n"
  " STG 13,0(%2) \n"
  " LA 13,8(%2) \n"
        " SAM31 \n"
	" SYSSTATE AMODE64=NO \n"
	" POINT RPL=(1)\n"
	" SAM64 \n"
	" SYSSTATE AMODE64=YES \n"
  " LG 13,-8(13) \n"
	" ST 15,%0 \n":
	"=m"(status):
	"m"(rpl), "r"(&parms31->r13):
	"r1", "r2", "r15");
  if (TRACE && status) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nPOINT64 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback); 
  }
#else
  __asm(ASM_PREFIX
        " L 1,%1\n"
	" POINT RPL=(1)\n"
	" ST 15,%0":
	"=m"(status):
	"m"(rpl):
	"r1", "r2", "r15");
  if (TRACE && status) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nPOINT32 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback);
  }
#endif
  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );
  return status;
}

/**************************************************************
* This function inserts a record into the VSAM data set currently active within a given ACB.
*  It does so by calling the PUT macro with the given RPL and line to handle insertion.
* The return value is the status returned by the macro.
* NOTE:  This method assumes you have modified the RPL prior to insertion (except for line).
* NOTE:  Do not use a line length longer than the RPL buffer space allows.
***************************************************************/
int putRecord(char *acb, char* line, int length){
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  if ((int)(line) != (rpl->workArea)) {
    rpl->workArea = (int)line; 
  }
  if (length != rpl->bufLen) {
    modRPL(acb, rpl->rplType, rpl->keyLen, rpl->workArea, rpl->arg, rpl->optcd1, rpl->optcd2,
           rpl->nextRPL, length, length);
  }

  return easyPut(acb);
}

/**************************************************************
* This function inserts a record into the VSAM data set currently active within a given ACB.
*  It does so by calling the PUT macro with the given RPL to handle insertion.
* The return value is the status returned by the macro OR with trace:
*                     1 - Duplicate Key
*                     2 - Out of Space
*                     3 - Incorrect RBA use in KSDS
*                     6 - Invalid Record Length
*                     7 - Invalid Key Length
* NOTE:  This method assumes you have modified the RPL prior to insertion (including line).
***************************************************************/
int easyPut(char *acb){
  int status = 0;
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  if (!(rpl->workArea)) {
    /* printf("\nInvalid Work Area specified"); */
    return 8;
  }

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint64 r13;
      char callerSaveArea[72];
    )
  );

#ifdef _LP64
  __asm(ASM_PREFIX
	" LG 1,%1\n"
  " STG 13,0(%2) \n"
  " LA 13,8(%2) \n"
        " SAM31 \n"
	" SYSSTATE AMODE64=NO \n"
	" PUT RPL=(1)\n"
	" SAM64 \n"
	" SYSSTATE AMODE64=YES \n"
  " LG 13,-8(13) \n"
	" ST 15,%0 \n":
	"=m"(status):
	"m"(rpl), "r"(&parms31->r13):
	"r1", "r2", "r15");
  if (TRACE) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nPUT64 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback); 
  }
#else
  __asm(ASM_PREFIX
        " L 1,%1\n"
	" PUT RPL=(1)\n"
	" ST 15,%0":
	"=m"(status):
	"m"(rpl):
	"r1", "r2", "r15");
  if (TRACE && status) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nPUT32 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback);
  }
#endif

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  if (TRACE) {
    if (((rpl->feedback & 0xff) == 0x08) || ((rpl->feedback & 0xff) == 0x0C)) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  PUT32 encountered a duplicate key.");
      return 1;
    }
    if ((rpl->feedback & 0xff) == 0x1C) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  PUT32 did not have enough space for the PUT.");           
      return 2;
    }
    if ((rpl->feedback & 0xff) == 0x4C) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  PUT32 was called with ADR or CNV set to a KSDS dataset.");           
      return 3;
    }
    if ((rpl->feedback & 0xff) == 0x6C) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  PUT32 did not have a large enough Record Length.");           
      return 6;
    }
    if ((rpl->feedback & 0xff) == 0x70) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  PUT32 did not have a large enough Key Length.");           
      return 7;
    }
  }
  return status;
}

/**************************************************************
* This function reads a record from the VSAM data set currently active within a given ACB.
*  It does so by calling the GET macro with the necessary parameters to handle the request.
*  Then, it stores the record into line.
* The return value is the eof value.
* NOTE:  This method assumes you have modified the RPL prior to the request (except for line).
***************************************************************/
int getRecord(char *acb, char *line, int *lengthRead) {
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  int status = 0;
  if ((int)(line) != (rpl->workArea)) {
    rpl->workArea = (int)line;
  }
  status = easyGet(acb);
  *lengthRead = rpl->recLen;
  return status;
}

/**************************************************************
* This function reads a record from the VSAM data set currently active within a given ACB.
*  It does so by calling the GET macro with the given RPL to handle insertion.
* The return value is 1 - Sequential End of File
*  only modified if   2 - No Matching Record
*  TRACE is set       3 - No Matching RBA
*                     5 - Work Area too small
*                     6 - Invalid Record Length
*                     7 - Invalid Key Length
*                     9 - ADR set for RRDS
*                     else - return code provided by GET macro
* NOTE:  This method assumes you have modified the RPL prior to request (including line).
***************************************************************/
int easyGet(char *acb){
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  int status = 0;
  int eof = 0;

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint64 r13;
      char callerSaveArea[72];
    )
  );

#ifdef _LP64
  __asm(ASM_PREFIX
        " LG 1,%1\n"
        " STG 13,0(%2) \n"
        " LA 13,8(%2) \n"
        " SAM31 \n"
	" SYSSTATE AMODE64=NO \n"
        " GET RPL=(1)\n"
        " SAM64 \n"
	" SYSSTATE AMODE64=YES \n"
        " LG 13,-8(13) \n"
        " ST 15,%0 \n":
        "=m"(status):
        "m"(rpl), "r"(&parms31->r13):
        "r1", "r15");
  if (TRACE) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nGET64 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback);
  }
#else
  __asm(ASM_PREFIX
        " L 1,%1\n"
        " GET RPL=(1)\n"
        " ST 15,%0":
        "=m"(status):
        "m"(rpl):
        "r1", "r15");
  if (TRACE && status) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nGET32 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback);
  }
#endif

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  if (TRACE) {
    if ((rpl->feedback & 0xff) == 0x04) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  GET32 encountered a Sequential End of Data"); 
      return 1;
    }
    if ((rpl->feedback & 0xff) == 0x10) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  GET32 did not find the record.");           
      return 2;
    }
    if ((rpl->feedback & 0xff) == 0x20) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  GET32 did not find the RBA.");              
      return 3;
    }
    if ((rpl->feedback & 0xff) == 0x2C) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  GET32 did not have a large enough Work Area.");           
      return 5;
    }
    if ((rpl->feedback & 0xff) == 0x6C) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  GET32 did not have a large enough Record Length.");           
      return 6;
    }
    if ((rpl->feedback & 0xff) == 0x70) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  GET32 did not have a large enough Key Length.");           
      return 7;
    }
    if ((rpl->feedback & 0xff) == 0xC4) {
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_WARNING, "\n  GET32 ran a request by address on an RRDS.");           
      return 9;
    }
  }
  return status;
}

/**************************************************************
* This function deletes a record from the VSAM data set currently active
* within a given ACB.  It does so by calling the ERASE macro with the
* given RPL to handle insertion.
* The return value is the return code provided by ERASE macro
* NOTE:  This method assumes you have modified the RPL prior to request.
* NOTE:  To do an erase, you must first do a GET for update
***************************************************************/
int eraseRecord(char *acb) {
  RPLCommon *rpl = (RPLCommon*)(acb+RPL_COMMON_OFFSET+8);
  int status = 0;

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint64 r13;
      char callerSaveArea[72];
    )
  );

#ifdef _LP64
  __asm(ASM_PREFIX
        " LG 1,%1\n"
        " STG 13,0(%2) \n"
        " LA 13,8(%2) \n"
        " SAM31 \n"
	    " SYSSTATE AMODE64=NO \n"
        " ERASE RPL=(1)\n"
        " SAM64 \n"
	    " SYSSTATE AMODE64=YES \n"
        " LG 13,-8(13) \n"
        " ST 15,%0 \n":
        "=m"(status):
        "m"(rpl), "r"(&parms31->r13):
        "r1", "r15");
  if (TRACE) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nERASE64 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback);
  }
#else
  __asm(ASM_PREFIX
        " L 1,%1\n"
        " ERASE RPL=(1)\n"
        " ST 15,%0":
        "=m"(status):
        "m"(rpl):
        "r1", "r15");
  if (TRACE && status) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nERASE32 at %08x returned %d, feedback=%02x%06x", rpl, status, rpl->status, rpl->feedback);
  }
#endif

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  return status;
}

static int getEffectiveIDCAMSStatus(int execRC, int execRSN) {

  if (execRC == RC_IDCAMS_CALL_FAILED) {
    return execRSN;
  }

  return execRC;
}

/**************************************************************  
* This function attempts to allocate a dataset.  It does so by
*  allocating a temporary QSAM dataset and fills it with the
*  corresponding IDCAMS commands and passes it via branch entry.
*  Then it calls TSO and allocates the DD to the DSN created.
* The return value is the maxcc from all commands it processes.
* WARNING:  THIS FUNCTION OVERWRITES EXISTING DATASETS!!!
* NOTE:  Assumes SMS managed environment
* NOTE:  If no key, give keyLen a value of 0
* NOTE:  If ignoring dataclass, give a value of 0
* NOTE:  This function does not support VTAM extensions.
* NOTE:  Assumes ESDS=(ADR,SEQ), KSDS=(KEY,SEQ), RRDS=(KEY,DIR), LDS=(CNV,SEQ)
***************************************************************/
int allocateDataset(char *ddname, char *dsn, int mode, char *space, int macrf1, int macrf2,
                    int opCode1, int avgRecLen, int maxRecLen, int bufLen,
                    int keyLen, int keyOff, char *dataclass) {
  if (TRACE) { 
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nallocateDataset dd=%s, dsn=%s", ddname, dsn);
  }

  int status = 0;

  //format the statement-specific variables
  char maxLenInt[10];                           //string representing max record length as an integer
  sprintf(maxLenInt, "%d", maxRecLen);
  char avgLenInt[10];                           //string representing average record length as an integer
  sprintf(avgLenInt, "%d", avgRecLen);
  char vsamtype[11];
  if ((macrf1 & 0xF800) == 0x5000) {          //ESDS
    strcpy(vsamtype, "NONINDEXED");
  } else if ((macrf1 & 0xF800) == 0x9000) {   //KSDS
    strcpy(vsamtype, "INDEXED");
  } else if ((macrf1 & 0xF800) == 0x8800) {   //RRDS
    strcpy(vsamtype, "NUMBERED");
  } else if ((macrf1 & 0xF800) == 0x3000) {   //LDS
    strcpy(vsamtype, "LINEAR");
  }
  char keyInt[3];                             //string representing key length as an integer
  char keyOffInt[10];                         //string representing key offset as an integer
  if (keyLen > 999) {
    keyLen = 999;
    /* printf("Key Length Reduced to maximum 999."); */
  }
  if (keyLen) {
    sprintf(keyInt, "%d", keyLen);
  }
  if (keyOff > 999999999) {
    keyOff = 999999999;
    /* printf("Key Offset Reduced to maximum 999999999."); */
  }
  sprintf(keyOffInt, "%d", keyOff);

  int allocType = 0;
  char remaining[10] = "";
  char leadChar;
  leadChar = *space;
  if (leadChar == 'c' || leadChar == 'C') {        //Cylinders
    allocType = 1;
    strcpy(remaining, space+2);
  } else if (leadChar == 'k' || leadChar == 'K') { //Kilobytes
    allocType = 2;
    strcpy(remaining, space+2);
  } else if (leadChar == 'm' || leadChar == 'M') { //Megabytes
    allocType = 3;
    strcpy(remaining, space+2);
  } else if (leadChar == 'r' || leadChar == 'R') { //Records
    allocType = 4;
    strcpy(remaining, space+2);
  } else if (leadChar == 't' || leadChar == 'T') { //Tracks
    allocType = 5;
    strcpy(remaining, space+2);
  }

  IDCAMSCommand *idcamsCmd = idcamsCreateCommand();
  /* fill the IDCAMS command with the relevant IDCAMS processing options */
  char line[80] = {};
  strcpy(line, " DELETE '");
  strcat(line, dsn);
  strcat(line, "'");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, " SET MAXCC = 0                                                                  ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, " DEFINE CLUSTER                                  -                              ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "  ( NAME ('");
  strcat(line, dsn);
  strcat(line, "') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "    ");
  strcat(line, vsamtype);
  strcat(line, " -");
  idcamsAddLineToCommand(idcamsCmd, line);
  if (allocType == 1) {
    strcpy(line, "    CYLINDERS(");
  } else if (allocType == 2) {
    strcpy(line, "    KILOBYTES(");
  } else if (allocType == 3) {
    strcpy(line, "    MEGABYTES(");
  } else if (allocType == 4) {
    strcpy(line, "    RECORDS(");
  } else if (allocType == 5) {
    strcpy(line, "    TRACKS(");
  }
  if (allocType) {
    strcat(line, remaining);
    strcat(line, ") -");
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  strcpy(line, "    SHAREOPTIONS (2 3)                           -                              ");
  idcamsAddLineToCommand(idcamsCmd, line);
  if ((macrf1 & 0xF800) != 0x3000) {  /* LDS doesn't use RECORDSIZE */
    strcpy(line, "    RECORDSIZE (");
    strcat(line, avgLenInt);
    strcat(line, " ");
    strcat(line, maxLenInt);
    strcat(line, ") -");
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  if (keyLen > 0) {
    strcpy(line, "    KEYS(");
    strcat(line, keyInt);
    strcat(line, " ");
    strcat(line, keyOffInt);
    strcat(line, ") -");
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  if (dataclass) {
    strcpy(line, "    DATACLAS ('");
    strcat(line, dataclass);
    strcat(line, "') -");
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  /* strcpy(line, "    REUSE                                        -                              ");  //causes problem with AIX */
  /* putline(idcamsDCB, line); */
  strcpy(line, "  )                                              -                              ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, " DATA                                            -                              ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "  ( NAME ('");
  strcat(line, dsn);
  strcat(line, ".DATA') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "  )                                                                             ");
  idcamsAddLineToCommand(idcamsCmd, line);

  /* invoke IDCAMS */
  IDCAMSCommandOutput *idcamsResult = NULL;
  int idcamsRSN = 0;
  int idcamsRC = idcamsExecuteCommand(idcamsCmd, &idcamsResult, &idcamsRSN);
  //set the maxcc
  /* Since all the code here mixes RCs from different system calls, we'll do
   * our best without a total rewrite of the error handling parts. */
  int idcamsStatus = getEffectiveIDCAMSStatus(idcamsRC, idcamsRSN);
  status = (idcamsStatus > status) ? idcamsStatus : status;

  /* Original code indirectly used the default DD to print IDCAMS message.
   * We'll do the same and print everything using the standard function. */
  if (idcamsResult != NULL) {
    idcamsPrintCommandOutput(idcamsResult);
    idcamsDeleteCommandOutput(idcamsResult);
    idcamsResult = NULL;
  }

  //free the allocated space
  idcamsDeleteCommand(idcamsCmd);
  idcamsCmd = NULL;

  //Alloc control blocks for SVC99
  char *reqBlock = safeMalloc31(24, "Request Block");  /* size is 20, include plist */
  SV99RB *realRB = (SV99RB *) (reqBlock + 4);
  *((int*)(reqBlock)) = (0x80000000 | (int)realRB);

  //Set up text units for SVC99 a second time
  SV99TU *sv99dd = (SV99TU*) safeMalloc31(14, "DD Name");
  SV99TU *sv99dsn = (SV99TU*) safeMalloc31(52, "DSN");
  SV99TU *sv99status2 = (SV99TU*) safeMalloc(7, "Status2");

  int code[13] = {0};
  //0001 - DD Name
  code[0] = 0x00010001;
  code[1] = strlen(ddname);
  memcpy((char*)code+4, (char*)code+6, 2);
  strcpy((char*)code+6, ddname);
  memset((char*)code+6+strlen(ddname), 0, 50-strlen(ddname));
  memcpy(sv99dd, code, 14);

  //0002 - DSName
  code[0] = 0x00020001;
  code[1] = strlen(dsn);
  memcpy((char*)code+4, (char*)code+6, 2);
  strcpy((char*)code+6, dsn);
  memset((char*)code+6+strlen(dsn), 0x40, 50-strlen(dsn));
  memcpy(sv99dsn, code, 6+strlen(dsn));

  //0004 - Status = SHR (08)
  code[0] = 0x00040001;
  code[1] = 0x00010800;
  memset((char*)code+7, 0, 49);
  memcpy(sv99status2, code, 7);

  //reference the text units with a TU pointer list (array of addresses, last with high-order bit set)
  SV99TU *tuPList2[3] = { sv99dd, sv99dsn, (SV99TU*) (0x80000000 | (int)sv99status2)};
  //set up the request block
  realRB->rbLen = 0x14;
  realRB->verbCode = SV99_VERB_ALLOC;
  realRB->textUnit = (int)tuPList2;
  realRB->flag1 = SV99_FLAG1_NOCNV | SV99_FLAG1_NOMNT | SV99_FLAG1_GDGNT;  //for now...

  /* call SVC 99 to assign the DD name (R1 - plist = sv99rb) */
  int reg15 = 0;
#ifdef _LP64
  __asm(ASM_PREFIX
        " SAM31 \n"
	" SYSSTATE AMODE64=NO \n"
        " LG 1,%1\n"
        " SVC 99\n"
        " ST 15,%0 \n"
        " SAM64 \n"
	" SYSSTATE AMODE64=YES \n":
        "=m"(reg15):
        "m"(reqBlock):
        "r1", "r15");
  if (TRACE) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nSVC99.64 at %08x returned %d, feedback=%08x", realRB, reg15, realRB->error);
  }
#else
  __asm(ASM_PREFIX
        " L 1,%1\n"
        " SVC 99\n"
        " ST 15,%0":
        "=m"(reg15):
        "m"(reqBlock):
        "r1", "r15");
  if (TRACE) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\nSVC99.32 at %08x returned %d, feedback=%08x", realRB, reg15, realRB->error);
  }
#endif
  //set the maxcc
  status = (reg15 > status) ? reg15 : status;

  //free the allocated space
  safeFree31((char *)reqBlock,24);
  safeFree31((char *)sv99dd,14);
  safeFree31((char *)sv99dsn,50);
  safeFree31((char *)sv99status2,7);

  //return maxcc
  return status;
}

/**************************************************************  
* This function creates an alternate index to a dataset.  It
*  does so by allocating a temporary QSAM dataset and fills
*  it with the corresponding IDCAMS commands and passes it via
*  branch entry.
* The return value is the maxcc from all commands it processes.
* WARNING:  THIS FUNCTION OVERWRITES EXISTING DATASETS!!!
*           Do not give an aixname that resembles an existing cluster
*           Names generated follow (aixdsn).xxxxx format.
*           If you want more than one AIX give a unique aixname.
* NOTE:  Assumes SMS managed environment
***************************************************************/
int defineAIX(char *clusterdsn, char *aixdsn, char *pathdsn, char* space,
              int recLen, int keyLen, int keyOff, int uniqueKey, int upgrade,
              int debug) {

  int status = 0;

  //format the statement-specific variables
  char integer[10];                           //string representing record length as an integer
  sprintf(integer, "%d", recLen);
  char keyInt[3];                             //string representing key length as an integer
  char keyOffInt[10];                         //string representing key offset as an integer
  if (keyLen > 999) {
    keyLen = 999;
    //printf("Key Length Reduced to maximum 999.");
  }
  if (keyLen) {
    sprintf(keyInt, "%d", keyLen);
  }
  if (keyOff > 999999999) {
    keyOff = 999999999;
    //printf("Key Offset Reduced to maximum 999999999.");
  }
  sprintf(keyOffInt, "%d", keyOff);
  int allocType = 0;
  char remaining[10] = "";
  char leadChar;
  leadChar = *space;
  if (leadChar == 'c' || leadChar == 'C') {        //Cylinders
    allocType = 1;
    strcpy(remaining, space+2);
  } else if (leadChar == 'k' || leadChar == 'K') { //Kilobytes
    allocType = 2;
    strcpy(remaining, space+2);
  } else if (leadChar == 'm' || leadChar == 'M') { //Megabytes
    allocType = 3;
    strcpy(remaining, space+2);
  } else if (leadChar == 'r' || leadChar == 'R') { //Records
    allocType = 4;
    strcpy(remaining, space+2);
  } else if (leadChar == 't' || leadChar == 'T') { //Tracks
    allocType = 5;
    strcpy(remaining, space+2);
  }

  IDCAMSCommand *idcamsCmd = idcamsCreateCommand();
  /* fill the IDCAMS command with the relevant IDCAMS processing options */
  char line[80] = {};
  strcpy(line, " DELETE '");
  strcat(line, aixdsn);
  strcat(line, "'");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, " SET MAXCC = 0                                                                  ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, " DEFINE ALTERNATEINDEX                   -                                      ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "   ( NAME('");
  strcat(line, aixdsn);
  strcat(line, "') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "     RELATE('");
  strcat(line, clusterdsn);
  strcat(line, "') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "     KEYS(");
  strcat(line, keyInt);
  strcat(line, " ");
  strcat(line, keyOffInt);
  strcat(line, ") -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "     RECORDSIZE (");
  strcat(line, integer);
  strcat(line, " ");
  strcat(line, integer);
  strcat(line, ") -");
  idcamsAddLineToCommand(idcamsCmd, line);
  if (allocType == 1) {
    strcpy(line, "     CYLINDERS(");
  } else if (allocType == 2) {
    strcpy(line, "     KILOBYTES(");
  } else if (allocType == 3) {
    strcpy(line, "     MEGABYTES(");
  } else if (allocType == 4) {
    strcpy(line, "     RECORDS(");
  } else if (allocType == 5) {
    strcpy(line, "     TRACKS(");
  }
  if (allocType) {
    strcat(line, remaining);
    strcat(line, ") -");
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  if (uniqueKey) {
    strcpy(line, "     UNIQUEKEY                           -                                      "); 
    idcamsAddLineToCommand(idcamsCmd, line);
  } else {
    strcpy(line, "     NONUNIQUEKEY                        -                                      "); 
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  if (upgrade) {
    strcpy(line, "     UPGRADE                             -                                      "); 
    idcamsAddLineToCommand(idcamsCmd, line);
  } else {
    strcpy(line, "     NOUPGRADE                             -                                      "); 
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  strcpy(line, "   )                                                                            ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, " DEFINE PATH                             -                                      ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "   ( NAME('");
  strcat(line, pathdsn);
  strcat(line, "') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "     PATHENTRY('");
  strcat(line, aixdsn);
  strcat(line, "') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "   )                                                                            ");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, " BLDINDEX IDS('");
  strcat(line, clusterdsn);
  strcat(line, "') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  strcpy(line, "          ODS('");
  strcat(line, aixdsn);
  strcat(line, "') -");
  idcamsAddLineToCommand(idcamsCmd, line);
  if (debug) {
    strcpy(line, "          SORTMESSAGELEVEL(ALL)  -                                              "); 
    idcamsAddLineToCommand(idcamsCmd, line);
  }
  strcpy(line, "          SORTCALL                                                              ");
  idcamsAddLineToCommand(idcamsCmd, line);

  /* invoke IDCAMS */
  IDCAMSCommandOutput *idcamsResult = NULL;
  int idcamsRSN = 0;
  int idcamsRC = idcamsExecuteCommand(idcamsCmd, &idcamsResult, &idcamsRSN);
  //set the maxcc
  /* Since all the code here mixes RCs from different system calls, we'll do
   * our best without a total rewrite of the error handling parts. */
  int idcamsStatus = getEffectiveIDCAMSStatus(idcamsRC, idcamsRSN);
  status = (idcamsStatus > status) ? idcamsStatus : status;

  /* Original code indirectly used the default DD to print IDCAMS message.
   * We'll do the same and print everything using the standard function. */
  if (idcamsResult != NULL) {
    idcamsPrintCommandOutput(idcamsResult);
    idcamsDeleteCommandOutput(idcamsResult);
    idcamsResult = NULL;
  }

  //free the allocated space
  idcamsDeleteCommand(idcamsCmd);
  idcamsCmd = NULL;

  return status;
}

/**************************************************************  
* This function attempts to delete a cluster and all related
*  datasets. It does so by allocating a temporary QSAM dataset
*  and fills it with the corresponding IDCAMS commands and
*  passes it via branch entry.
* The return value is the maxcc from all commands it processes.
***************************************************************/
int deleteCluster(char *dsname) {
  if (TRACE) { 
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "\ndeleteCluster %s", dsname);
  }

  int status = 0;

  IDCAMSCommand *idcamsCmd = idcamsCreateCommand();
  /* fill the IDCAMS command with the relevant IDCAMS processing options */
  char line[80] = {};
  strcpy(line, " DELETE '");
  strcat(line, dsname);
  strcat(line, "'");
  idcamsAddLineToCommand(idcamsCmd, line);

  /* invoke IDCAMS */
  IDCAMSCommandOutput *idcamsResult = NULL;
  int idcamsRSN = 0;
  int idcamsRC = idcamsExecuteCommand(idcamsCmd, &idcamsResult, &idcamsRSN);
  //set the maxcc
  /* Since all the code here mixes RCs from different system calls, we'll do
   * our best without a total rewrite of the error handling parts. */
  int idcamsStatus = getEffectiveIDCAMSStatus(idcamsRC, idcamsRSN);
  status = (idcamsStatus > status) ? idcamsStatus : status;

  /* Original code indirectly used the default DD to print IDCAMS message.
   * We'll do the same and print everything using the standard function. */
  if (idcamsResult != NULL) {
    idcamsPrintCommandOutput(idcamsResult);
    idcamsDeleteCommandOutput(idcamsResult);
    idcamsResult = NULL;
  }

  //free the allocated space
  idcamsDeleteCommand(idcamsCmd);
  idcamsCmd = NULL;

  return status;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

