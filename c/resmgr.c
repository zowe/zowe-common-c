

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
#include "metalio.h"
#else
#include "stddef.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "resmgr.h"
#include "zos.h"
#include "qsam.h"
#include "utils.h"

#if !defined(__ZOWE_OS_ZOS)
#error "unsupported platform"
#endif

#if !defined(METTLE)
#error "LE not supported"
#endif

ZOWE_PRAGMA_PACK

typedef struct ResourceManagerWrapperParmList_tag {
  char eyecatcher[8];
#define RESMGR_WRAPPER_PARMS_EYECATCHER  "RSRSMGWP"
  ResourceManagerRouinte * __ptr32 userRoutine;
  void * __ptr32 userData;
  void * __ptr32 userFunctionStack;
  unsigned int userFunctionStackSize;
#define RESMGR_USER_FUNCTION_STACK_SIZE 65536
} ResourceManagerWrapperParmList;

typedef struct ResourceManagerUserParm_tag {
  union {
    ResourceManagerWrapperParmList * __ptr32 wrapperParmList;
    void * __ptr32 userData;
  };
  unsigned char pswKey;
  char reserved[3];
} ResourceManagerUserParm;

ZOWE_PRAGMA_PACK_RESET

static Addr31 getResourceManagerWrapper() {

  Addr31 wrapper = NULL;

  __asm(
      ASM_PREFIX
      "         LARL  1,L$RMW                                                  \n"
      "         ST    1,%0                                                     \n"
      "         J     L$RMWEX                                                  \n"
      "L$RMW    DS    0H                                                       \n"

      /* establish addressability */
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  10,L$RMW                                                 \n"
      "         USING L$RMW,10                                                 \n"
      /* save input */
      "         LTR   1,1                 are parms from system zero?          \n"
      "         BZ    L$RMWRT             yes, leave                           \n"
      "         USING RMPL,1                                                   \n"
      /* handle input parms */
      "         L     3,RMPLUPP           user provided field address          \n"
      "         LTR   3,3                 is it zero?                          \n"
      "         BZ    L$RMWRT             yes, leave                           \n"
      "         USING RMWUPRM,3                                                \n"
      "         ICM   2,X'0001',RMWUPKEY  load PSW key                         \n"
      "         SPKA  0(2)                change key                           \n"
      "         L     4,RMWUPWPR          wrapper parms                        \n"
      "         LTR   4,4                 is it zero?                          \n"
      "         BZ    L$RMWRT             yes, leave                           \n"
      "         USING RMWPL,4                                                  \n"
      "         CLC   RMWPLEYE,=C'"RESMGR_WRAPPER_PARMS_EYECATCHER"' \n"
      "         BNE   L$RMWRT             bad eyecatcher, leave                \n"
      /* handle stack */
      "         L     13,RMWPLSTK         load pre-allocated stack             \n"
      "         LTR   13,13               is it zero?                          \n"
      "         BZ    L$RMWRT             yes, leave                           \n"
      "         USING RMWSA,13                                                 \n"
#ifdef _LP64
      "         MVC   RMWSAEYE,=C'F1SA'   stack eyecatcher                     \n"
#endif
      "         LA    5,RMWSANSA          address of next save area            \n"
      "         ST    5,RMWSANXT          save next save area address          \n"
      "         MVC   RMWSARMP,RMPLPRM    resource manager parameter list      \n"
      "         MVC   RMWSAUP,RMWPLPRV    user data                            \n"
      "         LA    5,RMWSARSN          address of reason code field         \n"
      "         ST    5,RMWSARSA          save rsn address for user routine    \n"
      /* call user routine */
      "         L     15,RMWPLGPR         user routine                         \n"
      "         LTR   15,15               is it zero?                          \n"
      "         BZ    L$RMWRT             yes, leave                           \n"
      "         LA    1,RMWSAPRM          load pamlist address to R1           \n"
#ifdef _LP64
      "         SAM64                     go AMODE64 if needed                 \n"
#endif
      "         BASR  14,15               call user function                   \n"
#ifdef _LP64
      "         SAM31                     go back to AMODE31                   \n"
#endif
      "         L     0,RMWSARSN          reason code for system               \n"
      /* return to caller */
      "L$RMWRT  DS    0H                                                       \n"
      "         EREG  1,1                 R1 must be restored                  \n"
      "         PR    ,                   return                               \n"
      /* non executable code */
      "         LTORG                                                          \n"
      "         POP   USING                                                    \n"

      "L$RMWEX  DS    0H                                                       \n"
      : "=m"(wrapper)
      :
      : "r1"
  );

  return wrapper;
}

