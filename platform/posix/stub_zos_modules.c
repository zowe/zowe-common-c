
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  Stub implementations of the z/OS-specific and network QuickJS module
  initializers for POSIX platforms (Linux, macOS) that do not have these
  capabilities.  The stubs satisfy the link references from embeddedjs.c
  while doing nothing, so configmgr can be built and run without the
  z/OS or network JS modules.
*/

#include <stdlib.h>
#include "zowetypes.h"
#include "cutils.h"
#include "quickjs-libc.h"
#include "qjszos.h"
#include "qjsnet.h"

int ejsInitZOSCallback(JSContext *ctx, JSModuleDef *m) {
  return 0;
}

JSModuleDef *ejsInitModuleZOS(JSContext *ctx, const char *module_name) {
  return NULL;
}

int ejsInitNetCallback(JSContext *ctx, JSModuleDef *m) {
  return 0;
}

JSModuleDef *ejsInitModuleNet(JSContext *ctx, const char *module_name) {
  return NULL;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
