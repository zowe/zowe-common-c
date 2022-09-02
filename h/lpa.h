

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef H_LPA_H_
#define H_LPA_H_   1

#include "zowetypes.h"

ZOWE_PRAGMA_PACK

typedef struct LPMEA_tag {
  struct {
    char name[8];
    char inputFlags0;
#define LPMEA_INPUT_FLAGS0_FIXED                  0x80
#define LPMEA_INPUT_FLAGS0_PAGEPROTPAGE           0x40
#define LPMEA_INPUT_FLAGS0_STORAGEOWNERSYSTEM     0x20
    char inputFlags1;
  } inputInfo;
  struct {
    char outputFlags0;
    union {
      char modProbFunction;
      char successConcatNum;
    };
    union {
      struct {
        char deleteToken[8];
        struct {
          char entryPointAddrBytes0To2[3];
          char entryPointAddrByte3;
        } entryPointAddr;
        void * __ptr32 loadPointAddr;
        char modLen[4];
        char loadPointAddr2[4];
        char modLen2[4];
      } successInfo;
      struct {
        char retCode[4];
        char rsnCode[4];
      } retRsnCodes;
      struct {
        char abendCode[4];
        char abendRsnCode[4];
      } abendRsnCodes;
    } stuff; /* name from IBM doc */
  } outputInfo;
} LPMEA;

typedef struct LPMED_tag {
  struct {
    char name[8];
    char deleteToken[8];
    char inputFlags0;
    char inputFlags1;
  } inputInfo;
  struct {
    char outputFlags0;
    char modProbFunction;
  } outputInfo;
} LPMED;

ZOWE_PRAGMA_PACK_RESET

#pragma map(lpaAdd, "LPAADD")
#pragma map(lpaDelete, "LPADEL")

int lpaAdd(LPMEA * __ptr32 lpmea,
           EightCharString  * __ptr32 ddname,
           EightCharString  * __ptr32 dsname,
           int *reasonCode);

int lpaDelete(LPMEA * __ptr32 lpmea, int *reasonCode);

#endif /* H_LPA_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
