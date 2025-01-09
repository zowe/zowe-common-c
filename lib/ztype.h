/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZTYPE_H
#define ZTYPE_H

#include <stdint.h>

#define RTNCD_SUCCESS 0
#define RTNCD_FAILURE -1

#if defined(__IBM_METAL__)

#define ATTRIBUTE(...) __attribute__((__VA_ARGS__)) // ATTRIBUTE(amode31)
#define PTR32 __ptr32
#define PTR64 __ptr64
#define FAR __far
#define ASMREG(register) __asm(register)

#else

#define ATTRIBUTE(...)
#define PTR32
#define PTR64
#define FAR
#define ASMREG(register)

#endif

#if (defined(__IBMCPP__) || defined(__IBMC__))
#pragma pack(packed)
#endif

// NOTE(Kelosky): struct is padded to nearest double word boundary; ensure proper alignment for fields
typedef struct {
  char service_name[24];

  int32_t detail_rc;
  int32_t service_rc;

  int32_t service_rsn;
  int32_t service_rsn_secondary;

  unsigned char reserve_0[4];
  int32_t e_msg_len;

  char e_msg[256];

  void *PTR64 data;

  unsigned int data_len;
  unsigned char reserve_1[4];

} ZDIAG;

#if (defined(__IBMCPP__) || defined(__IBMC__))
#pragma pack(reset)
#endif

#endif
