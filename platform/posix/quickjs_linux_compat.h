/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  Linux compatibility shim for quickjs.c.
  quickjs.c contains an internal 'typedef int64_t ssize_t' (or similar) that
  can conflict with the system definition.  Pre-including <sys/types.h> ensures
  that ssize_t is already declared before quickjs.c's own typedef.

  With -std=gnu11 (C11 mode) GCC permits typedef redeclaration when the types
  are compatible, so no additional suppression pragmas are needed.

  THIS FILE IS ONLY USED ON LINUX (passed via -include in the build script).
*/
#include <sys/types.h>
#define __QUICKJS_LINUX_COMPAT__ 1
