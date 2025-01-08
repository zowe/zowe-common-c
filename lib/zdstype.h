/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZDSTYPE_H
#define ZDSTYPE_H

#include <stdint.h>
#include "ztype.h"

#define ZDS_RTNCD_SUCCESS 0
#define ZDS_RTNCD_FAILURE -1
#define ZDS_RTNCD_SERVICE_FAILURE -2
#define ZDS_RTNCD_MAX_JOBS_REACHED -3
#define ZDS_RTNCD_INSUFFICIENT_BUFFER -4
#define ZDS_RTNCD_JOB_NOT_FOUND -5

#define ZDS_DEFAULT_BUFFER_SIZE 128000
#define ZDS_DEFAULT_MAX_JOBS 100
#define ZDS_DEFAULT_MAX_DDS 100

#if (defined(__IBMCPP__) || defined(__IBMC__))
#pragma pack(packed)
#endif

// NOTE(Kelosky): struct is padded to nearest double word boundary; ensure proper alignment for fields
typedef struct {
  char eye_beg[4];

  char service_name[24];
  int32_t detail_rc;
  int32_t service_rc;
  int32_t service_rsn;
  int32_t e_msg_len;
  char e_msg[256];

  char eye_end[4];
} ZDS;

#if (defined(__IBMCPP__) || defined(__IBMC__))
#pragma pack(reset)
#endif

#endif