/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZMETAL_H
#define ZMETAL_H

#if defined(__IBM_METAL__)

// on z
#define ATTRIBUTE(...) __attribute__((__VA_ARGS__)) // e.g. ATTRIBUTE(amode31, armode)
#define PTR32 __ptr32
#define PTR64 __ptr64
#define ASMREG(register) __asm(register)

#else

// off z
#define ATTRIBUTE(...)
#define PTR32
#define PTR64
#define ASMREG(register)

#endif

#define MAX_PARM_LENGTH 100 + 1

typedef struct
{
  short int length;
  char parms[MAX_PARM_LENGTH];
} IN_DATA;

#define HI_BIT_MASK 0x7FFFFFFF
typedef struct
{
  union
  {
    IN_DATA *PTR32 addr;
    int addrValue;
  } data;
} IN_PARM;

#if defined(__IBM_METAL__)
#define S0C3(n)                                               \
  __asm(                                                      \
      "*                                                  \n" \
      " LLGF  0,%0      = Value passed by caller          \n" \
      " EXRL  0,*       Execute                           \n" \
      " DC    C'@S0C3'  Find by '@S0C3'                     " \
      "*                                                    " \
      :                                                       \
      : "m"(n)                                                \
      : "r0");
#else
#define S0C3(n)
#endif

static void s0c3Abend(int n)
{
  S0C3(n);
}

#if defined(__IBM_METAL__)
#define TESTAUTH(rc)                                          \
  __asm(                                                      \
      "*                                                  \n" \
      " TESTAUTH FCTN=1,"                                     \
      "RBLEVEL=1,"                                            \
      "STATE=YES,"                                            \
      "KEY=YES                                            \n" \
      "*                                                  \n" \
      " ST 15,%0               Save RC                    \n" \
      "*                                                    " \
      : "=m"(rc)                                              \
      :: "r0","r1","r14","r15");
#else
#define TESTAUTH(rc)
#endif

static int testAuth()
{
  int rc = 0;
  TESTAUTH(rc);
  return rc;
}

#if defined(__IBM_METAL__)
#define MODESET(value)                                         \
  __asm(                                                       \
      "*                                                   \n" \
      " MODESET MODE="#value##"                            \n" \
      "*                                                    "  \
      :::);
#else
#define MODESET(n)
#endif

static void modesup()
{
  MODESET(SUP);
}

static void modeprob()
{
  MODESET(PROB);
}

// int reg = 0;
// GET_REG(13, &reg);
#if defined(__IBM_METAL__)
#define GET_REG(num, reg)                                       \
  __asm(                                                        \
      "*                                                   \n"  \
      " ST    " #num ",%0 = Value passed by caller         \n"  \
      "*                                                    "   \
      : "=m"(*reg)                                              \
      :                                                         \
      :);
#else
#define GET_REG(num, reg)
#endif

#if defined(__IBM_METAL__)
#define SET_REG(num, reg)                                       \
  __asm(                                                        \
      "*                                                    \n" \
      " L    %0," #num "  = Value passed by caller          \n" \
      "*                                                    "   \
      : "=m"(*reg)                                              \
      :                                                         \
      : "#num");
#else
#define SET_REG(num, reg)
#endif

#endif