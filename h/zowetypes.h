

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


#ifndef __ZOWETYPES__
#define __ZOWETYPES__  1

#ifndef __IBM_METAL__
#include <stdint.h>
#include <stdbool.h>
#else
#include <metal/metal.h>
#include <metal/stdint.h>
#endif

#ifndef __cplusplus
#ifndef true
typedef int bool;
#define true 1
#define false 0
#endif
#endif /* __cplusplus */

#define TRUE  1
#define FALSE 0 

#ifndef ANSI_TRUE
#define ANSI_TRUE   TRUE
#define ANSI_FALSE  FALSE
#define ANSI_OK     ANSI_TRUE
#define ANSI_FAILED ANSI_FALSE
#endif

/*
  Notes about which symbols are defined on which compilers and operating systems with which compiler options

  __IBM_METAL__   -- this is introduced by IBM xlc on zOS with the -qMetal options specified 
  _LP64           -- this is on for xlc metal/not metal on zOS, and maybe other xlc platforms
  METTLE          -- this is our tradition to have a symbol introduced in the compilation for IBM_METAL
  _WIN32          -- this is used by MSFT c, does gnu (gcc) also do this
  _WIN64          -- this is used by MSFT c, does gnu (gcc) also do this

  
__HHW_AS400__
    Indicates that the host hardware is an IBM� i processor.
__HOS_OS400__
    Indicates that the host OS is ibm I
__ILEC400__
    C compiler only Indicates that the ILE C compiler is being used.
__OS400__
    This macro is always defined when the compiler is used with the IBM i operating system.



  Dimensions of Variation

  Compiler
  CPU Architecture
  Operating System

__ZOWE_OS_ISERIES
__ZOWE_OS_ZOS
__ZOWE_OS_WINDOWS
__ZOWE_OS_LINUX
__ZOWE_OS_AIX
__ZOWE_OS_ZVM

__ZOWE_ARCH_X86_64
__ZOWE_ARCH_Z
__ZOWE_ARCH_POWER
__ZOWE_ARCH_I
__ZOWE_ARCH_IA64  - itanium

__ZOWE_COMP_GCC
__ZOWE_COMP_VCPP
__ZOWE_COMP_XLC
__ZOWE_COMP_IXLC
__ZOWE_COMP_CLANG - new in 2022!

__ZOWE_64BIT

Internal character set: only one of these will be set

__ZOWE_EBCDIC  - compiler puts character strings into the executable as EBCDIC
__ZOWE_ASCII   - compiler puts character strings into the executable as ASCII

70-90K - 

300

The conventions 

#ifdef METTLE still exists (means ZOS and XLC compiler with METAL option)

#elif defined(__ZOWE_OS_WINDOWS)
  can be GCC (32 and 64 bit)

#elif defined(__ZOWE_OS_LINUX){
  can be Z_ARCH or X_ARCH or P_ARCH.  Since we have not written any 32 bit linux code, let's not ever go there, m'kay
}

#elif defined(__ZOWE_OS_AIX){
  can be P_ARCH.  
}

#elif defined(__ZOWE_OS_ISERIES)

no ifdef means XLC LE on ZOS, and everything else.  This is effectively our "default" platform.  Why? Tradition, minimal re-writing of the code



 */


/* Base macros for Windows */
#ifdef _MSC_VER

#define ZOWE_PRAGMA_PACK  
#define ZOWE_PRAGMA_PACK_RESET


#define __ZOWE_OS_WINDOWS 1

#ifdef _WIN64
#define __ZOWE_64
#else 
#define __ZOWE_32
#endif

#endif  /* _MSC_VER */



/* Base Macros for zOS */
#if defined(__MVS__) && (defined (__IBMC__) || defined (__IBMCPP__)) && (defined (_LP64) || defined (_ILP32))

#define __ZOWE_OS_ZOS
#define __ZOWE_ARCH_Z
#define __ZOWE_COMP_XLC
#ifdef __clang__
#define __ZOWE_COMP_XLCLANG /* new in 2022, xlclang defines *BOTH* __IBMC__ *AND* __clang__ */
#endif

#ifdef _LP64
#define __ZOWE_64
#endif

/* Structure packing */
#define ZOWE_PRAGMA_PACK  _Pragma ( "pack(packed)" )
#define ZOWE_PRAGMA_PACK_RESET  _Pragma ( "pack(reset)" )

#endif /* __MVS__ __IBMC__ */


/* new clang on ZOS cases */
#if defined(__MVS__) && defined(__clang__) && (defined (_LP64) || defined (_ILP32))

#define __ZOWE_OS_ZOS
#define __ZOWE_ARCH_Z
#define __ZOWE_COMP_CLANG

#ifdef _LP64
#define __ZOWE_64
#else 
#define __ZOWE_32
#endif

/* Structure packing */
#define ZOWE_PRAGMA_PACK  _Pragma ( "pack(packed)" )
#define ZOWE_PRAGMA_PACK_RESET  _Pragma ( "pack(reset)" )

#endif /* __MVS__ __clang__ */

#if defined(_AIX) && (defined (__IBMC__) || defined (__IBMCPP__))
 #define __ZOWE_OS_AIX 1
 #define __ZOWE_COMP_XLC 1
 #define ZOWE_PRAGMA_PACK  _Pragma ( "pack(1)" )
 #define ZOWE_PRAGMA_PACK_RESET  _Pragma ( "pack(pop)" )
 #if defined(__powerpc__)
  #define __ZOWE_ARCH_POWER 1
 #endif
 #ifdef _LP64
 #define __ZOWE_64
 #else 
 #define __ZOWE_32
 #endif
#endif /* AIX */

