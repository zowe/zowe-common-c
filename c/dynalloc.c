

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdint.h>
#include <metal/string.h>
#include "qsam.h"
#include "metalio.h"
#else
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dynit.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "dynalloc.h"
#include "logging.h"

#define TEXT_UNIT_ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

TextUnit *createSimpleTextUnit2(int key, char *value, int firstParameterLength) {
  TextUnit *textUnit = NULL;
  int length;

  length = sizeof(TextUnit) + firstParameterLength;
  textUnit = (TextUnit *) safeMalloc31(length, "TextUnit");
  if (textUnit == NULL) {
    return NULL;
  }

  textUnit->key = key;
  textUnit->number_of_parameters = (value == NULL) ? 0 : 1;
  textUnit->first_parameter_length = firstParameterLength;
  if (value != NULL) {
    memcpy(((char * )textUnit) + sizeof(TextUnit), value, firstParameterLength);
  }
  return textUnit;
}

TextUnit *createSimpleTextUnit(int key, char *value) {
  int firstParameterLength = (value == NULL) ? 0 : strlen(value);
  return createSimpleTextUnit2(key, value, firstParameterLength);
}

TextUnit *createIntTextUnit(int key, int value) {
  return createSimpleTextUnit2(key, (char*)&value, sizeof(value));
}

TextUnit *createInt8TextUnit(int key, int8_t value) {
  return createSimpleTextUnit2(key, (char*)&value, sizeof(value));
}

TextUnit *createInt16TextUnit(int key, int16_t value) {
  return createSimpleTextUnit2(key, (char*)&value, sizeof(value));
}

TextUnit *createCharTextUnit(int key, char value) {
  char valueArray[2];
  valueArray[0] = value;
  valueArray[1] = 0;
  return createSimpleTextUnit2(key, valueArray, 1);
}

TextUnit *createCompoundTextUnit(int key, char **values, int valueCount) {

  TextUnit *textUnit = NULL;
  int firstParameterLength;
  int length, i, valuesLength = 0;
  char *fillPointer;

  for (i = 0; i < valueCount; i++) {
    valuesLength += (2 + strlen(values[i]));
  }

  length = sizeof(TextUnit) + (valuesLength - 2); /* -2 for first parm length in header */
  textUnit = (TextUnit *)safeMalloc31(length, "Compound TextUnit");
  if (textUnit == NULL) {
    return NULL;
  }

  textUnit->key = key;
  textUnit->number_of_parameters = valueCount;
  fillPointer = ((char *)textUnit) + sizeof(TextUnit) - 2;
  for (i = 0; i < valueCount; i++) {
    int valueLength = strlen(values[i]);
    *((short *)fillPointer) = valueLength;
    fillPointer += 2;
    strncpy(fillPointer, values[i], valueLength);
    fillPointer += valueLength;
  }

  return textUnit;
}

static int getTextUnitSize(TextUnit *unit) {

  if (unit == NULL) {
    return 0;
  }

  int tuSize = sizeof(TextUnit) - sizeof(unit->first_parameter_length);

  struct {
    short size;
    char value[0];
  } *currParm = (void *)&unit->first_parameter_length;

  for (int i = 0; i < unit->number_of_parameters; i++) {
    tuSize += (sizeof(currParm->size) + currParm->size);
    currParm = (void *)(currParm->value + currParm->size);
  }

  return tuSize;
}

void freeTextUnit(TextUnit *textUnit) {
  if (textUnit == NULL) {
    return;
  }
  safeFree31((char *)textUnit, getTextUnitSize(textUnit));
}

static void freeTextUnitArray(TextUnit * __ptr32 * array, unsigned int count) {
  for (unsigned int i = 0; i < count; i++) {
    turn_off_HOB(array[i]);
    freeTextUnit(array[i]);
    array[i] = NULL;
  }
}

ZOWE_PRAGMA_PACK

typedef struct SVC99RequestBlock_tag {

  char requestBlockLen;
  char verbCode;

  short flags1;
#define SVC99_FLAG1_ONCNV 0x8000
#define SVC99_FLAG1_NOCNV 0x4000
#define SVC99_FLAG1_NOMNT 0x2000
#define SVC99_FLAG1_JBSYS 0x1000
#define SVC99_FLAG1_CNENQ 0x0800
#define SVC99_FLAG1_GDGNT 0x0400
#define SVC99_FLAG1_MSGL0 0x0200
#define SVC99_FLAG1_NOMIG 0x0100

  short errorReasonCode;
  short infoReasonCode;

  TextUnit * __ptr32 * __ptr32 textUnits;
  void * __ptr32 requestBlockExtension;

  int flags2;

} SVC99RequestBlock;

