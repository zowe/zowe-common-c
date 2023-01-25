
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
#include <metal/stdbool.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "rusermap.h"

#define MAP_USERID_TO_LOTUS_NOTES_ID 0x0001  /* Lotus Notes, hahahahahahaha */
#define MAP_LOTUS_NOTES_ID_TO_USERID 0x0002

#define MAP_CERTIFICATE_TO_USERID 0x0006

#define MAX_CERT_SIZE 4096

#define IRRSIM00_WORKAREA_LENGTH 1024


/* last arg must be at least 9 chars in length */
int getUseridByCertificate(char *certificate, int certificateLength, char *useridBuffer,
                           int *racfRC, int *racfReason){

#pragma pack(packed)
  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS
    (
     Addr31   parmlistStorage[15];
     uint32_t zero; /* must be initted to zero, used for all the ALET's and NULL pointers */
     uint32_t highbitZero;
     uint32_t safRC;
     uint32_t racfRC;
     uint32_t racfReason;
     uint16_t functionCode;
     uint8_t  reserved;
     uint8_t  useridLength;
     char     userid[8];
     char     workArea[IRRSIM00_WORKAREA_LENGTH];
     uint32_t certificateLength;
     char     certificate[MAX_CERT_SIZE];
     )
    );
#pragma pack(reset)
  
  memset(parms31,0,sizeof(*parms31));
  parms31->functionCode = MAP_CERTIFICATE_TO_USERID;
  parms31->highbitZero = 0x80000000;
  parms31->useridLength = 0;
  if (certificateLength > MAX_CERT_SIZE){
    return 12;
  } else{
    memcpy(parms31->certificate, certificate, certificateLength);
    parms31->certificateLength = certificateLength;
  }

  int rc = 0;
  /*
  printf(" before call parms31Len=0x%x\n",(int)sizeof(*parms31));fflush(stdout);
  dumpbuffer((char*)parms31,sizeof(*parms31));
  */
  __asm(ASM_PREFIX
        " LLGT 15,X'10'(,0) \n"   /* Get the CVT */
        " LLGT 15,X'220'(,15) \n" /* CSRTABLE */
        " LLGT 15,X'28'(,15) \n"  /* Some RACF Routin Vector */
        " LLGT 15,X'A0'(,15) \n"  /* IRRSIM00 itself */
#ifdef _LP64
        " SAM31 \n"
        " SYSSTATE AMODE64=NO \n"
#endif
        " CALL (15),(%[wa]"
        ",%[z],%[safRC]"
        ",%[z],%[racfRC]"
        ",%[z],%[racfRSN]"
        ",%[z],%[fc]"
        ",%[z]"   /* options */
        ",%[userid]"
        ",%[cert]"
        ",%[z]"   /* appl userid */
        ",%[z]"   /* Distinguished name */
        ",%[z])" /* registry name, which is NULL with high bit set */
        ",VL,MF=(E,%[parmlist]) \n"

#ifdef _LP64
        " SAM64 \n"
        " SYSSTATE AMODE64=YES \n"
#endif
        " ST 15,%[rc]"
        : [rc]"=m"(rc)
        : [wa]"m"(parms31->workArea),
          [z]"m"(parms31->zero),
          [highbit]"m"(parms31->highbitZero),
          [fc]"m"(parms31->functionCode),
          [safRC]"m"(parms31->safRC),
          [racfRC]"m"(parms31->racfRC),
          [racfRSN]"m"(parms31->racfReason),
          [userid]"m"(parms31->useridLength),   /* must point at pre-pended length */
          [cert]"m"(parms31->certificateLength),
          [parmlist]"m"(parms31->parmlistStorage)
        :"r14","r15");
  

  int safRC = parms31->safRC;
  *racfRC = parms31->racfRC;
  *racfReason = parms31->racfReason;

  if (safRC == 0){
    memcpy(useridBuffer,parms31->userid,parms31->useridLength);
  }

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );
  
  return safRC;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
