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

#pragma prolog(ZUTTEST, "&CCN_MAIN SETB 1 \n MYPROLOG")

typedef int (*func)(char *PTR32) ATTRIBUTE(amode31);
int ZUTTEST()
{
  // void *pointer = load_module("IEFBR14"); // amode 24
  // void *pointer = load_module("CCNEDSCT"); // amode 31
  // void *pointer = load_module("CAVDSRV"); // amode 64
  // void *pointer = load_module("BPXWDY2"); //
  void *pointer = load_module("BPXWDYN"); //

  WTO_BUF buf = {0};
  buf.len = sprintf(buf.msg, "@test %llx", pointer);
  wto(&buf);

  if (pointer)
  {
    int rc = 0;
    if ((long long int)pointer & 0x0000000080000000) // amode 31
    {
      long long unsigned int pint = (long long unsigned int)pointer;
      pint &= 0x000000007FFFFFFF;
      func bpxwdy2 = (func)pint;
        WTO_BUF buf = {0};
      buf.len = sprintf(buf.msg, "@test %llx", bpxwdy2);
      wto(&buf);
      rc = 2;

      // keywords
      // https://www.ibm.com/docs/en/zos/2.4.0?topic=output-requesting-dynamic-allocation
      // return codes s
      // https://www.ibm.com/docs/en/zos/2.4.0?topic=output-bpxwdyn-return-codes
      // detail codes (high 4 hex bytes):
      // https://www.ibm.com/docs/en/zos/2.4.0?topic=codes-interpreting-error-reason-from-dynalloc#erc__mjfig8
      char *PTR32 str = "free dd(none)";
      str = (char *PTR32)((unsigned int)str | 0x80000000);
      return bpxwdy2(str);
      // return bpxwdy2("FREE DD(NONE)");
    }
    else if ((long long int)pointer & 0x0000000000000001) // amode 64
    {

      // func bpx
      rc = 3;
    }
    else
    {
      rc = 1; // amode 24
    }
    delete_module("IEFBR14");
    return rc;
  }
  else
  return 5;
  // int rc = 0;
  // int time = 1 * 100 * 1; // 3 seconds
  // // rc = cancel_timers();
  // char *data = "hello world";

  // timer(time, TIMEEXIT, data); // set a timer without waiting, that will fire in 3 seconds
  // // rc = cancel_timers();

  // int time2 = 1 * 100 * 2;
  // time_wait(time2); // set a timer and WAIT for 5 seconds;

  // return rc;
}

#pragma prolog(ZUTMGUSR, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZUTMGUSR(char user[8])
{
  char user31[8] = {0};
  int rc = zutm1gur(user31);

  if (0 != rc)
  {
    return rc;
  }

  memcpy(user, user31, sizeof(user31));
  return rc;
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
