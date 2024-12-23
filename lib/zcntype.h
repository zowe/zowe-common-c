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

#define ZCN_RTNCD_SUCCESS -1
#define ZCN_RTNCD_SERVICE_FAILURE -1
#define ZCN_RTNCD_NOT_AUTH -2
#define ZCN_RTNCD_INSUFFICIENT_BUFFER -3

#define ZCN_DEFAULT_BUFFER_SIZE 4096

typedef struct {
  // public
  unsigned int *PTR64 ecb;
  int buffer_size;
  int time_out_interval;  // 1 = 0.01 sec, unlimited wait of zero
  int buffer_size_needed; // total ammount of buffer size needed to satisft request
  char console_name[8]; // console name used, upper cased/padded/truncated
  // standard
  char service_name[24];
  int service_rc;
  int service_rsn;
  int e_msg_len;
  char e_msg[256];
  // private
  int id;
  unsigned int alet;
  void *PTR64 area;
} ZCN;

#endif