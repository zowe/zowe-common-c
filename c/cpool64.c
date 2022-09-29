

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
#include <metal/stdint.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "cpool64.h"
#include "zos.h"

#define IARCP64_PARMLIST_LENGTH 72

uint64_t iarcp64Create(bool isCommon, int cellSize, int *returnCodePtr, int *reasonCodePtr){
  uint64_t cpid = 0;
  char header[24];
  int  returnCode = 0;
  int  reasonCode = 0;
  char parmlist[IARCP64_PARMLIST_LENGTH];
  if (isCommon){
    __asm(ASM_PREFIX
	  " IARCP64 REQUEST=BUILD,CELLSIZE=%3,OUTPUT_CPID=%0,"
	  "HEADER=%1,COMMON=YES,OWNER=HOME,DUMP=NO,FPROT=NO,"
	  "TYPE=PAGEABLE,CALLERKEY=YES,TRAILER=NO,FAILMODE=RC,"
	  "RETCODE=%4,RSNCODE=%5,MF=(E,%2,COMPLETE) \n"
	  :
	  :
	  "m"(cpid),"m"(header),"m"(parmlist),"m"(cellSize),"m"(returnCode),"m"(reasonCode)
	  );
  } else{
    __asm(ASM_PREFIX
	  " IARCP64 REQUEST=BUILD,CELLSIZE=%3,OUTPUT_CPID=%0,"
	  "HEADER=%1,COMMON=NO,OWNINGTASK=CMRO,DUMP=NO,FPROT=NO,"
	  "TYPE=PAGEABLE,CALLERKEY=YES,TRAILER=NO,FAILMODE=RC,"
	  "RETCODE=%4,RSNCODE=%5,MF=(E,%2,COMPLETE) \n"
	  :
	  :
	  "m"(cpid),"m"(header),"m"(parmlist),"m"(cellSize),"m"(returnCode),"m"(reasonCode)
	  );
  }
  *returnCodePtr = returnCode;
  *reasonCodePtr = reasonCode;
  return cpid;
}

void *iarcp64Get(uint64_t cpid, int *returnCodePtr, int *reasonCodePtr){
  uint64_t cellAddr = 0;
  int  returnCode = 0;
  int  reasonCode = 0;
  char f4sa[160];
  __asm(ASM_PREFIX
	" LAE 13,%4 \n"   /* Required by REG=SAVE */
	" IARCP64 REQUEST=GET,INPUT_CPID=%3,REGS=SAVE,"
	"CELLADDR=%0,TRACE=NO,EXPAND=YES,FAILMODE=RC,"
	"RETCODE=%1,RSNCODE=%2 \n"
	:
	"=m"(cellAddr),"=m"(returnCode),"=m"(reasonCode)
	:
	"m"(cpid),"m"(f4sa)
	);
  *returnCodePtr = returnCode;
  *reasonCodePtr = reasonCode;
  return (void*)cellAddr;
}

void iarcp64Free(uint64_t cpid, void *cellAddr){
  char f4sa[160];
  __asm(ASM_PREFIX
	" LAE 13,%2 \n"   /* Required by REG=SAVE */
	" IARCP64 REQUEST=FREE,INPUT_CPID=%1,REGS=SAVE,"
	"CELLADDR=%0 \n"
	:
	:
	"m"(cellAddr),"m"(cpid),"m"(f4sa)
	);
}

int iarcp64Delete(uint64_t cpid, int *reasonCodePtr){
  int  returnCode = 0;
  int  reasonCode = 0;
  __asm(ASM_PREFIX
	" IARCP64 REQUEST=DELETE,INPUT_CPID=%2 \n"
	" ST 15,%0 \n"
	" ST 0,%1 "
	:
	"=m"(returnCode),"=m"(reasonCode)
	:
	"m"(cpid)
	);
  *reasonCodePtr = reasonCode;
  return returnCode;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

