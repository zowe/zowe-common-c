/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include "zdstype.h"
#include "zmetal.h"
#include "zdsm.h"

// purge a job
#pragma prolog(ZDSATTRS, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZDSATTRS(ZDS *zds, const char *dsn)
{
  int rc = 0;

  // load our service
  void *function = load_module("BPXWDY2"); // EP which doesn't require R0 == 0
  if (!function)
  {
    return RTNCD_FAILURE;
  }

  return 0;
}