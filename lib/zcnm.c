/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include <stdio.h>
#include <string.h>
#include "zwto.h"
#include "zcnm31.h"
#include "zcntype.h"
#include "zecb.h"

#pragma prolog(ZCNACT, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZCNACT(ZCN *zcn)
{
  int rc = 0;
  rc = test_auth();
  if (0 != rc)
  {
    strcpy(zcn->service_name, "TESTAUTH");
    zcn->e_msg_len = sprintf(zcn->e_msg, "Not authorized - %d", rc);
    zcn->detail_rc = ZCN_RTNCD_NOT_AUTH;
    return ZCN_RTNCD_FAILURE;
  }

  ZCN zcn31 = {0};
  memcpy(&zcn31, zcn, sizeof(ZCN));
  rc = zcnm1act(&zcn31);
  memcpy(zcn, &zcn31, sizeof(ZCN));

  if (0 != rc)
  {
    zcn->e_msg_len = sprintf(zcn->e_msg, "Error activating console, service: %s, rc: %d, service_rc: %d, service_rsn: %d", zcn->service_name, rc, zcn->service_rc, zcn->service_rsn);
    return ZCN_RTNCD_FAILURE;
  }

  return rc;
}

#pragma prolog(ZCNPUT, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZCNPUT(ZCN *zcn, const char *command)
{
  int rc = 0;

  rc = test_auth();
  if (0 != rc)
  {
    strcpy(zcn->service_name, "TESTAUTH");
    zcn->e_msg_len = sprintf(zcn->e_msg, "Not authorized - %d", rc);
    zcn->detail_rc = ZCN_RTNCD_NOT_AUTH;
    return ZCN_RTNCD_FAILURE;
  }

  ZCN zcn31 = {0};
  memcpy(&zcn31, zcn, sizeof(ZCN));
  rc = zcnm1put(&zcn31, command);
  memcpy(zcn, &zcn31, sizeof(ZCN));

  if (0 != rc)
  {
    zcn->e_msg_len = sprintf(zcn->e_msg, "Error writting data to console, service: %s, rc: %d, service_rc: %d, service_rsn: %d", zcn->service_name, rc, zcn->service_rc, zcn->service_rsn);
    return ZCN_RTNCD_FAILURE;
  }

  return rc;
}

#pragma prolog(ZCNGET, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZCNGET(ZCN *zcn, char *response)
{
  int rc = 0;

  rc = test_auth();
  if (0 != rc)
  {
    strcpy(zcn->service_name, "TESTAUTH");
    zcn->e_msg_len = sprintf(zcn->e_msg, "Not authorized - %d", rc);
    zcn->detail_rc = ZCN_RTNCD_NOT_AUTH;
    return ZCN_RTNCD_FAILURE;
  }

  if (zcn->ecb) ecb_wait((ECB *PTR32)zcn->ecb);
  ZCN zcn31 = {0};
  memcpy(&zcn31, zcn, sizeof(ZCN));
  rc = zcnm1get(&zcn31, response);
  memcpy(zcn, &zcn31, sizeof(ZCN));

  if (0 != rc)
  {
    zcn->e_msg_len = sprintf(zcn->e_msg, "Error getting data from console, service: %s, rc: %d, service_rc: %d, service_rsn: %d", zcn->service_name, rc, zcn->service_rc, zcn->service_rsn);
    return ZCN_RTNCD_FAILURE;
  }

  return rc;
}

#pragma prolog(ZCNDACT, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZCNDACT(ZCN *zcn)
{
  int rc = 0;

  rc = test_auth();
  if (0 != rc)
  {
    strcpy(zcn->service_name, "TESTAUTH");
    zcn->e_msg_len = sprintf(zcn->e_msg, "Not authorized - %d", rc);
    zcn->detail_rc = ZCN_RTNCD_NOT_AUTH;
    return ZCN_RTNCD_FAILURE;
  }

  ZCN zcn31 = {0};
  memcpy(&zcn31, zcn, sizeof(ZCN));
  rc = zcnm1dea(&zcn31);
  memcpy(zcn, &zcn31, sizeof(ZCN));

  if (0 != rc)
  {
    zcn->e_msg_len = sprintf(zcn->e_msg, "Error deactivating console, service: %s, rc: %d, service_rc: %d, service_rsn: %d", zcn->service_name, rc, zcn->service_rc, zcn->service_rsn);
    return ZCN_RTNCD_FAILURE;
  }

  return rc;
}