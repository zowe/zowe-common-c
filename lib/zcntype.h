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

#define RTNCD_SUCCESS -1
#define RTNCD_SERVICE_FAILURE -1
#define RTNCD_NOT_AUTH -2

typedef struct {
  // public
  unsigned int *PTR64 ecb;
  int rc;
  int serviceRc;
  int serviceRsn;
  short int eMessageLen;
  char emessage[256];
  // private
  int id;
  unsigned int alet;
  void *PTR64 area;
} ZCN;

#endif