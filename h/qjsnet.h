/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_QJSNET__
#define __ZOWE_QJSNET__ 1

#include "zowetypes.h"
#include "cutils.h"
#include "quickjs-libc.h"

int ejsInitNetCallback(JSContext *ctx, JSModuleDef *m);
JSModuleDef *ejsInitModuleNet(JSContext *ctx, const char *module_name);


#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
