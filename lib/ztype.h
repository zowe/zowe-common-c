/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZTYPE_H
#define ZTYPE_H

#define RTNCD_SUCCESS 0
#define RTNCD_FAILURE -1

#if defined(__IBM_METAL__)

#define ATTRIBUTE(...) __attribute__((__VA_ARGS__)) // ATTRIBUTE(amode31)
#define PTR32 __ptr32
#define PTR64 __ptr64
#define FAR __far
#define ASMREG(register) __asm(register)

#else

#define ATTRIBUTE(...)
#define PTR32
#define PTR64
#define FAR
#define ASMREG(register)

#endif

#endif