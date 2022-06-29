# Supported Targets

As of 2022 the entire zowe-common-c library is intended to work on ZOS 2.3 and later.   Also most of this repo works on Unix/Linux, Windows and Apple Mac.   For every target OS aside from ZOS, Clang 12 or later is used.  Furthermore, all non-ZOS code is built for 64 bit addressing, as smaller addresses are rare outside of embedded devices, etc.  

# ZOS Target

## ZOS C Compiler History  ( since 2000's )

The IBM XLC Compiler has been used on Zowe and its closed source precursors since the mid 2000's.   Around 2009/2010 the team started to use the new Metal option for inline assembly and ease of interfacing with OS linkage and total control over memory management.  At some release (2.1 or 2.2) the non-Metal support for inline assembly was added and these libraries and more development shifted to C from HLASM.   Most platform detection and variation is in the header file h/zowetypes.h.  Originally all the code used AMODE=31, but the need for greater headroom and use of shared memory (a 64-bit only feature) drove support for supporting LP64.   The metal option continues to use an upward-growing stack with R13 as frame pointer, but LE 64-bit only supports XPLINK, (downward R4-based stack).  So the codebase supported the following targets as of 2020:

- LE 31 Traditional (aka Fastlink)
- LE 64 XPLINK
- Metal 31
- Metal 64

The code has a fair number of #ifdef's that manage these differences.   Making build scripts can be challenging because of the wide variety of options.  

## What about open source on ZOS?

There were enough differences between XLC and GCC and Clang that the zowe-common-c project used no 3rd party open source. However, beginning in 2020 with the advent of xlclang, open source adoption is starting, and does not seem to have much limitation.  xlclang is based on a Clang-derived front end (syntax, static analysis, types, etc) and the XLC backend (IR, optimization, codegen).   And although xlclang is great, it brings another target to this codebase and since it's not a superset of any of the existing targets it's introduction requires care.  There's really thorough comparison of xlc and xlclang at:

[Benefits of upgrading to XL C/C++ V2.4.1 for z/OS V2.4 ](https://www.ibm.com/support/pages/sites/default/files/inline-files/$FILE/zosxlcpp_v2.4.1_benefits.pdf)

## How do we use xlclang and xlc together?

When projects like zowe/zss are now using some open source (e.g. libyaml), the build compiles the OSS files with xlclang and the zowe-common-c "native" files with xlc and links them together.   Because xlclang only supports AMODE=64 and XPLINK stacks, the xlc builds use the matching options.   They both create standard GOFF object files and the binder (linker for you non-Z folks) has no problems.  

Actually most of zowe-common-c can compile with xlclang (or clang off ZOS).  There are specific issues when files use obsolete non-standard pragmas,etc.  If anybody can find a definitive differences list, please submit a PR to this document.

There are many nits in porting OSS to ZOS.  There are differences, and some omissions in standard libaries.   And a lot of open source code has Linux-isms in it that violate language and POSIX standards.  So it is rarely just "make and run".

## Enter Open XL C/C++ 1.1

Open XL C/C++ is essentially Clang 14 for ZOS.  And it's GA as of May 2022.  Porting open-source to ZOS should be even broader in scope and simpler in implementation than xlclang.  Please read the migration guide at:

[Migration Guide](https://www.ibm.com/docs/en/SSUMVNF_1.1.0/pdf/migrate.pdf).

This repo does not exploit it yet, but that may change soon.  

## Language Standards

As of Zowe 2.0 (2022) feel free to use C11 in most places.  We had been holding at C99 for a long time, but the support for Atomics, type-generics, anonymous struct/union, Unicode, etc. is valuable and becoming more common in OSS.  

# Question, Gotcha's and Wishlists

## Questions

- Will all 3 streams continue indefinitely (XLC,XLCLANG,OPENXLC/CLANG)?
- Will other non-XPLINK stack support come to xlclang or clang
- NR is deprecated and the doc refers us to the clang documentation. Clang syntax doesn't seem to work. The question is, are there examples of how to use all the __asm__ features in Open XL C/C++? Are there plans to add examples to the Open XL C/C++ doc?

## Gotcha's

- XLC does allow integer bitfields for all integer widths
- use "_Packed", not pragma pack(...) - works for all 3 compilers.

## Wishlists

- ZOS Macros for inline ASM in Open XLC (clang)
- Thread local support.  

### OSS Ports
- CMake 
- libuv
- openssl 
- libcurl
- 