static unsigned char getPSWKey() {

  unsigned char key = 0;
  __asm(
      ASM_PREFIX
      "         IPK   ,                                                        \n"
      "         N     2,=X'000000F0'                                           \n"
      "         STC   2,%0                                                     \n"
      : "=m"(key)
      :
      : "r2"
  );

  return key;
}

int resmgrAddTaskResourceManager(ResourceManagerRouinte * __ptr32 userRoutine, void * __ptr32 userData,
                                 ResourceManagerHandle * __ptr32 handle, int *managerRC) {

  char managerParmList[1024];
  memset(managerParmList, 0, sizeof(managerParmList));

  Addr31 wrapperRoutine = getResourceManagerWrapper();

  ResourceManagerWrapperParmList * __ptr32 wrapperParms =
      (ResourceManagerWrapperParmList *)safeMalloc31(sizeof(ResourceManagerWrapperParmList), "ResourceManagerWrapperParmList");
  memcpy(wrapperParms->eyecatcher, RESMGR_WRAPPER_PARMS_EYECATCHER, sizeof(wrapperParms->eyecatcher));
  wrapperParms->userRoutine = userRoutine;
  wrapperParms->userData = userData;
  wrapperParms->userFunctionStackSize = RESMGR_USER_FUNCTION_STACK_SIZE;
  wrapperParms->userFunctionStack = safeMalloc31(wrapperParms->userFunctionStackSize, "Resmgr user function stack");

  memset(handle, 0, sizeof(ResourceManagerHandle));
  memcpy(handle->eyecatcher, RESMGR_HANDLE_EYECATCHER, sizeof(handle->eyecatcher));
  handle->wrapperParms = wrapperParms;

  ResourceManagerUserParm userParm;
  memset(&userParm, 0, sizeof(ResourceManagerUserParm));
  userParm.wrapperParmList = wrapperParms;
  userParm.pswKey = getPSWKey();

  int wasProblemState = supervisorMode(TRUE);

  int returnCode = 0;
  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         RESMGR ADD,TOKEN=(%1),TYPE=TASK,ASID=CURRENT,TCB=CURRENT"
      ",ROUTINE=(BRANCH,(%2))"
      ",PARAM=(%3)"
      ",MF=(E,(%4))                                                            \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ST    15,%0                                                    \n"
      : "=m"(returnCode)
      : "r"(&handle->token), "r"(wrapperRoutine), "r"(&userParm), "r"(&managerParmList)
      : "r0", "r1", "r14", "r15"
  );

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  *managerRC = returnCode;
  if (returnCode > 4) {
    safeFree31((char *)handle->wrapperParms, sizeof(ResourceManagerWrapperParmList));
    memset(handle, 0, sizeof(ResourceManagerHandle));
    return RC_RESMGR_SERVICE_FAILED;
  }

  return RC_RESMGR_OK;
}

