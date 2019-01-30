

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
#else
#include "stddef.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "nametoken.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS is supported only
#endif

int nameTokenCreate(NameTokenLevel level, const NameTokenUserName *name, const NameTokenUserToken *token) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      char callParmListBuffer[64];
      int level;
      NameTokenUserName name;
      NameTokenUserToken token;
      int persistOption;
      int returnCode;
    )
  );

  parms31->level = level;
  parms31->name = *name;
  parms31->token = *token;
  parms31->persistOption = NAME_TOKEN_PERSIST_OPT_NO_PERSISTS_NO_CHECK_POINT;

  __asm(
      ASM_PREFIX
      "NTPCRT   DS    0H                                                       \n"
      "         LARL  10,NTPCRT                                                \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         USING NTPCRT,10                                                \n"
      "         XGR   15,15                                                    \n"
      "         L     15,X'10'                                                 \n"
      "         L     15,X'220'(15,0)                                          \n"
      "         L     15,X'14'(15,0)                                           \n"
      "         L     15,X'04'(15,0)                                           \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         CALL  (15),((%0),(%1),(%2),(%3),(%4)),MF=(E,(%5))              \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         POP   USING                                                    \n"

      :
      : "r"(&parms31->level), "r"(&parms31->name), "r"(&parms31->token), "r"(&parms31->persistOption),
        "r"(&parms31->returnCode),
        "r"(&parms31->callParmListBuffer)
      : "r10", "r15"
  );

  int returnCode = parms31->returnCode;

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  return returnCode;
}

int nameTokenCreatePersistent(NameTokenLevel level, const NameTokenUserName *name, const NameTokenUserToken *token) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      char callParmListBuffer[64];
      int level;
      NameTokenUserName name;
      NameTokenUserToken token;
      int persistOption;
      int returnCode;
    )
  );

  parms31->level = level;
  parms31->name = *name;
  parms31->token = *token;
  parms31->persistOption = NAME_TOKEN_PERSIST_OPT_PERSIST;

  __asm(
      ASM_PREFIX
      "NTPCRTP  DS    0H                                                       \n"
      "         LARL  10,NTPCRTP                                               \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         USING NTPCRTP,10                                               \n"
      "         XGR   15,15                                                    \n"
      "         L     15,X'10'                                                 \n"
      "         L     15,X'220'(15,0)                                          \n"
      "         L     15,X'14'(15,0)                                           \n"
      "         L     15,X'04'(15,0)                                           \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         CALL  (15),((%0),(%1),(%2),(%3),(%4)),MF=(E,(%5))              \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         POP   USING                                                    \n"

      :
      : "r"(&parms31->level), "r"(&parms31->name), "r"(&parms31->token), "r"(&parms31->persistOption),
        "r"(&parms31->returnCode),
        "r"(&parms31->callParmListBuffer)
      : "r10", "r15"
  );

  int returnCode = parms31->returnCode;

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  return returnCode;
}

int nameTokenDelete(NameTokenLevel level, const NameTokenUserName *name) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      char callParmListBuffer[64];
      int level;
      NameTokenUserName name;
      int returnCode;
    )
  );

  parms31->level = level;
  parms31->name = *name;

  __asm(
      ASM_PREFIX
      "NTPDEL   DS    0H                                                       \n"
      "         LARL  10,NTPDEL                                                \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         USING NTPDEL,10                                                \n"
      "         XGR   15,15                                                    \n"
      "         L     15,X'10'                                                 \n"
      "         L     15,X'220'(15,0)                                          \n"
      "         L     15,X'14'(15,0)                                           \n"
      "         L     15,X'0C'(15,0)                                           \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         CALL  (15),((%0),(%1),(%2)),MF=(E,(%3))                        \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         POP   USING                                                    \n"

      :
      : "r"(&parms31->level), "r"(&parms31->name),
        "r"(&parms31->returnCode),
        "r"(&parms31->callParmListBuffer)
      : "r10", "r15"
  );

  int returnCode = parms31->returnCode;

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  return returnCode;
}

int nameTokenRetrieve(NameTokenLevel level, const NameTokenUserName *name, NameTokenUserToken *token) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      char callParmListBuffer[64];
      int level;
      NameTokenUserName name;
      NameTokenUserToken token;
      int returnCode;
    )
  );

  parms31->level = level;
  parms31->name = *name;

  __asm(
      ASM_PREFIX
      "NTPRTR   DS    0H                                                       \n"
      "         LARL  10,NTPRTR                                                \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         USING NTPRTR,10                                                \n"
      "         XGR   15,15                                                    \n"
      "         L     15,X'10'                                                 \n"
      "         L     15,X'220'(15,0)                                          \n"
      "         L     15,X'14'(15,0)                                           \n"
      "         L     15,X'08'(15,0)                                           \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         CALL  (15),((%0),(%1),(%2),(%3)),MF=(E,(%4))                   \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         POP   USING                                                    \n"

      :
      : "r"(&parms31->level), "r"(&parms31->name), "r"(&parms31->token),
        "r"(&parms31->returnCode),
        "r"(&parms31->callParmListBuffer)
      : "r10", "r15"
  );

  int returnCode = parms31->returnCode;
  if (returnCode == RC_NAMETOKEN_OK) {
    *token = parms31->token;
  }

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  return returnCode;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

