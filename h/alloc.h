

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ALLOC__
#define __ALLOC__ 1

#include "zowetypes.h"

/** \file
 *  \brief alloc.h defines an interface for allocating memory that enables some features that are special to each platform.
 *
 *  Some platforms such as Windows do not support the richness of specification that zOS does and some parameters 
 *  are ignored.
 */

/* The standard for our H files is to make short-naming the default, but
   follow the standard XLC symbol __LONGNAME__ which is introduced by compiler option "longname"
   */

#ifndef __LONGNAME__
#define safeMalloc SAFEMLLC
#define safeMalloc2 SAFEMLC2
#define safeMalloc31 SAFMLC31
#define safeMalloc31Key8 SAFMLC38
#define safeFree31 SAFFRE31
#define safeFree31Key8 SAFFRE38
#define safeFree64 SAFFRE64
#define safeFree64ByToken SFFRE64T
#define safeMalloc64 SAFMLC64
#define safeMalloc64ByToken SFMLC64T
#define safeFree64v2 SAFRE642
#define safeMalloc64v2 SAMLC642
#define safeFree64v3 SAFRE643
#define safeMalloc64v3 SAMLC643
#endif

void *malloc31(size_t size);
void free31(void *data, size_t size);

/**
 *   safeMalloc is the primary heap memory allocated for Zowe COMMON c code.   It
 *   includes a second argument that defines the call site for diagnostics including leak detection.
 */

void *safeMalloc(size_t size, const char *site);
void *safeMalloc2(size_t size, const char *site, int *indicator);
void *safeMalloc31(size_t size, const char *site);
void *safeMalloc31Key8(size_t size, const char *site);

/** 
 *   safeFree is the primary de-allocator of memory for Zowe COMMON code.
 */

void safeFree(void *data, size_t size);
void safeFree31(void *data, size_t size);
void safeFree31Key8(void *data, size_t size);

void *safeMalloc64(size_t  size, const char *site);
void *safeMalloc64ByToken(size_t size, const char *site, long long token);


void safeFree64(void *data, size_t size);
void safeFree64ByToken(void *data, size_t size, long long token);

void *safeRealloc(void *ptr, int size, int oldSize, char *site);

#if defined(__ZOWE_OS_ZOS)
/* 64-bit allocator and de-allocator v2. */
char *safeMalloc64v2(unsigned long long size, int zeroOut, char *site,
                     int *returnCode, int *sysRC, int *sysRSN);
int safeFree64v2(void *data, unsigned long long size, int *sysRC, int *sysRSN);
/* 64-bit allocator and de-allocator v3, it is the same as v2 but allocates storage for a single owner */
#ifdef METTLE
char *safeMalloc64v3(unsigned long long size, int zeroOut, char *site,
                     int *returnCode, int *sysRC, int *sysRSN);
int safeFree64v3(void *data, unsigned long long size, int *sysRC, int *sysRSN);
#endif

#define ALLOC64V2_RC_TCBTOKEN_FAILURE 2
#define ALLOC64V2_RC_IARV64_FAILURE   4
#define ALLOC64V2_RC_IARST64_FAILURE  8
#define FREE64V2_RC_TCBTOKEN_FAILURE  2
#define FREE64V2_RC_IARV64_FAILURE    4
#endif

#ifdef __ZOWE_OS_ZOS
void *allocECSA(size_t size, unsigned int key);
int freeECSA(void *data, size_t size, unsigned int key);
#endif

#ifdef __ZOWE_OS_ZOS

#define STRUCT31_NAME(name) name
#define STRUCT31_FIELDS(...) __VA_ARGS__

#if defined(_LP64) && !defined(METTLE)
#define ALLOC_STRUCT31(name, fields) \
  struct name {\
  fields \
} *name \
  = (struct name *)safeMalloc31(sizeof(struct name), "mem31:"#name)
#define DEFINE_STRUCT31(name, fields) \
  struct name {\
  fields \
} *name \
  = NULL
#define GET_STRUCT31(name) \
 name = (struct name *)safeMalloc31(sizeof(struct name), "mem31:"#name)
#else
#define STRUCT31_LOCAL_VAR_NAME(name) stack_alloc_struct31_##name
#define ALLOC_STRUCT31(name, fields) \
  struct STRUCT31_LOCAL_VAR_NAME(name) {\
  fields \
} STRUCT31_LOCAL_VAR_NAME(name); \
struct STRUCT31_LOCAL_VAR_NAME(name) *name = &STRUCT31_LOCAL_VAR_NAME(name); \
memset(name, 0, sizeof(struct STRUCT31_LOCAL_VAR_NAME(name)))
#define DEFINE_STRUCT31(name, fields) ALLOC_STRUCT31(name,fields)
#define GET_STRUCT31(name)
#endif

#if defined(_LP64) && !defined(METTLE)
#define FREE_STRUCT31(name) \
  safeFree31(name, sizeof(struct name)); \
  name = NULL
#define FREE_COND_STRUCT31(name) \
  if (name) \
  safeFree31(name, sizeof(struct name)); \
  name = NULL
#else
#define FREE_STRUCT31(name) \
  name = NULL
#define FREE_COND_STRUCT31(name) \
  name = NULL
#endif

#endif 


#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