int resmgrAddAddressSpaceResourceManager(unsigned short asid, void * __ptr32 userRoutine, void * __ptr32 userData,
                                         ResourceManagerHandle * __ptr32 handle, int *managerRC) {

  char managerParmList[1024];
  memset(managerParmList, 0, sizeof(managerParmList));

  Addr31 wrapperRoutine = getResourceManagerWrapper();

  memset(handle, 0, sizeof(ResourceManagerHandle));
  memcpy(handle->eyecatcher, RESMGR_HANDLE_EYECATCHER, sizeof(handle->eyecatcher));
  handle->wrapperParms = userData;
  handle->asid = asid;

  ResourceManagerUserParm userParm;
  memset(&userParm, 0, sizeof(ResourceManagerUserParm));
  userParm.userData = userData;
  userParm.pswKey = getPSWKey();

  int wasProblemState = supervisorMode(TRUE);

  int returnCode = 0;
  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         RESMGR ADD,TOKEN=(%1),TYPE=ADDRSPC,ASID=(%2)"
      ",ROUTINE=(BRANCH,(%3))"
      ",PARAM=(%4)"
      ",MF=(E,(%5))                                                            \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ST    15,%0                                                    \n"
      : "=m"(returnCode)
      : "r"(&handle->token), "r"(asid), "r"(userRoutine), "r"(&userParm), "r"(&managerParmList)
      : "r0", "r1", "r14", "r15"
  );

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  *managerRC = returnCode;
  if (returnCode > 4) {
    safeFree31((char *)handle->wrapperParms, sizeof(ResourceManagerWrapperParmList));
    memset(handle, 0, sizeof(ResourceManagerHandle));
    return RC_RESMGR_SERVICE_FAILED;
  }

  return RC_RESMGR_OK;
}

int resmgrDeleteTaskResourceManager(ResourceManagerHandle * __ptr32 handle, int *managerRC) {

  char managerParmList[1024];
  memset(managerParmList, 0, sizeof(managerParmList));

  int wasProblemState = supervisorMode(TRUE);

  int returnCode = 0;
  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         RESMGR DELETE,TOKEN=(%1),TYPE=TASK,ASID=CURRENT,TCB=CURRENT"
      ",MF=(E,(%2))                                                            \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ST    15,%0                                                    \n"
      : "=m"(returnCode)
      : "r"(&handle->token), "r"(&managerParmList)
      : "r0", "r1", "r14", "r15"
  );

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  *managerRC = returnCode;
  if (returnCode > 8) {
    return RC_RESMGR_SERVICE_FAILED;
  } else {
    ResourceManagerWrapperParmList *wrapperParms = handle->wrapperParms;
    safeFree31(wrapperParms->userFunctionStack, wrapperParms->userFunctionStackSize);
    wrapperParms->userFunctionStack = NULL;
    safeFree31((char *)handle->wrapperParms, sizeof(ResourceManagerWrapperParmList));
    wrapperParms = NULL;
    memset(handle, 0, sizeof(ResourceManagerHandle));
  }

  return RC_RESMGR_OK;
}

int resmgrDeleteAddressSpaceResourceManager(ResourceManagerHandle * __ptr32 handle, int *managerRC) {

  char managerParmList[1024];
  memset(managerParmList, 0, sizeof(managerParmList));

  int wasProblemState = supervisorMode(TRUE);

  int returnCode = 0;
  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         RESMGR DELETE,TOKEN=(%1),TYPE=ADDRSPC,ASID=(%2)"
      ",MF=(E,(%3))                                                            \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         ST    15,%0                                                    \n"
      : "=m"(returnCode)
      : "r"(&handle->token), "r"(handle->asid), "r"(&managerParmList)
      : "r0", "r1", "r14", "r15"
  );

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  *managerRC = returnCode;
  if (returnCode > 8) {
    return RC_RESMGR_SERVICE_FAILED;
  } else {
    memset(handle, 0, sizeof(ResourceManagerHandle));
  }

  return RC_RESMGR_OK;
}

