/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zmetal.h"
#include "zstorage.h"
#include "zwto.h"
#include "zutm.h"
#include "zutm31.h"
#include "zecb.h"

#define ZUT_RTNCD_SUCCESS 0
#define ZUT_LOAD_FAILURE -1
#define ZUT_BPXWDYN_SERVICE_FAILURE -2

typedef struct {
  short len;
  char str[RET_ARG_MAX_LEN];
} BPXWDYN_RET_ARG;

typedef int (*BPXWDYN)(const char *PTR32, BPXWDYN_RET_ARG *PTR32, BPXWDYN_RET_ARG *PTR32, BPXWDYN_RET_ARG *PTR32, BPXWDYN_RET_ARG *PTR32, BPXWDYN_RET_ARG *PTR32) ATTRIBUTE(amode31); // NOTE(Kelosky): this is not dynamic based on MSG_ENTRIES

#pragma prolog(ZUTWDYN, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZUTWDYN(const char *parm, unsigned int *code, char response[RET_ARG_MAX_LEN * MSG_ENTRIES + 1])
{
  void *function = load_module("BPXWDY2"); // EP which doesn't require R0 == 0
  if (!function)
  {
    return ZUT_LOAD_FAILURE;
  }

  long long unsigned int ifunction = (long long unsigned int)function;
  ifunction &= 0x000000007FFFFFFF; // clear high bit
  BPXWDYN dynalloc = (BPXWDYN)ifunction;

  BPXWDYN_RET_ARG msg = {RET_ARG_MAX_LEN - sizeof(msg.len), "MSG"};
  BPXWDYN_RET_ARG msg_response[MSG_ENTRIES] = {0};

  // Doc:
  // * keywords - https://www.ibm.com/docs/en/zos/3.1.0?topic=output-requesting-dynamic-allocation
  // * return codes - https://www.ibm.com/docs/en/zos/3.1.0?topic=output-bpxwdyn-return-codes
  // * detail codes (high 4 hex bytes) - https://www.ibm.com/docs/en/zos/2.4.0?topic=codes-interpreting-error-reason-from-dynalloc#erc__mjfig8
  // * parm list - https://www.ibm.com/docs/en/zos/3.1.0?topic=conventions-conventional-mvs-parameter-list

  for (int i = 0; i < MSG_ENTRIES; i++)
  {
    msg_response[i].len = RET_ARG_MAX_LEN - sizeof(msg_response[i].len);
    sprintf(msg_response[i].str, "MSG.%d", i +1);
  }

  BPXWDYN_RET_ARG *PTR32 last = &msg_response[MSG_ENTRIES - 1];
  last = (BPXWDYN_RET_ARG *PTR32)((unsigned int)last | 0x80000000);

  int rc = dynalloc(parm, &msg, &msg_response[0], &msg_response[1], &msg_response[2], last); // NOTE(Kelosky): this is not dynamic based on MSG_ENTRIES

  *code = rc;

  char *respp = response;
  for (int i = 0, j=atoi(msg.str); i<j && i <MSG_ENTRIES; i++){
    if (msg_response[i].len == RET_ARG_MAX_LEN - sizeof(msg_response[i].len))
    {
      return ZUT_BPXWDYN_SERVICE_FAILURE;
    }
    int len = sprintf(respp, "%s\n", msg_response[i].str);
    respp = respp + len;
  }

  return (0 != rc) ? ZUT_BPXWDYN_SERVICE_FAILURE : ZUT_RTNCD_SUCCESS;
}

#pragma prolog(ZUTTEST, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZUTTEST()
{
  return 0;
}

#pragma prolog(ZUTMGUSR, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZUTMGUSR(char user[8])
{
  char user31[8] = {0};
  int rc = zutm1gur(user31);

  if (0 != rc) return rc;

  memcpy(user, user31, sizeof(user31));
  return 0;
}

#pragma prolog(ZUTMFR64, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZUTMFR64(void *PTR64 data)
{
  storageFree64(data);
  return 0;
}

#pragma prolog(ZUTMGT64, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZUTMGT64(void **PTR64 data, int *len)
{
  *data = storageGet64(*len);
  return 0;
}