ZOWE_PRAGMA_PACK_RESET

#ifdef METTLE

typedef struct DynallocParms_tag {
  SVC99RequestBlock svc99ReqBlock;
  SVC99RequestBlock * __ptr32 svc99ReqBlockAddress;
} DynallocParms;

#else

typedef __dyn_t DynallocParms;

#endif

static void dynallocParmsSetTextUnits(DynallocParms *parms,
                                      TextUnit * __ptr32 * units,
                                      int unitCount) {

#ifdef METTLE
  parms->svc99ReqBlock.textUnits = units;
#else
  parms->__miscitems = (char * __ptr32 * __ptr32)units;
#endif

}

static int dynallocParmsGetErrorCode(DynallocParms *parms) {

#ifdef METTLE
  return parms->svc99ReqBlock.errorReasonCode;
#else
  return parms->__errcode;
#endif

}

static int dynallocParmsGetInfoCode(DynallocParms *parms) {

#ifdef METTLE
  return parms->svc99ReqBlock.infoReasonCode;
#else
  return parms->__infocode;
#endif

}

static void dynallocParmsInit(DynallocParms *parms) {

#ifdef METTLE
  memset(parms, 0, sizeof(DynallocParms));
  parms->svc99ReqBlock.requestBlockLen = sizeof(parms->svc99ReqBlock);
  parms->svc99ReqBlock.verbCode = S99VRBAL;
  parms->svc99ReqBlock.textUnits = NULL;
  parms->svc99ReqBlockAddress =
      (SVC99RequestBlock * __ptr32)((int)&parms->svc99ReqBlock | 0x80000000);
#else
  dyninit(parms);
#endif

}

static void dynallocParmsTerm(DynallocParms *parms) {

  memset(parms, 0, sizeof(DynallocParms));

}

static int invokeSVC99(SVC99RequestBlock * __ptr32 * requestBlockHandle) {

  int rc = 0;

  __asm(
      ASM_PREFIX
      "         LA    1,0(,%[rb])                                              \n"
      "         SVC   99                                                       \n"
      "         ST    15,%[rc]                                                 \n"
      : [rc]"=m"(rc)
      : [rb]"r"(requestBlockHandle)
      : "r0", "r1", "r14", "r15"
  );

  return rc;
}

static int invokeDynalloc(DynallocParms *parms) {

#ifdef METTLE
  return invokeSVC99(&parms->svc99ReqBlockAddress);
#else
  return dynalloc(parms);
#endif

}