#define resmgrDESCTs RSMDSECT
void resmgrDESCTs(){

  __asm(
#ifndef _LP64
      "         DS    0D                                                       \n"
      "RMWSA    DSECT ,                                                        \n"
      /* 31-bit save area */
      "RMWSARSV DS    CL4                                                      \n"
      "RMWSAPRV DS    A                    previous save area                  \n"
      "RMWSANXT DS    A                    next save area                      \n"
      "RMWSAGPR DS    15F                  GPRs                                \n"
      /* parameters on stack */
      "RMWSAPRM DS    0F                   parameter block for Metal C         \n"
      "RMWSARMP DS    F                    resource manager parm list          \n"
      "RMWSAUP  DS    F                    user parms                          \n"
      "RMWSARSA DS    F                    resource manager reason code address\n"
      "RMWSARSN DS    F                    resource manager reason code        \n"
      "RMWSALEN EQU   *-RMWSA                                                  \n"
      "RMWSANSA DS    0F                   top of next save area               \n"
      "         EJECT ,                                                        \n"
#else
      "         DS    0D                                                       \n"
      "RMWSA    DSECT ,                                                        \n"
      /* 64-bit save area */
      "RMWSARSV DS    CL4                                                      \n"
      "RMWSAEYE DS    CL4                  eyecatcher F1SA                     \n"
      "RMWSAGPR DS    15D                  GPRs                                \n"
      "RMWSAPD1 DS    F                    padding                             \n"
      "RMWSAPRV DS    A                    previous save area                  \n"
      "RMWSAPD2 DS    F                    padding                             \n"
      "RMWSANXT DS    A                    next save area                      \n"
      /* parameters on stack */
      "RMWSAPRM DS    0F                   parameter block for Metal C         \n"
      "RMWSAPD3 DS    F                    padding                             \n"
      "RMWSARMP DS    F                    resource manager parm list          \n"
      "RMWSAPD4 DS    F                    padding                             \n"
      "RMWSAUP  DS    F                    user parms                          \n"
      "RMWSAPD5 DS    F                    padding                             \n"
      "RMWSARSA DS    F                    resource manager reason code address\n"
      "RMWSARSN DS    F                    resource manager reason code        \n"
      "RMWSALEN EQU   *-RMWSA                                                  \n"
      "RMWSANSA DS    0D                   top of next save area               \n"
      "         EJECT ,                                                        \n"
#endif

      "         DS    0D                                                       \n"
      "RMPL     DSECT ,                                                        \n"
      "RMPLPRM  DS    A                    resource manager parm list          \n"
      "RMPLUPP  DS    A                    address of 8-byte user provided parm\n"
      "RMPLLEN  EQU   *-RMPL                                                   \n"
      "         EJECT ,                                                        \n"

      "         DS    0D                                                       \n"
      "RMWPL    DSECT ,                                                        \n"
      "RMWPLEYE DS    CL8                  eyecatcher                          \n"
      "RMWPLGPR DS    A                    user routine                        \n"
      "RMWPLPRV DS    A                    user data                           \n"
      "RMWPLSTK DS    A                    user routine stack                  \n"
      "RMWPLSTS DS    A                    user routine stack size             \n"
      "RMWPLLEN EQU   *-RMWPL                                                  \n"
      "         EJECT ,                                                        \n"

      "         DS    0D                                                       \n"
      "RMWUPRM  DSECT ,                                                        \n"
      "RMWUPWPR DS    A                    wrapper parm list RMWPL             \n"
      "RMWUPKEY DS    X                    PSW key                             \n"
      "RMWUPRSV DS    3X                   reserved                            \n"
      "RMWUPLEN EQU   *-RMWUPRM                                                \n"
      "         EJECT ,                                                        \n"

      "         DS    0H                                                       \n"
      "         IHASDWA                                                        \n"
      "         EJECT ,                                                        \n"

      "         CVT   DSECT=YES,LIST=NO                                        \n"
      "         EJECT ,                                                        \n"

#ifdef METTLE
      "         CSECT ,                                                        \n"
      "         RMODE ANY                                                      \n"
#endif
  );

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

