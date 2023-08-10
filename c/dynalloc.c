

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

TextUnit *createInt24TextUnit(int key, int value) {
  return createSimpleTextUnit2(key, (char*)&value + 1, INT24_SIZE);
}

TextUnit *createLongIntTextUnit(int key, long long value) {
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
      (SVC99RequestBlock * __ptr32)INT2PTR32((int)&below2G->plist | 0x80000000);
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

int setTextUnit(int type, int size, char* stringValue, int numValue, int key, 
                int *index, TextUnit **inputTextUnit) {
  int rc = -1;
  switch (type) {
    case TEXT_UNIT_STRING:
      inputTextUnit[*index] = createSimpleTextUnit2(key, stringValue, size);
      break;
    case TEXT_UNIT_CHAR:
      inputTextUnit[*index] = createCharTextUnit(key, numValue);
      break;
    case TEXT_UNIT_INT16:
      inputTextUnit[*index] = createInt16TextUnit(key, numValue);
      break;
    case TEXT_UNIT_INT24:
      inputTextUnit[*index] = createInt24TextUnit(key, numValue);
      break;
    case TEXT_UNIT_LONGINT:
      inputTextUnit[*index] = createLongIntTextUnit(key, (long long)numValue);
      break;
    case TEXT_UNIT_BOOLEAN:
      inputTextUnit[*index] = createSimpleTextUnit2(key, NULL, 0);
      break;
  }
  if (inputTextUnit[*index] == NULL) {
    return rc;
  } else {
    (*index)++;
    return 0;
  }
}

typedef TextUnit *__ptr32 *__ptr32 TextUnitPtrArray;

int dynallocNewDataset(TextUnit **inputTextUnit, int inputTextUnitCount, int *reasonCode) {
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DynallocParms parms;
      TextUnitPtrArray textUnits;
    )
  );

  below2G->textUnits = (TextUnitPtrArray)safeMalloc31(sizeof(TextUnit*__ptr32) * inputTextUnitCount, "Text units array");
  if(below2G->textUnits == NULL) {
    return -1;
  }
  DynallocParms *parms = &below2G->parms;
  dynallocParmsInit(parms);

  dynallocParmsSetTextUnits(parms, (TextUnitPtrArray)below2G->textUnits, inputTextUnitCount);

  int rc;

  do {
    rc = 0;
    for (int i = 0; i < inputTextUnitCount; i++) { 
      below2G->textUnits[i] = (TextUnit *__ptr32)inputTextUnit[i];
      if (below2G->textUnits[i] == NULL) {
        rc = -1;
        break;
      }       
    }
    if (rc == -1) {
      break;
    }
    turn_on_HOB(below2G->textUnits[inputTextUnitCount - 1]);
    rc = invokeDynalloc(parms);
    *reasonCode = dynallocParmsGetInfoCode(parms) +
        (dynallocParmsGetErrorCode(parms) << 16);
  } while (0);
  freeTextUnitArray((TextUnit * __ptr32 *)below2G->textUnits, inputTextUnitCount);
  safeFree31((char*)below2G->textUnits, sizeof(TextUnit*) * inputTextUnitCount);
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
      (SVC99RequestBlock * __ptr32)INT2PTR32((int)&below2G->requestBlock | 0x80000000);
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
      (SVC99RequestBlock * __ptr32)INT2PTR32((int)&below2G->requestBlock | 0x80000000);
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
      (SVC99RequestBlock * __ptr32)INT2PTR32((int)&below2G->requestBlock | 0x80000000);
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


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