/* Base Macros for GCC, which is usually Linux or Unix, but Z,P and X architectures can use GCC so analysis is more complex */
#if defined(__GNUC__) && !defined(__clang__)

#ifdef _LP64
#define __ZOWE_64
#else 
#define __ZOWE_32
#endif

/* Structure packing */
#define ZOWE_PRAGMA_PACK  _Pragma ( "pack(push,1)" )
#define ZOWE_PRAGMA_PACK_RESET  _Pragma ( "pack(pop)" )


#if defined(__x86_64__) || defined(__amd64__)
/* proven to be GCC for x86-64, but are we linux? - anyway assume linux for now. */

#define __ZOWE_OS_LINUX

#elif defined(__powerpc__)

#define __ZOWE_ARCH_POWER 1

#ifdef _AIX
#define __ZOWE_OS_AIX
#else  /* could this be i-series, too?? */
#define __ZOWE_OS_LINUX
#endif

#else /* of architecture if-then-else */

#define __ZOWE_OS_LINUX    /* Z Linux */

#endif /* end of architecture if-then-else */

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

/* either X86 or new Power Series Little-endian mode */

#else

/* either Z-arch or Power Series traditional big-endian mode */

#endif

#endif /* end of GCC-specific analysis */

/* Long external names are OK for all platforms except z/OS */
#ifndef __ZOWE_OS_ZOS
#ifndef __LONGNAME__ 
#define __LONGNAME__ 1
#else
#warning __LONGNAME__ was already defined for a non-z/OS platform
#endif
#endif

#if defined(__ZOWE_OS_ZOS) && (__CHARSET_LIB == 0)
#define __ZOWE_EBCDIC 1
#else
#define __ZOWE_ASCII 1
#endif


/* shared definitions below */

#if defined(__ZOWE_ARCH_Z) && !defined(__ZOWE_OS_LINUX)
typedef void *__ptr32 Addr31;
#else
typedef uint32_t Addr31;
#endif

typedef unsigned int uint32;
typedef unsigned char flagbits1;
typedef unsigned short flagbits2;


#ifdef __ZOWE_64

typedef int64_t int64;
typedef uint64_t uint64;

#define INT64_CHARPTR(x) ((char*)((int64)x))
#define INT64_INT(x) ((int)x)
#define INT_INT64(x) ((int64)x)
#define CHARPTR_INT64(x) ((int64)x)
#define INT2PTR(x) ((void*)((int64_t)(x)))
#define INT64_LL(x) ((long long)(x))
#define UINT64_ULL(x) ((unsigned long long)(x))

#else

typedef long long int64;
typedef unsigned long long uint64;
#define INT64_CHARPTR(x) ((char*)((int)x))
#define INT64_INT(x) ((int)x)
#define INT_INT64(x) ((int64)x)
#define CHARPTR_INT64(x) ((int64)((int)x))
#define INT2PTR(x) ((void*)(x))
#define INT64_LL(x) ((long long)(x))
#define UINT64_ULL(x) ((unsigned long long)(x))

#endif

#ifdef __ZOWE_OS_ZOS
/* things normally found in /sys/types.h or features.h which are not metal compatible */
#ifdef _LP64
 #ifndef     __pad31
 #define     __pad31(n,l)
 #endif

 #ifndef     __pad64
 #define     __pad64(n,l)  char n[l];
 #endif

 #ifdef _EXT
   #ifndef __ptr31
   #define __ptr31(t,n)    t *__ptr32 n;
   #endif
 #else
   #ifndef __ptr31
   #define __ptr31(t,n)    unsigned int n;
   #endif
 #endif
#else
 #ifndef     __pad31
 #define     __pad31(n,l)  char n[l];
 #endif

 #ifndef     __pad64
 #define     __pad64(n,l)
 #endif

 #ifndef    __ptr31
 #define    __ptr31(t,n)  t *n;
 #endif
#endif

#if !defined(__u_char)
  #define __u_char    1
  typedef  unsigned char  u_char;
#endif

#if !defined(__u_int)
  #define __u_int          1
  typedef  unsigned int   u_int;
#endif

#if !defined(__ushort)
  #define __ushort         1
  typedef  unsigned short ushort;
#endif

#if !defined(__u_short)
  #define __u_short   1
  typedef  unsigned short u_short;
#endif

#if !defined(__u_long)
  #define __u_long    1
  typedef  unsigned long  u_long;
#endif
#endif

#ifdef METTLE

#define ASM_PREFIX 

#else

#ifndef _LP64
#define ASM_PREFIX " SYSSTATE ARCHLVL=2 \n" \
                   " IEABRCX DEFINE \n"
#else
#define ASM_PREFIX " SYSSTATE ARCHLVL=2,AMODE64=YES \n" \
                   " IEABRCX DEFINE \n"
#endif

#endif


#ifdef __ZOWE_64
#define INT_FROM_POINTER(x) ((int)((int64)(x)))
#define UINT_FROM_POINTER(x) ((unsigned int)((int64)(x)))
#define POINTER_FROM_INT(x) ((void*)((int64)(x)))
#define POINTER_FROM_UINT(x) ((void*)((int64)(x)))
#else
#define INT_FROM_POINTER(x) ((int)(x))
#define UINT_FROM_POINTER(x) ((unsigned int)(x))
#define POINTER_FROM_INT(x) ((void*)(x))
#define POINTER_FROM_UINT(x) ((void*)(x))
#endif

#ifdef __ZOWE_64
#define PAD_LONG(x,y) y
#else
#define PAD_LONG(x,y) int filler##x; y
#endif

typedef struct EightCharString_tag {
  char text[8];
} EightCharString;

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

