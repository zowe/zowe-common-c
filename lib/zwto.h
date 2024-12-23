/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZWTO_H
#define ZWTO_H

#include <stdio.h>
#include <string.h>

#include "ihaecb.h"
#include "zmetal.h"
#include "zecb.h"

#if defined(__IBM_METAL__)
#define WTO_MODEL(wtom)                                         \
    __asm(                                                      \
        "*                                                  \n" \
        " WTO TEXT=,"                                           \
        "ROUTCDE=(11),"                                         \
        "DESC=(6),"                                             \
        "MF=L                                               \n" \
        "*                                                    " \
        : "DS"(wtom));
#else
#define WTO_MODEL(wtom) void *wtom;
#endif

WTO_MODEL(wtoModel); // make this copy in static storage

#if defined(__IBM_METAL__)
#define WTOR_MODEL(wtorm)                                       \
    __asm(                                                      \
        "*                                                  \n" \
        " WTOR TEXT=(,,,),"                                     \
        "ROUTCDE=(11),"                                         \
        "DESC=(6),"                                             \
        "MF=L                                               \n" \
        "*                                                    " \
        : "DS"(wtorm));
#else
#define WTOR_MODEL(wtorm) void *wtorm;
#endif

WTOR_MODEL(wtorModel); // make this copy in static storage

#define MAX_WTO_TEXT 126

#if defined(__IBM_METAL__)
#define WTO(buf, plist, rc)                                     \
    __asm(                                                      \
        "*                                                  \n" \
        " SLGR  0,0       Save RC                           \n" \
        "*                                                  \n" \
        " WTO TEXT=(%2),"                                       \
        "MF=(E,%0)                                          \n" \
        "*                                                  \n" \
        " ST    15,%1     Save RC                           \n" \
        "*                                                    " \
        : "+m"(plist),                                          \
          "=m"(rc)                                              \
        : "r"(buf)                                              \
        : "r0", "r1", "r14", "r15");
#else
#define WTO(buf, plist, rc)
#endif

#if defined(__IBM_METAL__)
#define WTOR(buf, reply, replylen, ecb, rc, plist)              \
    __asm(                                                      \
        "*                                                  \n" \
        " SLGR  0,0       Save RC                           \n" \
        " LLGF  2,%5      Get len                           \n" \
        "*                                                  \n" \
        " WTOR TEXT=((%4),%2,(2),%1),"                          \
        "MF=(E,%0)                                          \n" \
        "*                                                  \n" \
        " ST    15,%3     Save RC                           \n" \
        "*                                                    " \
        : "+m"(plist),                                          \
          "+m"(ecb),                                            \
          "=m"(reply),                                          \
          "=m"(rc)                                              \
        : "r"(buf),                                             \
          "m"(replylen)                                         \
        : "r0", "r1", "r2", "r14", "r15");
#else
#define WTOR(buf, reply, replylen, ecb, rc, plist)
#endif

#define MAX_WTOR_TEXT 122
#define MAX_WTOR_REPLY_TEXT 119

typedef struct
{
    short int len;
    char msg[MAX_WTO_TEXT];
} WTO_BUF;

static int wto(WTO_BUF *buf)
{
    int rc = 0;
    WTO_MODEL(dsaWtoModel); // stack var
    dsaWtoModel = wtoModel; // copy model
    WTO(buf, dsaWtoModel, rc);
    return rc;
}

typedef struct
{
    char msg[MAX_WTOR_TEXT + 1];
} WTOR_REPLY_BUF;

static int wtor(WTO_BUF *buf, WTOR_REPLY_BUF *reply, ECB *ecb)
{
    int rc = 0;
    int replyLen = sizeof(WTOR_REPLY_BUF);
    memset(reply, 0x00, sizeof(WTOR_REPLY_BUF));
    WTOR_MODEL(dsaWtorModel); // stack var
    dsaWtorModel = wtorModel; // copy model
    WTOR(buf, *reply, replyLen, *ecb, rc, dsaWtorModel);
    return rc;
}

#endif