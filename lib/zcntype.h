/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZCNTYPE_H
#define ZCNTYPE_H

#include "zmetal.h"
#include <stdint.h>

#define ZCN_RTNCD_SUCCESS 0
#define ZCN_RTNCD_FAILURE -1
#define ZCN_RTNCD_SERVICE_FAILURE -2
#define ZCN_RTNCD_NOT_AUTH -3
#define ZCN_RTNCD_INSUFFICIENT_BUFFER -4

#define ZCN_DEFAULT_BUFFER_SIZE 4096

#if (defined(__IBMCPP__) || defined(__IBMC__))
#pragma pack(packed)
#endif

#define EYE_BEG "ZCNB"
#define EYE_END "ZCNE"

// NOTE(Kelosky): struct is padded to nearest double word boundary; ensure proper alignment for fields
typedef struct {
  char eye_beg[4];

  unsigned int *PTR64 ecb; // save and set to NULL to prevent waiting
  int32_t buffer_size;
  int32_t buffer_size_needed; // total ammount of buffer size needed to satisfy request
  char console_name[8]; // console name used, upper cased/padded/truncated
  int16_t unused; // non-zero if reply found in control
  int16_t reply_id_len; // non-zero if reply found in control
  char reply_id[8]; // if reply_id_len is non-zero

  char service_name[24];
  int32_t detail_rc;
  int32_t service_rc;
  int32_t service_rsn;
  int32_t e_msg_len;
  char e_msg[256];

  int32_t id;
  uint32_t alet;
  void *PTR64 area;
  char eye_end[4];
} ZCN;

#if (defined(__IBMCPP__) || defined(__IBMC__))
#pragma pack(reset)
#endif

#endif