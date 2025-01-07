/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZJBM_H
#define ZJBM_H

#include "zmetal.h"
#include "zssitype.h"
#include "zjbtype.h"

#if defined(__cplusplus) && (defined(__IBMCPP__) || defined(__IBMC__))
extern "OS"
{
#elif defined(__cplusplus)
extern "C"
{
#endif

int ZJBMFREE(void *PTR64);
int ZJBMLIST(ZJB *PTR64, STATJQTR **PTR64, int *PTR64);
int ZJBMLSDS(const char *PTR64, STATSEVB **PTR64, int *PTR64, unsigned char [8]);
int ZJBSYMB(const char *PTR64, char *PTR64);
int ZJBMPRG(const char *PTR64);

#if defined(__cplusplus)
}
#endif

#endif