

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ALLOC__
#define __ALLOC__ 1

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
#endif

char *malloc31(int size);
void free31(void *data, int size);

/**
 *   safeMalloc is the primary heap memory allocated for Zowe COMMON c code.   It
 *   includes a second argument that defines the call site for diagnostics including leak detection.
 */

char *safeMalloc(int size, char *site);
char *safeMalloc2(int size, char *site, int *indicator);
char *safeMalloc31(int size, char *site);
char *safeMalloc31Key8(int size, char *site);

/** 
 *   safeFree is the primary de-allocator of memory for Zowe COMMON code.  It is uses a char pointer
 *   rather than void pointer for historical reasons.  This will be fixed at some time. 
 */

void safeFree(char *data, int size);
void safeFree31(char *data, int size);
void safeFree31Key8(char *data, int size);

char *safeMalloc64(int size, char *site);
char *safeMalloc64ByToken(int size, char *site, long long token);


void safeFree64(char *data, int size);
void safeFree64ByToken(char *data, int size, long long token);

#ifdef __ZOWE_OS_ZOS
char *allocECSA(int size, int key);
int freeECSA(char *data, int size, int key);
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
  safeFree31((char *)name, sizeof(struct name)); \
  name = NULL
#define FREE_COND_STRUCT31(name) \
  if (name) \
  safeFree31((char *)name, sizeof(struct name)); \
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

