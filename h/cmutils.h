

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef H_CMUTILS_H_
#define H_CMUTILS_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#else
#include <stddef.h>
#endif

#include "zowetypes.h"

#define CROSS_MEMORY_ALET_PASN  0
#define CROSS_MEMORY_ALET_SASN  1
#define CROSS_MEMORY_ALET_HASN  2

#ifndef __LONGNAME__

#define cmCopyToSecondaryWithCallerKey CMCPTSSK
#define cmCopyFromSecondaryWithCallerKey CMCPFSSK
#define cmCopyToPrimaryWithCallerKey CMCPTPSK
#define cmCopyFromPrimaryWithCallerKey CMCPFPSK
#define cmCopyToHomeWithCallerKey CMCPTHSK
#define cmCopyFromHomeWithCallerKey CMCPFHSK
#define cmCopyWithSourceKeyAndALET CMCPYSKA
#define cmAlloc CMALLOC
#define cmFree CMFREE
#define cmAlloc2 CMALLOC2
#define cmFree2 CMFREE2

#endif

void cmCopyToSecondaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyFromSecondaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyToPrimaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyFromPrimaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyToHomeWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyFromHomeWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyWithSourceKeyAndALET(void *dest, const void *src, unsigned int key,
                                unsigned int alet, size_t size);

void *cmAlloc(unsigned int size, int subpool, int key);
void cmFree(void *data, unsigned int size, int subpool, int key);
void cmAlloc2(unsigned int size, int subpool, int key, void **resultPtr);
void cmFree2(void **dataPtr, unsigned int size, int subpool, int key);

#endif /* H_CMUTILS_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

