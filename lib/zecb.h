/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZECBWAIT_H
#define ZECBWAIT_H

#include "ihaecb.h"
#include "zmetal.h"

typedef struct ecb ECB;

#if defined(__IBM_METAL__)
#define STIMER_WAIT(time)                                     \
  __asm(                                                      \
      "*                                                  \n" \
      " TAM   ,         AMODE64??                         \n" \
      " JM    *+4+4+2   No, skip switching                \n" \
      " OILH  2,X'8000' Set AMODE31 flag                  \n" \
      " SAM31 ,         Set AMODE31                       \n" \
      "*                                                  \n" \
      " SYSSTATE PUSH       Save SYSSTATE                 \n" \
      " SYSSTATE AMODE64=NO                               \n" \
      "*                                                  \n" \
      " STIMER WAIT,BINTVL=(%0)                           \n" \
      "*                                                  \n" \
      " TMLH  2,X'8000' Did we switch AMODE??             \n" \
      " JNO   *+4+2     No, skip restore                  \n" \
      " SAM64 ,         Set AMODE64                       \n" \
      "*                                                  \n" \
      " NILH  2,X'7FFF' Clear flag if set                 \n" \
      "*                                                  \n" \
      " SYSSTATE POP    Restore SYSSTATE                  \n" \
      "*                                                    " \
      :                                                       \
      : "r"(time)                                             \
      : "r0", "r1", "r2", "r14", "r15");
#else
#define STIMER_WAIT(time)
#endif // __IBM_METAL__

#if defined(__IBM_METAL__)
#define ECB_WAIT(ecb)                                         \
  __asm(                                                      \
      "*                                                  \n" \
      " WAIT ECB=%0                                       \n" \
      "*                                                    " \
      : "+m"(*ecb)                                            \
      :                                                       \
      : "r0", "r1", "r14", "r15");
#else
#define ECB_WAIT(ecb)
#endif // __IBM_METAL__

#if defined(__IBM_METAL__)
#define ECBS_WAIT(count, list)                                \
  __asm(                                                      \
      "*                                                  \n" \
      " WAIT (%1),ECBLIST=%0                              \n" \
      "*                                                    " \
      : "+m"(*list)                                           \
      : "r"(count)                                            \
      : "r0", "r1", "r14", "r15");
#else
#define ECBS_WAIT(count, list)
#endif // __IBM_METAL__

static void ecbWait(ECB *PTR32 ecb)
{
  ECB_WAIT(ecb);
}

static void ecbsWait(
    int events, volatile ECB *PTR32 ecbList[],
    int ecbListCount)
{

  union overEcb {
    volatile ECB *__ptr32 ecb;
    unsigned int word;
  } oEcb = {0};

  if (ecbListCount >= 1)
  {

    oEcb.ecb = ecbList[ecbListCount - 1];
    oEcb.word |= 0x80000000;

    ecbList[ecbListCount - 1] = oEcb.ecb;

    ECBS_WAIT(events, ecbList);

    oEcb.word &= ~(0x80000000);
    ecbList[ecbListCount - 1] = oEcb.ecb;

  }

  return;
}

static void ecbsWaitOnOne(
    volatile ECB *__ptr32 ecbList[],
    int ecbListCount)
{

  ecbsWait(1, ecbList, ecbListCount);

  return;
}


// 1 = 0.01 seconds
static void timeWait(int interval)
{
  STIMER_WAIT(&interval);

  return;
}

#endif //ECBWAIT_H