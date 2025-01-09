/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZUTM_H
#define ZUTM_H
#include "zmetal.h"

#if defined(__cplusplus) && (defined(__IBMCPP__) || defined(__IBMC__))
extern "OS"
{
#elif defined(__cplusplus)
extern "C"
{
#endif

#define RET_ARG_MAX_LEN 260
#define MSG_ENTRIES 4

int ZUTMFR64(void *PTR64);
int ZUTMGT64(void **PTR64, int *PTR64);
int ZUTMGUSR(char[8]);
int ZUTWDYN(const char *, unsigned int *, char [RET_ARG_MAX_LEN * MSG_ENTRIES + 1]);
int ZUTTEST();

#if defined(__cplusplus)
}
#endif

#endif
