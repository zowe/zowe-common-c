/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include <stdio.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>
#include "zds.hpp"
#include "zcnm.h"
#include "zcn.hpp"

using namespace std;

int zcn_activate(ZCN *zcn, string name)
{
  int rc = 0;

  char charName[8] = {0};
  strncpy(charName, name.c_str(), sizeof(charName));

  zcn->ecb = (unsigned int *)__malloc31(sizeof(unsigned int));

  rc = ZCNACT(zcn, charName);

  if (0 != rc) free(zcn->ecb);

  return rc;
}

int zcn_put(ZCN *zcn, string command)
{
  int rc = 0;

  char *command31 = (char *)__malloc31(command.length());
  memcpy(command31, command.c_str(), command.length());
  rc = ZCNPUT(zcn, command31);
  free(command31);

  return rc;
}

int zcn_get(ZCN *zcn, string &response)
{
  int rc = 0;

  // user caller buffer size if provided
  if (0 == zcn->buffer_size) zcn->buffer_size = ZCN_DEFAULT_BUFFER_SIZE;

  char *resp31 = (char *)__malloc31(zcn->buffer_size);
  memset(resp31, 0x00, zcn->buffer_size);

  rc = ZCNGET(zcn, resp31);

  if (0 == rc) response += string(resp31);
  free(resp31);

  return 0;
}

int zcn_deactivate(ZCN *zcn)
{
  if (zcn->ecb) free(zcn->ecb);

  return ZCNDACT(zcn);
}