/* open a stream to the internal reader */
int AllocIntReader(char *datasetOutputClass,
                   char *ddnameResult,
                   char *errorBuffer) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[6];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createInt8TextUnit(DALRECFM, DALRECFM_F);
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createInt16TextUnit(DALLRECL, 80);
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    /* NULL means default */
    below2G->textUnits[2] = createSimpleTextUnit(DALSYSOU, datasetOutputClass);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    /* SYSOUT program name */
    below2G->textUnits[3] = createSimpleTextUnit(DALSPGNM, "INTRDR");
    if (below2G->textUnits[3] == NULL) {
      break;
    }

    below2G->textUnits[4] = createSimpleTextUnit(DALRTDDN, "12345678");
    if (below2G->textUnits[4] == NULL) {
      break;
    }

    /* deallocate at close (otherwise, would deallocate at step termination,
     * or at explicit deallocation using svc99 verb code = 2) */
    below2G->textUnits[5] = createSimpleTextUnit(DALCLOSE, 0);
    if (below2G->textUnits[5] == NULL) {
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    if ((rc = invokeDynalloc(parms)) != 0) {
      sprintf(errorBuffer, "dynalloc return code=%d, error=%04x, info=%04x", rc,
              dynallocParmsGetErrorCode(parms), dynallocParmsGetInfoCode(parms));
    } else{
      strncpy(ddnameResult, below2G->textUnits[4]->first_value,
              below2G->textUnits[4]->first_parameter_length);
    }

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

int dynallocSharedLibrary(char *ddname, char *dsn, char *errorBuffer) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[6];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createSimpleTextUnit(DALDDNAM, ddname);
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createInt8TextUnit(DALRECFM, DALRECFM_U);
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    below2G->textUnits[2] = createInt16TextUnit(DALLRECL, 0);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    below2G->textUnits[3] = createSimpleTextUnit(DALDSNAM, dsn);
    if (below2G->textUnits[3] == NULL) {
      break;
    }

    below2G->textUnits[4] = createCharTextUnit(DALSTATS, DISP_SHARE);
    if (below2G->textUnits[4] == NULL) {
      break;
    }

    /* deallocate at close (otherwise, would deallocate at step termination,
     * or at explicit deallocation using svc99 verb code = 2) */
    below2G->textUnits[5] = createSimpleTextUnit(DALCLOSE, 0);
    if (below2G->textUnits[5] == NULL) {
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    if ((rc = invokeDynalloc(parms)) != 0) {
      fflush(stdout);
      sprintf(errorBuffer, "dynalloc return code=%d, error=%04x, info=%04x", rc,
              dynallocParmsGetErrorCode(parms), dynallocParmsGetInfoCode(parms));
    }

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

int dynallocUSSDirectory(char *ddname, char *ussPath, char *errorBuffer) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[4];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createSimpleTextUnit(DALDDNAM, ddname);
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createInt8TextUnit(DALRECFM, DALRECFM_U);
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    below2G->textUnits[2] = createInt16TextUnit(DALLRECL, 0);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    below2G->textUnits[3] = createSimpleTextUnit(DALPATH, ussPath);
    if (below2G->textUnits[3] == NULL) {
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    if ((rc = invokeDynalloc(parms)) != 0) {
      fflush(stdout);
      sprintf(errorBuffer, "dynalloc return code=%d, error=%04x, info=%04x", rc,
              dynallocParmsGetErrorCode(parms), dynallocParmsGetInfoCode(parms));
    }

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

int dynallocUSSOutput(char *ddname, char *ussPath, char *errorBuffer) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[7];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createSimpleTextUnit(DALDDNAM, ddname);
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createInt8TextUnit(DALRECFM, DALRECFM_F);
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    below2G->textUnits[2] = createInt16TextUnit(DALLRECL, 133);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    below2G->textUnits[3] = createSimpleTextUnit(DALPATH, ussPath);
    if (below2G->textUnits[3] == NULL) {
      break;
    }

    /* opts (OWRONLY,OCREAT,OTRUNC),
       mode (SIRWXU,SIRGRP)          */
    below2G->textUnits[4] = createIntTextUnit(DALPOPT, DALPOPT_OCREAT |
                                              DALPOPT_OWRONLY |
                                              DALPOPT_OTRUNC);
    if (below2G->textUnits[4] == NULL) {
      break;
    }

    below2G->textUnits[5] = createIntTextUnit(DALPMDE, DALPMDE_SIRWXU |
                                              DALPMDE_SIRGRP);
    if (below2G->textUnits[5] == NULL) {
      break;
    }

    /* means text with newlines */
    below2G->textUnits[6] = createCharTextUnit(DALFDAT, 0x40);
    if (below2G->textUnits[6] == NULL) {
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    if ((rc = invokeDynalloc(parms)) != 0) {
      sprintf(errorBuffer, "dynalloc return code=%d, error=%04x, info=%04x", rc,
              dynallocParmsGetErrorCode(parms), dynallocParmsGetInfoCode(parms));
    }

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

/* allocate a "secret" SAPI output data set */
int AllocForSAPI(char *dsn,
                 char *ddnameResult,
                 char *ssname,
                 void *browseToken,
                 char *errorBuffer) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[4];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createSimpleTextUnit(DALDSNAM, dsn);
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createSimpleTextUnit(DALRTDDN, ddnameResult);
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    below2G->textUnits[2] = createSimpleTextUnit(DALSSREQ, ssname);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    below2G->textUnits[3] = browseToken;
    if (below2G->textUnits[3] == NULL) {
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    if ((rc = invokeDynalloc(parms)) != 0) {
      sprintf(errorBuffer, "dynalloc return code=%d, error=%04x, info=%04x", rc,
              dynallocParmsGetErrorCode(parms), dynallocParmsGetInfoCode(parms));
    } else {
      char *returnedDDName = below2G->textUnits[1]->first_value;
      memcpy(ddnameResult, returnedDDName, 8);
    }

  } while (0);

  /* don't free the last entry - it doesn't belong to us */
  freeTextUnitArray(below2G->textUnits, textUnitCount - 1);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

int AllocForDynamicOutput(char *outDescName, /* input - may be null */
                          char *ddnameResult, /* output, pass in 8-blanks */
                          char *ssname,       /* ? */
                          char *errorBuffer) { /* 256 for good fortune */

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[4];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createSimpleTextUnit(DALRTDDN, ddnameResult);
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createSimpleTextUnit(DALSYSOU, NULL);
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    below2G->textUnits[2] = createSimpleTextUnit(DALCLOSE, NULL);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    below2G->textUnits[2] = createSimpleTextUnit(DALCLOSE, NULL);
    if (outDescName != NULL) {

      below2G->textUnits[3] = createSimpleTextUnit(DALOUTPT, outDescName);
      if (below2G->textUnits[3] == NULL) {
        break;
      };

      turn_on_HOB(below2G->textUnits[3]);
    } else {
      turn_on_HOB(below2G->textUnits[2]);
    }

    if ((rc = invokeDynalloc(parms)) != 0) {
      sprintf(errorBuffer, "dynalloc return code=%d, error=%04x, info=%04x", rc,
              dynallocParmsGetErrorCode(parms), dynallocParmsGetInfoCode(parms));
    } else {
      char *returnedDDName = below2G->textUnits[0]->first_value;
      memcpy(ddnameResult, returnedDDName, 8);
    }

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

int DeallocDDName(char *ddname) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      SVC99RequestBlock plist;
      SVC99RequestBlock * __ptr32 plistAddress;
      TextUnit * __ptr32 textUnits[1];
    )
  );

  below2G->plistAddress =
      (SVC99RequestBlock * __ptr32)((int)&below2G->plist | 0x80000000);
  SVC99RequestBlock *plist = &below2G->plist;
  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);

  memset(plist, 0, sizeof(plist));
  plist->requestBlockLen = sizeof(SVC99RequestBlock);
  plist->verbCode = S99VRBUN;
  plist->flags1 = 0x00;
  plist->textUnits = below2G->textUnits;

  int rc;

  do {

    below2G->textUnits[0] = createSimpleTextUnit(DALDDNAM, ddname);
    if (below2G->textUnits[0] == NULL) {
      rc = -1;
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    if ((rc = invokeSVC99(&below2G->plistAddress)) != 0) {
      if (plist->errorReasonCode == 0x0438) {
        ;/* DDNAME was not in use */
      } else {
//        printf("ret=%d, error=%04x\n", rc, plist->errorReasonCode);
      }
    }

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

int dynallocDataset(DynallocInputParms *inputParms, int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[3];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createSimpleTextUnit2(DALDDNAM,
                                                  inputParms->ddName,
                                                  sizeof(inputParms->ddName));
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createSimpleTextUnit2(DALDSNAM,
                                                  inputParms->dsName,
                                                  sizeof(inputParms->dsName));
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    below2G->textUnits[2] = createCharTextUnit(DALSTATS, inputParms->disposition);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    rc = invokeDynalloc(parms);

    *reasonCode = dynallocParmsGetInfoCode(parms) +
        (dynallocParmsGetErrorCode(parms) << 16);

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;

}

int dynallocDatasetMember(DynallocInputParms *inputParms, int *reasonCode,
                          char *member) {

  int memberLength = strlen(member);
  if (memberLength > 8) {
    return 16;
  }

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnit * __ptr32 textUnits[4];
    )
  );

  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);
  dynallocParmsSetTextUnits(parms, below2G->textUnits, textUnitCount);

  int rc;

  do {

    rc = -1;

    below2G->textUnits[0] = createSimpleTextUnit2(DALDDNAM,
                                                  inputParms->ddName,
                                                  sizeof(inputParms->ddName));
    if (below2G->textUnits[0] == NULL) {
      break;
    }

    below2G->textUnits[1] = createSimpleTextUnit2(DALDSNAM,
                                                  inputParms->dsName,
                                                  sizeof(inputParms->dsName));
    if (below2G->textUnits[1] == NULL) {
      break;
    }

    below2G->textUnits[2] = createSimpleTextUnit2(DALMEMBR, member, memberLength);
    if (below2G->textUnits[2] == NULL) {
      break;
    }

    below2G->textUnits[3] = createCharTextUnit(DALSTATS, inputParms->disposition);
    if (below2G->textUnits[3] == NULL) {
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    rc = invokeDynalloc(parms);

    *reasonCode = dynallocParmsGetInfoCode(parms) +
        (dynallocParmsGetErrorCode(parms) << 16);

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  dynallocParmsTerm(parms);
  parms = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;

}

/* The function deallocates a previously allocated dataset and destroys its DD label. */
int unallocDataset(DynallocInputParms *inputParms, int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      SVC99RequestBlock requestBlock;
      SVC99RequestBlock * __ptr32 requestBlockAddress;
      TextUnit * __ptr32 textUnits[1];
    )
  );

  below2G->requestBlockAddress =
      (SVC99RequestBlock * __ptr32)((int)&below2G->requestBlock | 0x80000000);
  SVC99RequestBlock *requestBlock = &below2G->requestBlock;

  unsigned int textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);

  memset(requestBlock, 0, sizeof(SVC99RequestBlock));
  requestBlock->requestBlockLen = sizeof(SVC99RequestBlock);
  requestBlock->verbCode = S99VRBUN;
  requestBlock->textUnits = below2G->textUnits;

  int rc;

  do {

    below2G->textUnits[0] = createSimpleTextUnit2(DALDDNAM,
                                                  inputParms->ddName,
                                                  sizeof(inputParms->ddName));
    if (below2G->textUnits[0] == NULL) {
      rc = -1;
      break;
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    rc = invokeSVC99(&below2G->requestBlockAddress);

    *reasonCode = requestBlock->infoReasonCode +
        (requestBlock->errorReasonCode << 16);

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;

}

static void setAllocsFlags(SVC99RequestBlock *rb, DynallocAllocFlags flags) {

  if (flags & DYNALLOC_ALLOC_FLAG_NO_CONVERSION) {
    rb->flags1 |= SVC99_FLAG1_NOCNV;
  }

  if (flags & DYNALLOC_ALLOC_FLAG_NO_MOUNT) {
    rb->flags1 |= SVC99_FLAG1_NOMNT;
  }

}

int dynallocAllocDataset(const DynallocDatasetName *dsn,
                         const DynallocMemberName *member,
                         DynallocDDName *ddName,
                         DynallocDisposition disp,
                         DynallocAllocFlags flags,
                         int *sysRC, int *sysRSN) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      SVC99RequestBlock requestBlock;
      SVC99RequestBlock * __ptr32 requestBlockAddress;
      TextUnit * __ptr32 textUnits[4];
    )
  );

  below2G->requestBlockAddress =
      (SVC99RequestBlock * __ptr32)((int)&below2G->requestBlock | 0x80000000);
  SVC99RequestBlock *requestBlock = &below2G->requestBlock;

  unsigned textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);

  memset(requestBlock, 0, sizeof(SVC99RequestBlock));
  requestBlock->requestBlockLen = sizeof(SVC99RequestBlock);
  requestBlock->verbCode = S99VRBAL;
  requestBlock->textUnits = below2G->textUnits;
  setAllocsFlags(requestBlock, flags);

  TextUnit *ddNameInOutTU = NULL;

  int rc = RC_DYNALLOC_OK;
  int currTUIdx = 0;

  do {

    rc = RC_DYNALLOC_TU_ALLOC_FAILED;

    if (!memcmp(ddName->name, "????????", sizeof(ddName->name))) {
      ddNameInOutTU = createSimpleTextUnit2(DALRTDDN, "        ", 8);
    } else {
      ddNameInOutTU = createSimpleTextUnit2(DALDDNAM, ddName->name,
                                            sizeof(ddName->name));
    }

    below2G->textUnits[currTUIdx] = ddNameInOutTU;
    if (below2G->textUnits[currTUIdx] == NULL) {
      break;
    }
    currTUIdx++;

    below2G->textUnits[currTUIdx] = createSimpleTextUnit2(DALDSNAM,
                                                          (char *)dsn->name,
                                                          sizeof(dsn->name));
    if (below2G->textUnits[currTUIdx] == NULL) {
      break;
    }
    currTUIdx++;

    below2G->textUnits[currTUIdx] = createCharTextUnit(DALSTATS, disp);
    if (below2G->textUnits[currTUIdx] == NULL) {
      break;
    }
    currTUIdx++;

    if (member) {
      below2G->textUnits[currTUIdx] = createSimpleTextUnit2(DALMEMBR,
                                                            (char *)member->name,
                                                            sizeof(member->name));
      if (below2G->textUnits[currTUIdx] == NULL) {
        break;
      }
      currTUIdx++;
    }

    turn_on_HOB(below2G->textUnits[currTUIdx - 1]);

    int svc99RC = invokeSVC99(&below2G->requestBlockAddress);
    int svc99RSN = requestBlock->infoReasonCode +
        (requestBlock->errorReasonCode << 16);

    if (svc99RC == 0) {
      rc = RC_DYNALLOC_OK;
    } else {
      *sysRC = svc99RC;
      *sysRSN = svc99RSN;
      rc = RC_DYNALLOC_SVC99_FAILED;
    }

  } while (0);

  if (ddNameInOutTU) {
    memcpy(ddName->name, ddNameInOutTU->first_value, sizeof(ddName->name));
  }

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

