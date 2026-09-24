/*
 * Local z/OS cross-compile shim for upstream clang (-target s390x-ibm-zos).
 *
 * Required because upstream LLVM does not expose every IBM-downstream driver
 * flag (-fzos-le-char-mode, -mzos-float-kind, -mzos-asmlib) even though the
 * underlying codegen is present. This header papers over a small number of
 * feature-macro and builtin-name differences so the zowe sources compile
 * cleanly under the cross toolchain. Used ONLY by build_cmgr_clang.sh when
 * mode=zos-cross; ibm-clang64 on z/OS does not need this file.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v2.0 which accompanies this distribution,
 * and is available at https://www.eclipse.org/legal/epl-v20.html
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

#ifndef ZOWE_ZOS_CROSS_COMPAT_H
#define ZOWE_ZOS_CROSS_COMPAT_H

/* Feature macros zowe-common-c expects; set early so the z/OS headers see them. */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#ifndef _OPEN_SYS
#define _OPEN_SYS 1
#endif
#ifndef _OPEN_THREADS
#define _OPEN_THREADS 1
#endif
#ifndef _OPEN_SYS_SOCK_IPV6
#define _OPEN_SYS_SOCK_IPV6 1
#endif
#ifndef _OPEN_SYS_FILE_EXT
#define _OPEN_SYS_FILE_EXT 1
#endif

/* Open XL's -qlanglvl=extended predefined _EXT for users of system headers
 * that require it; -std=gnu99 does not. The Open XL 2.2 migration guide
 * explicitly calls this out ("-std does not define the _EXT macro"). */
#ifndef _EXT
#define _EXT 1
#endif

/* ibm-clang64 exposes z/OS-specific varargs builtins that upstream LLVM lacks.
 * Map them to the standard builtins so variadic code compiles on the cross
 * toolchain. The generated code is semantically equivalent on s390x-ibm-zos. */
#define __builtin_zos_va_list  __builtin_va_list
#define __builtin_zos_va_start __builtin_va_start
#define __builtin_zos_va_end   __builtin_va_end
#define __builtin_zos_va_copy  __builtin_va_copy

#endif /* ZOWE_ZOS_CROSS_COMPAT_H */
