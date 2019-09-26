

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
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "zowetypes.h"
#include "isgenq.h"

#if !defined(METTLE) && defined(_LP64)
/* TODO the ISGENQ code may not work under LE 64-bit */
#error LE 64-bit is not supported
#endif

#ifdef METTLE
__asm("GLBENQPL    ISGENQ MF=(L,GLBENQPL)" : "DS"(GLBENQPL));
#endif

int isgenqTryExclusiveLock(const QName *qname,
                           const RName *rname,
                           uint8_t scope,
                           ENQToken *token,
                           int *reasonCode) {

  QName localQName = *qname;
  RName localRName = *rname;

#ifdef METTLE
  __asm(" ISGENQ MF=L" : "DS"(isgenqParmList));
  isgenqParmList = GLBENQPL;
#else
  char isgenqParmList[512];
#endif

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQOBTX  LARL  2,ENQOBTX                                                \n"
      "         USING ENQOBTX,2                                                \n"
      "         ISGENQ   REQUEST=OBTAIN"
      ",TEST=NO"
      ",CONTENTIONACT=FAIL"
      ",USERDATA=NO_USERDATA"
      ",RESLIST=NO"
      ",QNAME=(%2)"
      ",RNAME=(%3)"
      ",RNAMELEN=(%4)"
      ",CONTROL=EXCLUSIVE"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%5)"
      ",RNL=YES"
      ",ENQTOKEN=(%6)"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",PLISTVER=IMPLIED_VERSION"
      ",MF=(E,(%7),COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"
      : "=m"(rc), "=m"(rsn)
      : "r"(&localQName.value), "r"(&localRName.value), "r"(&localRName.length),
        "r"(&scope), "r"(token), "r"(&isgenqParmList)
      : "r0", "r1", "r2", "r14", "r15"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}

int isgenqGetExclusiveLock(const QName *qname,
                           const RName *rname,
                           uint8_t scope,
                           ENQToken *token,
                           int *reasonCode) {

  QName localQName = *qname;
  RName localRName = *rname;

#ifdef METTLE
  __asm(" ISGENQ MF=L" : "DS"(isgenqParmList));
  isgenqParmList = GLBENQPL;
#else
  char isgenqParmList[512];
#endif

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQOBTXW LARL  2,ENQOBTXW                                               \n"
      "         USING ENQOBTXW,2                                               \n"
      "         ISGENQ   REQUEST=OBTAIN"
      ",TEST=NO"
      ",CONTENTIONACT=WAIT"
      ",USERDATA=NO_USERDATA"
      ",RESLIST=NO"
      ",QNAME=(%2)"
      ",RNAME=(%3)"
      ",RNAMELEN=(%4)"
      ",CONTROL=EXCLUSIVE"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%5)"
      ",RNL=YES"
      ",ENQTOKEN=(%6)"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",PLISTVER=IMPLIED_VERSION"
      ",MF=(E,(%7),COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"
      : "=m"(rc), "=m"(rsn)
      : "r"(&localQName.value), "r"(&localRName.value), "r"(&localRName.length),
        "r"(&scope), "r"(token), "r"(&isgenqParmList)
      : "r0", "r1", "r2", "r14", "r15"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}

int isgenqTrySharedLock(const QName *qname,
                        const RName *rname,
                        uint8_t scope,
                        ENQToken *token,
                        int *reasonCode) {

  QName localQName = *qname;
  RName localRName = *rname;

#ifdef METTLE
  __asm(" ISGENQ MF=L" : "DS"(isgenqParmList));
  isgenqParmList = GLBENQPL;
#else
  char isgenqParmList[512];
#endif

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQOBTS  LARL  2,ENQOBTS                                                \n"
      "         USING ENQOBTS,2                                                \n"
      "         ISGENQ   REQUEST=OBTAIN"
      ",TEST=NO"
      ",CONTENTIONACT=FAIL"
      ",USERDATA=NO_USERDATA"
      ",RESLIST=NO"
      ",QNAME=(%2)"
      ",RNAME=(%3)"
      ",RNAMELEN=(%4)"
      ",CONTROL=SHARED"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%5)"
      ",RNL=YES"
      ",ENQTOKEN=(%6)"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",PLISTVER=IMPLIED_VERSION"
      ",MF=(E,(%7),COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"
      : "=m"(rc), "=m"(rsn)
      : "r"(&localQName.value), "r"(&localRName.value), "r"(&localRName.length),
        "r"(&scope), "r"(token), "r"(&isgenqParmList)
      : "r0", "r1", "r2", "r14", "r15"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}

