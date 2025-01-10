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

#if defined(__IBM_METAL__)
#define SET_R1_PARM(parm) __asm(" LA 1,%0\n":"+m"(parm)::"r1");
#else
#define SET_R1_PARM(parm)
#endif

typedef struct {
  short len;
  char str[RET_ARG_MAX_LEN];
} BPXWDYN_RET_ARG;

typedef int (*BPXWDYN)() ATTRIBUTE(amode31);

#pragma prolog(ZUTWDYN, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZUTWDYN(BPXWDYN_PARM *parm, BPXWDYN_RESPONSE *response)
// int ZUTWDYN(const char *parm, unsigned int *code, char response[RET_ARG_MAX_LEN * MSG_ENTRIES + 1])
{
  int rc = 0;
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

  zwto_debug("string len and value was %d '%.*s'",  parm->len, parm->len, parm->parm);

  char *freeparm = parm->parm;
  // char *freeparm = "free dd(nononono)";
  // char *freeparm = "alloc da(dkelosky.temp.test5) space(5,5) dsorg(po) dir(5) cyl lrecl(80) recfm(f,b)";
  // freeparm = "alloc da(dkelosky.temp.test5) space(5,5) dsorg(po) dir(5) cyl lrecl(80) recfm(f,b) new";

  void *PTR32 parms[MSG_ENTRIES + 1 + 1] = {0};
  parms[0] = (void *PTR32)parm->parm; //freeparm;
  parms[1] = &msg;
  for (int i = 2; i <= MSG_ENTRIES + 1 + 1; i++)
  {
    parms[i] = &msg_response[i - 1 - 1];
  }

  // Doc:
  // * keywords - https://www.ibm.com/docs/en/zos/3.1.0?topic=output-requesting-dynamic-allocation
  // * return codes - https://www.ibm.com/docs/en/zos/3.1.0?topic=output-bpxwdyn-return-codes
  // * detail codes (high 4 hex bytes) - https://www.ibm.com/docs/en/zos/3.1.0?topic=codes-interpreting-error-reason-from-dynalloc#erc__mjfig8
  // * parm list - https://www.ibm.com/docs/en/zos/3.1.0?topic=conventions-conventional-mvs-parameter-list

  for (int i = 0; i < MSG_ENTRIES; i++)
  {
    msg_response[i].len = RET_ARG_MAX_LEN - sizeof(msg_response[i].len);
    sprintf(msg_response[i].str, "MSG.%d", i +1);
  }

  parms[MSG_ENTRIES + 1] = (void *PTR32)((unsigned int)parms[MSG_ENTRIES + 1] | 0x80000000);
  SET_R1_PARM(parms[0]);
  rc = dynalloc();
  // parms[MSG_ENTRIES + 1] = (void *PTR32)((unsigned int)parms[MSG_ENTRIES + 1] | 0x7FFFFFFF);

  response->code = rc;

  char *respp = response->response;
  for (int i = 0, j=atoi(msg.str); i<j && i <MSG_ENTRIES; i++){
    if (msg_response[i].len == RET_ARG_MAX_LEN - sizeof(msg_response[i].len))
    {
      return (0 != rc) ? ZUT_BPXWDYN_SERVICE_FAILURE : ZUT_RTNCD_SUCCESS;
    }
    int len = sprintf(respp, "%.*s\n", msg_response[i].len, msg_response[i].str);
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