static int dynallocUnallocDatasetByDDNameInternal(const DynallocDDName *ddName,
                                                  DynallocUnallocFlags flags,
                                                  int *sysRC, int *sysRSN,
                                                  bool removeDataset) {
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      SVC99RequestBlock requestBlock;
      SVC99RequestBlock * __ptr32 requestBlockAddress;
      TextUnit * __ptr32 textUnits[2];
    )
  );

  below2G->requestBlockAddress =
      (SVC99RequestBlock * __ptr32)((int)&below2G->requestBlock | 0x80000000);
  SVC99RequestBlock *requestBlock = &below2G->requestBlock;

  unsigned textUnitCount = TEXT_UNIT_ARRAY_SIZE(below2G->textUnits);

  memset(requestBlock, 0, sizeof(SVC99RequestBlock));
  requestBlock->requestBlockLen = sizeof(SVC99RequestBlock);
  requestBlock->verbCode = S99VRBUN;
  requestBlock->textUnits = below2G->textUnits;

  int rc = RC_DYNALLOC_OK;

  do {

    rc = RC_DYNALLOC_TU_ALLOC_FAILED;

    below2G->textUnits[0] = createSimpleTextUnit2(DALDDNAM,
                                                  (char *)ddName->name,
                                                  sizeof(ddName->name));
    if (below2G->textUnits[0] == NULL) {
      break;
    }
    
    if (removeDataset) {
      below2G->textUnits[1] = createCharTextUnit(DALNDISP, DISP_DELETE);
      if (below2G->textUnits[1] == NULL) {
        break;
      }
    }

    turn_on_HOB(below2G->textUnits[textUnitCount - 1]);

    int svc99RC = invokeSVC99(&below2G->requestBlockAddress);
    int svc99RSN = requestBlock->infoReasonCode +
        (requestBlock->errorReasonCode << 16);

    if (svc99RC == 0) {
      rc = RC_DYNALLOC_OK;
    } else {
      *sysRC = svc99RC;
      *sysRSN = svc99RSN;
      rc = RC_DYNALLOC_SVC99_FAILED;
    }

  } while (0);

  freeTextUnitArray(below2G->textUnits, textUnitCount);

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