int isgenqGetSharedLock(const QName *qname,
                        const RName *rname,
                        uint8_t scope,
                        ENQToken *token,
                        int *reasonCode) {

  QName localQName = *qname;
  RName localRName = *rname;

#ifdef METTLE
  __asm(" ISGENQ MF=L" : "DS"(isgenqParmList));
  isgenqParmList = GLBENQPL;
#else
  char isgenqParmList[512];
#endif

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQOBTSW LARL  2,ENQOBTSW                                               \n"
      "         USING ENQOBTSW,2                                               \n"
      "         ISGENQ   REQUEST=OBTAIN"
      ",TEST=NO"
      ",CONTENTIONACT=WAIT"
      ",USERDATA=NO_USERDATA"
      ",RESLIST=NO"
      ",QNAME=(%2)"
      ",RNAME=(%3)"
      ",RNAMELEN=(%4)"
      ",CONTROL=SHARED"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%5)"
      ",RNL=YES"
      ",ENQTOKEN=(%6)"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",PLISTVER=IMPLIED_VERSION"
      ",MF=(E,(%7),COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"
      : "=m"(rc), "=m"(rsn)
      : "r"(&localQName.value), "r"(&localRName.value), "r"(&localRName.length),
        "r"(&scope), "r"(token), "r"(&isgenqParmList)
      : "r0", "r1", "r2", "r14", "r15"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}

int isgenqTestLock(const QName *qname,
                   const RName *rname,
                   uint8_t scope,
                   int *reasonCode) {

  QName localQName = *qname;
  RName localRName = *rname;

#ifdef METTLE
  __asm(" ISGENQ MF=L" : "DS"(isgenqParmList));
  isgenqParmList = GLBENQPL;
#else
  char isgenqParmList[512];
#endif

  ENQToken localToken;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQOBTXT LARL  2,ENQOBTXT                                               \n"
      "         USING ENQOBTXT,2                                               \n"
      "         ISGENQ   REQUEST=OBTAIN"
      ",TEST=YES"
      ",RESLIST=NO"
      ",QNAME=(%2)"
      ",RNAME=(%3)"
      ",RNAMELEN=(%4)"
      ",CONTROL=EXCLUSIVE"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%5)"
      ",RNL=YES"
      ",ENQTOKEN=(%6)"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",PLISTVER=IMPLIED_VERSION"
      ",MF=(E,(%7),COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"
      : "=m"(rc), "=m"(rsn)
      : "r"(&localQName.value), "r"(&localRName.value), "r"(&localRName.length),
        "r"(&scope), "r"(&localToken), "r"(&isgenqParmList)
      : "r0", "r1", "r2", "r14", "r15"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}

int isgenqTestExclusiveLock(const QName *qname,
                            const RName *rname,
                            uint8_t scope,
                            int *reasonCode) {

  return isgenqTestLock(qname, rname, scope, reasonCode);

}

int isgenqTestSharedLock(const QName *qname,
                         const RName *rname,
                         uint8_t scope,
                         int *reasonCode) {

  QName localQName = *qname;
  RName localRName = *rname;

#ifdef METTLE
  __asm(" ISGENQ MF=L" : "DS"(isgenqParmList));
  isgenqParmList = GLBENQPL;
#else
  char isgenqParmList[512];
#endif

  ENQToken localToken;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQOBTST LARL  2,ENQOBTST                                               \n"
      "         USING ENQOBTST,2                                               \n"
      "         ISGENQ   REQUEST=OBTAIN"
      ",TEST=YES"
      ",RESLIST=NO"
      ",QNAME=(%2)"
      ",RNAME=(%3)"
      ",RNAMELEN=(%4)"
      ",CONTROL=SHARED"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%5)"
      ",RNL=YES"
      ",ENQTOKEN=(%6)"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",PLISTVER=IMPLIED_VERSION"
      ",MF=(E,(%7),COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"
      : "=m"(rc), "=m"(rsn)
      : "r"(&localQName.value), "r"(&localRName.value), "r"(&localRName.length),
        "r"(&scope), "r"(&localToken), "r"(&isgenqParmList)
      : "r0", "r1", "r2", "r14", "r15"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}

int isgenqReleaseLock(ENQToken *token, int *reasonCode) {

#ifdef METTLE
  __asm(" ISGENQ MF=L" : "DS"(isgenqParmList));
  isgenqParmList = GLBENQPL;
#else
  char isgenqParmList[512];
#endif

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQREL   LARL  2,ENQREL                                                 \n"
      "         USING ENQREL,2                                                 \n"
      "         ISGENQ   REQUEST=RELEASE"
      ",RESLIST=NO"
      ",ENQTOKEN=(%2)"
      ",OWNINGTTOKEN=CURRENT_TASK"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",PLISTVER=IMPLIED_VERSION"
      ",MF=(E,(%3),COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"
      : "=m"(rc), "=m"(rsn)
      : "r"(token), "r"(&isgenqParmList)
      : "r0", "r1", "r2", "r14", "r15"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

