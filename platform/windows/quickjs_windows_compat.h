/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  Windows compatibility shim for quickjs.c – MinGW / non-MSVC clang mode.

  When building with vanilla clang targeting Windows in GNU mode
  (_MSC_VER NOT defined, e.g. -target x86_64-w64-mingw32), quickjs.c's
  Windows path via winstdio.h is NOT taken because it is guarded by
  "#ifdef _MSC_VER".  The fallback branch in quickjs.c then fires:

      #elif !defined(__APPLE__) && !defined(__linux__)
          typedef int ssize_t;

  which produces a 32-bit ssize_t on a 64-bit target—incorrect for
  allocations that can exceed 2 GiB.

  Pre-including this file (via -include in the build script) ensures
  ssize_t is correctly typed as int64_t before quickjs.c's internal
  typedef is seen.  The subsequent compatible redeclaration in quickjs.c
  is suppressed by -Wno-typedef-redefinition in QJS_EXTRA_CFLAGS.

  NOTE: When building with clang in MSVC compatibility mode (the default
  when running the LLVM Windows installer build of clang), _MSC_VER is
  defined and quickjs.c automatically includes porting/winstdio.h which
  provides ssize_t.  This file is therefore a no-op in MSVC mode but is
  harmless to include in both cases.

  THIS FILE IS ONLY USED ON WINDOWS (passed via -include in the build
  script for QuickJS sources).
*/

#ifndef __QUICKJS_WINDOWS_COMPAT__
#define __QUICKJS_WINDOWS_COMPAT__ 1

#include <stdint.h>

/* Define ssize_t as a 64-bit signed integer, matching the Windows
   convention used by porting/winstdio.h.  Guard against redefinition
   in case the MSVC CRT or a MinGW header already provided it. */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef int64_t ssize_t;
#endif

/* ------------------------------------------------------------------
 * MSVC-specific shims for GCC/clang built-in functions and attributes
 * used by quickjs's cutils.h.
 *
 * Guarded by _MSC_VER && !__clang__ so they only fire for pure MSVC
 * (clang in MSVC-compat mode already handles these natively).
 * ------------------------------------------------------------------ */
#if defined(_MSC_VER) && !defined(__clang__)

#include <intrin.h>

/* __builtin_clz / __builtin_ctz ------------------------------------ */
static __forceinline int __builtin_clz(unsigned int x) {
    unsigned long r;
    _BitScanReverse(&r, (unsigned long)x);
    return 31 - (int)r;
}
static __forceinline int __builtin_ctz(unsigned int x) {
    unsigned long r;
    _BitScanForward(&r, (unsigned long)x);
    return (int)r;
}
#ifdef _WIN64
/* _BitScanReverse64/_BitScanForward64 are only available in 64-bit builds */
static __forceinline int __builtin_clzll(unsigned long long x) {
    unsigned long r;
    _BitScanReverse64(&r, x);
    return 63 - (int)r;
}
static __forceinline int __builtin_ctzll(unsigned long long x) {
    unsigned long r;
    _BitScanForward64(&r, x);
    return (int)r;
}
#else
/* 32-bit fallback: split into two 32-bit scans */
static __forceinline int __builtin_clzll(unsigned long long x) {
    unsigned long r;
    if ((unsigned long)(x >> 32)) { _BitScanReverse(&r, (unsigned long)(x >> 32)); return 31 - (int)r; }
    _BitScanReverse(&r, (unsigned long)x); return 63 - (int)r;
}
static __forceinline int __builtin_ctzll(unsigned long long x) {
    unsigned long r;
    if ((unsigned long)x) { _BitScanForward(&r, (unsigned long)x); return (int)r; }
    _BitScanForward(&r, (unsigned long)(x >> 32)); return 32 + (int)r;
}
#endif

/* __builtin_expect ------------------------------------------------- */
#define __builtin_expect(expr, val) (expr)

/* __builtin_frame_address ------------------------------------------ */
/* Used only for stack-overflow detection; an approximate current stack
   pointer is sufficient.  _AddressOfReturnAddress() is available in
   <intrin.h> (already included above) and gives a valid stack address. */
#define __builtin_frame_address(level) _AddressOfReturnAddress()

/* __attribute__ / __attribute ------------------------------------ */
/* Strip GCC/clang __attribute__ directives that MSVC does not support.
   Covers __attribute__((packed)), __attribute__((format(...))),
   __attribute__((unused)), etc.
   quickjs.c uses both __attribute__((x)) and __attribute((x)) (the latter
   is a non-standard single-underscore variant sometimes used in practice),
   so we define both spellings.
   Struct packing is benign to drop on x86/x64: these single-member
   structs have no intra-struct padding, so the packed attribute only
   affects alignment assumptions of the pointer cast, which is harmless
   on x86/x64 where unaligned loads work correctly. */
#define __attribute__(x)
#define __attribute(x)

/* CONFIG_VERSION --------------------------------------------------- */
/* CMD response files may strip the enclosing double-quotes from /D
   defines with string literal values, causing the macro to expand to a
   bare token sequence instead of a string literal.  Force a valid
   string literal here unconditionally; the build date is fixed for the
   Windows port. */
#undef  CONFIG_VERSION
#define CONFIG_VERSION "2021-03-27"

#endif /* _MSC_VER && !__clang__ */

#endif /* __QUICKJS_WINDOWS_COMPAT__ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