int dynallocUnallocDatasetByDDName2(const DynallocDDName *ddName,
                                    DynallocUnallocFlags flags,
                                    int *sysRC, int *sysRSN,
                                    bool removeDataset) {
  return dynallocUnallocDatasetByDDNameInternal(ddName, flags, sysRC, sysRSN, removeDataset);
}

int dynallocUnallocDatasetByDDName(const DynallocDDName *ddName,
                                   DynallocUnallocFlags flags,
                                   int *sysRC, int *sysRSN) {
  return dynallocUnallocDatasetByDDNameInternal(ddName, flags, sysRC, sysRSN, FALSE);
}

/*************************/
#define FIELD_MAX_LENGTH    1024
#define REQUEST_MAX_LENGTH  2048
typedef int EXTF();
#pragma linkage(EXTF,OS_UPSTACK)

static int allocDataSetOpenString(dataSetRequest *request, char *buffer,
                             int bufferSize ) {
  char * ptr;
  char field[FIELD_MAX_LENGTH];
  int  status = 0, value;
  char *baseRequest = "alloc new catalog msg(2) ";
  char tempString[20] = {0};

  /* setup initial request string */
  strncpy(buffer, baseRequest, bufferSize);

  /* Initialize the DD/FI field */
  ptr = (request->ddName == NULL ?  DATASET_DEFAULT_DDNAME: request->ddName);
  snprintf(field, sizeof(field)-1, "FI(%s) ", ptr);
  strncat(buffer, field, bufferSize - strlen(buffer));

  /* Initialize daName */
  snprintf(field, sizeof(field)-1, "DA(%s) ", request->daName);
  strncat(buffer, field, bufferSize - strlen(buffer));

  /* Initialize data set type */
  if ((ptr = request->dsnType) != NULL) {
    snprintf(field, sizeof(field)-1, "DSNTYPE(%s) ", ptr);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize organization */
  if (request->organization != NULL) {
    snprintf(field, sizeof(field)-1, "DSORG(%s) ", request->organization);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize record format */
  if (request->recordFormat != NULL) {
    snprintf(field, sizeof(field)-1, "RECFM(%s) ", request->recordFormat);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize record length */
  if (request->recordLength != 0) {
    snprintf(field, sizeof(field)-1, "LRECL(%d) ", request->recordLength);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize block size*/
  if (request->blkSize != 0) {
    snprintf(field, sizeof(field)-1, "BLKSIZE(%d) ", request->blkSize);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize storage type */
  if (request->storageClass != NULL) {
    snprintf(field, sizeof(field)-1, "STORCLAS(%s) ", request->storageClass);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize  data type */
  if (request->fileData != NULL) {
    snprintf(field, sizeof(field)-1, "FILEDATA(%s) ", request->fileData);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize extended attribute */
  if (request->eattr != NULL) {
    strncpy(tempString, request->eattr, sizeof (tempString) -1);
    strupcase(tempString);
    if (strncmp(tempString, "TRUE", 5)) {
      snprintf(field, sizeof(field)-1, "EATTR(OPT) ");
      strncat(buffer, field, bufferSize - strlen(buffer));
    }
  }

  /* Initialize number of generations */
  if (request->numGenerations != 0) {
    snprintf(field, sizeof(field)-1, "MAXGENS(%d) ", request->numGenerations);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize volumn if defined */
  if (request->volume != NULL) {
    snprintf(field, sizeof(field)-1, "VOL(%s) ", request->volume);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize dataClass if defined */
  if (request->dataClass != NULL) {
    snprintf(field, sizeof(field)-1, "DATACLAS(%s) ", request->dataClass);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize management class if defined */
  if (request->manageClass != NULL) {
    snprintf(field, sizeof(field)-1, "MGMTCLAS(%s) ", request->manageClass);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize management class if defined */
  if (request->averageRecord != NULL) {
    snprintf(field, sizeof(field)-1, "AVGREC(%s) ", request->averageRecord);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize expiration data  */
  if (request->expiration != NULL) {
    snprintf(field, sizeof(field)-1, "EXPDL(%s) ", request->expiration);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }

  /* Initialize expiration data  */
  if (request->spaceUnit != NULL) {
    snprintf(field, sizeof(field)-1, "SPACE(%s) ", request->spaceUnit);
    strncat(buffer, field, bufferSize - strlen(buffer));
  }
  return status;
}

static void  allocDataSetCloseString(dataSetRequest *request, char *buffer,
                             int bufferSize ) {
  char * ptr;
  char *freeRequest = "free  ";
  char field[FIELD_MAX_LENGTH];
  char tempString[20] = {0};

  strncpy( buffer, freeRequest, bufferSize);

  /* Initialize the DD/FI field */
  ptr = (request->ddName == NULL ?  DATASET_DEFAULT_DDNAME: request->ddName);
  snprintf(field, sizeof(field)-1, "fi(%s) ", ptr);
  strncat(buffer, field, bufferSize - strlen(buffer));

  /* Initialize daName */
  snprintf(field, sizeof(field)-1, "da(%s) ", request->daName);
  strncat(buffer, field, bufferSize - strlen(buffer));
}

static void allocDataSetInterpretError(char *requestBuffer,
       int errorStatus, char *outBuffer, int bufferSize){

  char field[FIELD_MAX_LENGTH];
  int cause = 0, reasonIndex;
  char *reason;

  /* errorStatus between -20 and -9999 represents Key errors. */
  /* The key index is (-errorStatus -20)                      */
  /* errorStatus below -10000 has the IEFDB476 return code in */
  /* lower 2 digits (decimal) of the errorStatus.             */
  /* Status code between  -1610612737 and -2147483648    */
  /* contains the S99ERROR code.                         */
# define KEY_ERROR_BEGIN        20
# define KEY_ERROR_END        9999
# define KEY_ERROR_MOD         100
# define IEF_ERROR_BEGIN (-32*1024)
# define IEF_ERROR_END     (-10000)
# define IEF_ERROR_MOD          100
# define S99_ERROR_BEGIN   (-1610612737)
# define S99_ERROR_END     (-2147483648)
  reason = strtok(requestBuffer, " ");
  sprintf(field,"", "");
  if ((errorStatus <= -KEY_ERROR_BEGIN) &&
      (errorStatus >= -KEY_ERROR_END)) {
    cause = (-errorStatus - KEY_ERROR_BEGIN) % KEY_ERROR_MOD;
    for (int i=0; i < cause -1; i ++) {
      reason = strtok(NULL, " ");
      if (reason == NULL) {
        break;
      }
    }
    if (reason != NULL) {
      int reasondIndex = -errorStatus / KEY_ERROR_MOD;
      snprintf(field, sizeof(field)-1, " Field %s Error: Code %d",
               reason, reasondIndex);
    }
  }
  if ((IEF_ERROR_BEGIN <= errorStatus) && (errorStatus < IEF_ERROR_END)) {
    cause = (-errorStatus - 20) %IEF_ERROR_MOD;
    snprintf(field, sizeof(field)-1, "IEFDB476 Error Code %d", cause);
  }
  if ((errorStatus <= S99_ERROR_BEGIN)  && (errorStatus >= S99_ERROR_END)) {
    snprintf(field, sizeof(field)-1, "S99INFO Error Code %08x", errorStatus);
  }

  /* Write error message if one found */
  if ((outBuffer != NULL) && strlen(field)) {
    strncpy(outBuffer, field, bufferSize);
  }
}

int allocDataSet( dataSetRequest *request, char* message, int messageLength){
#ifdef __ZOWE_OS_ZOS
  char *baseRequest = "alloc new catalog msg(2) ";
  char tempString[20] = {0};
  char field[FIELD_MAX_LENGTH];
  int  status, value;
  char *ptr;

  /* Must supply daName */
  if (request->daName == NULL) {
    return -1;
  }

  /* Allocate structure in A31 space */
  EXTF *bpxwdyn=(EXTF *)fetch("BPXWDYN");
  if (bpxwdyn == NULL) {
    strncpy(message, "Unable to fetch system function",  messageLength);
    return -1;
  }

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char  request[REQUEST_MAX_LENGTH];
    )
  );

  status = allocDataSetOpenString(request, below2G->request,
                          sizeof(below2G->request));
  if (status < 0) {

    /* Write error message if one found */
    if ((message != NULL) && strlen(field)) {
      strncpy(message, "Bad input value",  messageLength);
    }
    goto freeStruct;
  }

  /****************************/
  /* Create the data set      */
  /****************************/
  //printf (" REQUEST: %s\n",  below2G->request);
  status = bpxwdyn(below2G->request);

  /* If error, don't try to free, decipher error code */
  if (status != 0) {
    allocDataSetInterpretError(below2G->request, status, message,
                 messageLength);
    zowelog(NULL, LOG_COMP_ALLOC, ZOWE_LOG_SEVERE,
            "Create File %s Failed:: %s\n", request->daName, message);
    goto freeStruct;
  }

  zowelog(NULL, LOG_COMP_ALLOC, ZOWE_LOG_INFO,
            "Create File:: Request: %s\n",below2G->request);

  /* If created the data set, then free/close it up */
  allocDataSetCloseString(request, below2G->request,
                             sizeof (below2G->request));
  status = bpxwdyn(below2G->request);

  /* free the structure */
freeStruct:
  release ((void (*)())bpxwdyn);
  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return status;
#endif /* __ZOWE_OS_ZOS */
}
#undef FIELD_MAX_LENGTH
#undef REQUEST_MAX_LENGTH



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

