/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  Shared POSIX compatibility shim for quickjs.c (Linux and macOS).

  quickjs.c contains an internal ssize_t typedef that can conflict with the
  system definition on 64-bit targets.  Pre-including <sys/types.h> ensures
  that ssize_t is already declared before quickjs.c's own typedef, allowing
  the compiler to treat the subsequent redeclaration as compatible.

  On Linux with GCC in gnu11 mode the compatible redeclaration is silently
  accepted.  On macOS with clang the build script additionally passes
  -Wno-typedef-redefinition to suppress the diagnostic.

  This file is included via -include in the build script for QuickJS sources
  on both macOS and Linux.
*/

#ifndef __QUICKJS_POSIX_COMPAT__
#define __QUICKJS_POSIX_COMPAT__ 1

#include <sys/types.h>

#endif /* __QUICKJS_POSIX_COMPAT__ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
