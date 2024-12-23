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
int ZUTTEST()
{

  WTO_BUF buf = {0};
  WTOR_REPLY_BUF reply = {0};

  buf.len = sprintf(buf.msg, "reply with a number");

  ECB ecb = {0};
  wtor(&buf, &reply, &ecb);

  ecb_wait(&ecb);

  return 0;
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
