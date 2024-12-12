

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


int isgenqTryExclusiveLock(const QName *qname,
                           const RName *rname,
                           uint8_t scope,
                           ENQToken *token,
                           int *reasonCode) {

  char parmList[200] = {0};

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
      ",QNAME=%[qname]"
      ",RNAME=%[rname]"
      ",RNAMELEN=%[rname_len]"
      ",CONTROL=EXCLUSIVE"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%[scope])"
      ",RNL=YES"
      ",ENQTOKEN=%[token]"
      ",COND=YES"
      ",RETCODE=GPR15"
      ",RSNCODE=GPR00"
      ",PLISTVER=2"
      ",MF=(E,%[parmlist],COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"

      : [token]"=m"(*token), [rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)

      : [qname]"m"(qname->value),
        [rname]"m"(rname->value),
        [rname_len]"m"(rname->length),
        [scope]"r"(&scope),
        [parmlist]"m"(parmList)

      : "r1", "r2", "r14"
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

  char parmList[200] = {0};

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
      ",QNAME=%[qname]"
      ",RNAME=%[rname]"
      ",RNAMELEN=%[rname_len]"
      ",CONTROL=EXCLUSIVE"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%[scope])"
      ",RNL=YES"
      ",ENQTOKEN=%[token]"
      ",COND=YES"
      ",RETCODE=GPR15"
      ",RSNCODE=GPR00"
      ",PLISTVER=2"
      ",MF=(E,%[parmlist],COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"

      : [token]"=m"(*token), [rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)

      : [qname]"m"(qname->value),
        [rname]"m"(rname->value),
        [rname_len]"m"(rname->length),
        [scope]"r"(&scope),
        [parmlist]"m"(parmList)

      : "r1", "r2", "r14"
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

  char parmList[200] = {0};

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
      ",QNAME=%[qname]"
      ",RNAME=%[rname]"
      ",RNAMELEN=%[rname_len]"
      ",CONTROL=SHARED"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%[scope])"
      ",RNL=YES"
      ",ENQTOKEN=%[token]"
      ",COND=YES"
      ",RETCODE=GPR15"
      ",RSNCODE=GPR00"
      ",PLISTVER=2"
      ",MF=(E,%[parmlist],COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"

      : [token]"=m"(*token), [rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)

      : [qname]"m"(qname->value),
        [rname]"m"(rname->value),
        [rname_len]"m"(rname->length),
        [scope]"r"(&scope),
        [parmlist]"m"(parmList)

      : "r1", "r2", "r14"
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

  char parmList[200] = {0};

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
      ",QNAME=%[qname]"
      ",RNAME=%[rname]"
      ",RNAMELEN=%[rname_len]"
      ",CONTROL=SHARED"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%[scope])"
      ",RNL=YES"
      ",ENQTOKEN=%[token]"
      ",COND=YES"
      ",RETCODE=GPR15"
      ",RSNCODE=GPR00"
      ",PLISTVER=2"
      ",MF=(E,%[parmlist],COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"

      : [token]"=m"(*token), [rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)

      : [qname]"m"(qname->value),
        [rname]"m"(rname->value),
        [rname_len]"m"(rname->length),
        [scope]"r"(&scope),
        [parmlist]"m"(parmList)

      : "r1", "r2", "r14"
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

  ENQToken token = {0};
  char parmList[200] = {0};

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
      ",QNAME=%[qname]"
      ",RNAME=%[rname]"
      ",RNAMELEN=%[rname_len]"
      ",CONTROL=EXCLUSIVE"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%[scope])"
      ",RNL=YES"
      ",ENQTOKEN=%[token]"
      ",COND=YES"
      ",RETCODE=GPR15"
      ",RSNCODE=GPR00"
      ",PLISTVER=2"
      ",MF=(E,%[parmlist],COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"

      : [token]"=m"(token), [rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)

      : [qname]"m"(qname->value),
        [rname]"m"(rname->value),
        [rname_len]"m"(rname->length),
        [scope]"r"(&scope),
        [parmlist]"m"(parmList)

      : "r1", "r2", "r14"
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

  ENQToken token = {0};
  char parmList[200] = {0};

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
      ",QNAME=%[qname]"
      ",RNAME=%[rname]"
      ",RNAMELEN=%[rname_len]"
      ",CONTROL=SHARED"
      ",RESERVEVOLUME=NO"
      ",SCOPE=VALUE"
      ",SCOPEVAL=(%[scope])"
      ",RNL=YES"
      ",ENQTOKEN=%[token]"
      ",COND=YES"
      ",RETCODE=GPR15"
      ",RSNCODE=GPR00"
      ",PLISTVER=2"
      ",MF=(E,%[parmlist],COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"

      : [token]"=m"(token), [rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)

      : [qname]"m"(qname->value),
        [rname]"m"(rname->value),
        [rname_len]"m"(rname->length),
        [scope]"r"(&scope),
        [parmlist]"m"(parmList)

      : "r1", "r2", "r14"
  );

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }

  return rc;
}

int isgenqReleaseLock(const ENQToken *token, int *reasonCode) {

  char parmList[200] = {0};

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "ENQREL   LARL  2,ENQREL                                                 \n"
      "         USING ENQREL,2                                                 \n"
      "         ISGENQ   REQUEST=RELEASE"
      ",RESLIST=NO"
      ",ENQTOKEN=%[token]"
      ",OWNINGTTOKEN=CURRENT_TASK"
      ",COND=YES"
      ",RETCODE=GPR15"
      ",RSNCODE=GPR00"
      ",PLISTVER=2"
      ",MF=(E,%[parmlist],COMPLETE)"
      "                                                                        \n"
      "         DROP                                                           \n"
      "         POP USING                                                      \n"

      : [rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)

      : [token]"m"(*token), [parmlist]"m"(parmList)

      : "r1", "r2", "r14"
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

