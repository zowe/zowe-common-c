
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


#ifdef METTLE 
#error Metal C not supported
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <iconv.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
#include "bpxnet.h"
#include "collections.h"
#include "unixfile.h"
#include "socketmgmt.h"
#include "le.h"
#include "logging.h"
#include "scheduling.h"
#include "http.h"
#include "json.h"

#include "dynalloc.h"

/**
 
  Compiling and running this test

  64 Bit Build
  
  xlclang -q64 -I ../h -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 -DUSE_ZOWE_TLS=1 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -o datasetCreate datasetCreate.c ../c/json.c ../c/charsets.c ../c/dynalloc.c ../c/zosfile.c  ../c/xlate.c ../c/timeutls.c ../c/utils.c ../c/alloc.c ../c/logging.c ../c/collections.c ../c/le.c ../c/recovery.c ../c/zos.c ../c/scheduling.c

  31 Bit Build

  xlc -I ../h -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 -DUSE_ZOWE_TLS=1 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -o datasetCreate31 datasetCreate.c ../c/json.c ../c/charsets.c ../c/dynalloc.c ../c/zosfile.c  ../c/xlate.c ../c/timeutls.c ../c/utils.c ../c/alloc.c ../c/logging.c ../c/collections.c ../c/le.c ../c/recovery.c ../c/zos.c ../c/scheduling.c

  Running 

  ./datasetCreate "//'MY.DS64.NAME'" <createParms.json>

  ./datasetCreate31 "//'MY.DS31.NAME'" <createParms.json>
  */

#define DSPATH_PREFIX   "//\'"
#define DSPATH_SUFFIX   "\'"

#define ERROR_DECODING_DATASET            -2
#define ERROR_CLOSING_DATASET             -3
#define ERROR_OPENING_DATASET             -4
#define ERROR_ALLOCATING_DATASET          -5
#define ERROR_DEALLOCATING_DATASET        -6
#define ERROR_UNDEFINED_LENGTH_DATASET    -7
#define ERROR_BAD_DATASET_NAME            -8
#define ERROR_INVALID_DATASET_NAME        -9
#define ERROR_INCORRECT_DATASET_TYPE      -10
#define ERROR_DATASET_NOT_EXIST           -11
#define ERROR_MEMBER_ALREADY_EXISTS       -12
#define ERROR_DATASET_ALREADY_EXIST       -13
#define ERROR_DATASET_OR_MEMBER_NOT_EXIST -14
#define ERROR_VSAM_DATASET_DETECTED       -15
#define ERROR_DELETING_DATASET_OR_MEMBER  -16
#define ERROR_INVALID_JSON_BODY           -17
#define ERROR_COPY_NOT_SUPPORTED          -18
#define ERROR_COPYING_DATASET             -19

#define INDEXED_DSCB      96
#define LEN_THREE_BYTES   3
#define LEN_ONE_BYTE      1
#define VOLSER_SIZE       6
#define CLASS_WRITER_SIZE 8
#define TOTAL_TEXT_UNITS  23

#define IS_DAMEMBER_EMPTY($member) \
  (!memcmp(&($member), &(DynallocMemberName){"        "}, sizeof($member)))

static int bytesPerTrack=56664;
static int tracksPerCylinder=15;
static int bytesPerCylinder=849960;


typedef struct DatasetName_tag {
  char value[44]; /* space-padded */
} DatasetName;

typedef struct DatasetMemberName_tag {
  char value[8]; /* space-padded */
} DatasetMemberName;

typedef struct DDName_tag {
  char value[8]; /* space-padded */
} DDName;

typedef struct Volser_tag {
  char value[6]; /* space-padded */
} Volser;


static void extractDatasetAndMemberName(const char *datasetPath,
                                        DatasetName *dsn,
                                        DatasetMemberName *memberName) {

  memset(&dsn->value, ' ', sizeof(dsn->value));
  memset(&memberName->value, ' ', sizeof(memberName->value));

  size_t pathLength = strlen(datasetPath);

  const char *dsnStart = datasetPath + strlen(DSPATH_PREFIX);
  const char *leftParen = strchr(datasetPath, '(');

  if (leftParen) {
    memcpy(dsn->value, dsnStart, leftParen - dsnStart);
    const char *rightParen = strchr(datasetPath, ')');
    memcpy(memberName->value, leftParen + 1, rightParen - leftParen - 1);
  } else {
    memcpy(dsn->value, dsnStart,
           pathLength - strlen(DSPATH_PREFIX""DSPATH_SUFFIX));
  }

  for (int i = 0; i < sizeof(dsn->value); i++) {
    dsn->value[i] = toupper(dsn->value[i]);
  }

  for (int i = 0; i < sizeof(memberName->value); i++) {
    memberName->value[i] = toupper(memberName->value[i]);
  }

}

/* Returns a quantity of tracks or cylinders for dynalloc in case the user asked for bytes */
/* Yes, these are approximations but if people really want exact numbers they should use cyl & trk */
static int getDSSizeValueFromType(int quantity, char *spaceType) {
  if (!strcmp(spaceType, "CYL")) {
    return quantity;
  } else if (!strcmp(spaceType, "TRK")) {
    return quantity;
  } else if (!strcmp(spaceType, "BYTE")) {
    return quantity / bytesPerTrack;
  } else if (!strcmp(spaceType, "KB")) {
    return (quantity*1024) / bytesPerTrack;
  } else if (!strcmp(spaceType, "MB")) {
    return (quantity*1048576) / bytesPerCylinder;
  }
  return quantity;
}

static int setDatasetAttributesForCreation(JsonObject *object, int *configsCount, TextUnit **inputTextUnit) {
  JsonProperty *currentProp = jsonObjectGetFirstProperty(object);
  Json *value = NULL;
  int parmDefn = DALDSORG_NULL;
  int type = TEXT_UNIT_NULL;
  int rc = 0;

  parmDefn = DISP_NEW; //ONLY one for create
  rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALSTATS, configsCount, inputTextUnit);


  // most parameters below explained here https://www.ibm.com/docs/en/zos/2.1.0?topic=dfsms-zos-using-data-sets
  // or here https://www.ibm.com/docs/en/zos/2.1.0?topic=function-non-jcl-dynamic-allocation-functions
  // or here https://www.ibm.com/docs/en/zos/2.1.0?topic=function-dsname-allocation-text-units
  
  while(currentProp != NULL){
    value = jsonPropertyGetValue(currentProp);
    char *propString = jsonPropertyGetKey(currentProp);

    if(propString != NULL){
      errno = 0;
      if (!strcmp(propString, "dsorg")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          parmDefn = (!strcmp(valueString, "PS")) ? DALDSORG_PS : DALDSORG_PO;
          rc = setTextUnit(TEXT_UNIT_INT16, 0, NULL, parmDefn, DALDSORG, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "blksz")) {
        // https://www.ibm.com/docs/en/zos/2.1.0?topic=statement-blksize-parameter
        int64_t valueInt = jsonAsInt64(value);
        if(valueInt != 0){
          if (valueInt <= 0x7FF8 && valueInt >= 0) { //<-- If DASD, if tape, it can be 80000000
            type = TEXT_UNIT_INT16;
          } else if (valueInt <= 0x80000000){
            type = TEXT_UNIT_LONGINT;
          }
          if(type != TEXT_UNIT_NULL) {
            rc = setTextUnit(type, 0, NULL, valueInt, DALBLKSZ, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "lrecl")) {
        int valueInt = jsonAsNumber(value);
        if (valueInt == 0x8000 || (valueInt <= 0x7FF8 && valueInt >= 0)) {
          rc = setTextUnit(TEXT_UNIT_INT16, 0, NULL, valueInt, DALLRECL, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "volser")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= VOLSER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, VOLSER_SIZE, &(valueString)[0], 0, DALVLSER, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "recfm")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          int setRECFM = 0;
          if (indexOf(valueString, valueStrLen, 'A', 0) != -1){
            setRECFM = setRECFM | DALRECFM_A;
          }
          if (indexOf(valueString, valueStrLen, 'B', 0) != -1){
            setRECFM = setRECFM | DALRECFM_B;
          }
          if (indexOf(valueString, valueStrLen, 'V', 0) != -1){
            setRECFM = setRECFM | DALRECFM_V;
          }
          if (indexOf(valueString, valueStrLen, 'F', 0) != -1){
            setRECFM = setRECFM | DALRECFM_F;
          }
          if (indexOf(valueString, valueStrLen, 'U', 0) != -1){
            setRECFM = setRECFM | DALRECFM_U;
          }
          rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, setRECFM, DALRECFM, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "blkln")
                && !jsonObjectHasKey(object, "space")) { //mutually exclusive with daltrk, dalcyl
        // https://www.ibm.com/docs/en/zos/2.1.0?topic=units-block-length-specification-key-0009
        int valueInt = jsonAsNumber(value);
        if (valueInt <= 0xFFFF && valueInt >= 0){
          rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, valueInt, DALBLKLN, configsCount, inputTextUnit);
        }
        if (jsonObjectHasKey(object, "prime")) { //in tracks for blkln
          int primeSize = jsonObjectGetNumber(object, "prime");
          if (primeSize <= 0xFFFFFF && primeSize >= 0) {
            rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, primeSize, DALPRIME, configsCount, inputTextUnit);
          }
        }
        if (jsonObjectHasKey(object, "secnd")) { //in tracks for blkln
          int secondarySize = jsonObjectGetNumber(object, "secnd");
          if (secondarySize <= 0xFFFFFF && secondarySize >= 0) {
            rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, secondarySize, DALSECND, configsCount, inputTextUnit);
          }
        }
      } else if (!strcmp(propString, "ndisp")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          if (!strcmp(valueString, "KEEP")){
            parmDefn = DISP_KEEP;
          } else {
            parmDefn = DISP_CATLG;
          }
          rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALNDISP, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "strcls")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= CLASS_WRITER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, CLASS_WRITER_SIZE, &(valueString)[0], 0, DALSTCL, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "mngcls")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= CLASS_WRITER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, CLASS_WRITER_SIZE, &(valueString)[0], 0, DALMGCL, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "datacls")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= CLASS_WRITER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, CLASS_WRITER_SIZE, &(valueString)[0], 0, DALDACL, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "space")) {
        char *spaceType = jsonAsString(value);
        if (spaceType != NULL) {
          if (!strcmp(spaceType, "CYL")) {
            parmDefn = DALCYL;
          } else if (!strcmp(spaceType, "TRK")) {
            // https://www.ibm.com/docs/en/zos/2.1.0?topic=units-track-space-type-trk-specification-key-0007
            parmDefn = DALTRK;
          } else if (!strcmp(spaceType, "BYTE")) {
            parmDefn = DALTRK;
          } else if (!strcmp(spaceType, "KB")) {
            parmDefn = DALTRK;
          } else if (!strcmp(spaceType, "MB")) {
            parmDefn = DALCYL;
          }
          if(parmDefn != DALDSORG_NULL) {
            rc = setTextUnit(TEXT_UNIT_BOOLEAN, 0, NULL, 0, parmDefn, configsCount, inputTextUnit);
          }
          if (jsonObjectHasKey(object, "prime")) { //in tracks for blkln
            int primeSize = jsonObjectGetNumber(object, "prime");
            if (primeSize <= 0xFFFFFF && primeSize >= 0) {
              rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, getDSSizeValueFromType(primeSize, spaceType), DALPRIME, configsCount, inputTextUnit);
            }
          }
          if (jsonObjectHasKey(object, "secnd")) { //in tracks for blkln
            int secondarySize = jsonObjectGetNumber(object, "secnd");
            if (secondarySize <= 0xFFFFFF && secondarySize >= 0) {
              rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, getDSSizeValueFromType(secondarySize, spaceType), DALSECND, configsCount, inputTextUnit);
            }
          }
        }
      } else if(!strcmp(propString, "dir")) {
        int valueInt = jsonAsNumber(value);
        if (valueInt <= 0xFFFFFF && valueInt >= 0) {
          rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, valueInt, DALDIR, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "avgr")) {
        // https://www.ibm.com/docs/en/zos/2.1.0?topic=statement-avgrec-parameter
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          if (!strcmp(valueString, "M")) {
            parmDefn = DALDSORG_MREC;
          } else if (!strcmp(valueString, "K")) {
            parmDefn = DALDSORG_KREC;
          } else if (!strcmp(valueString, "U")) {
            parmDefn = DALDSORG_UREC;
          }
          if(parmDefn != DALDSORG_NULL) {
            rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALAVGR, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "dsnt")) {
        // https://www.ibm.com/docs/en/zos/2.3.0?topic=jcl-allocating-system-managed-data-sets
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          if (!strcmp(valueString, "PDSE")) {
            parmDefn = DALDSORG_PDSE;
          } else if (!strcmp(valueString, "PDS")) {
            parmDefn = DALDSORG_PDS;
          } else if (!strcmp(valueString, "HFS")) {
            parmDefn = DALDSORG_HFS;
          } else if (!strcmp(valueString, "EXTREQ")) {
            parmDefn = DALDSORG_EXTREQ;
          } else if (!strcmp(valueString, "EXTPREF")) {
            parmDefn = DALDSORG_EXTPREF;
          } else if (!strcmp(valueString, "BASIC")) {
            parmDefn = DALDSORG_BASIC;
          } else if (!strcmp(valueString, "LARGE")) {
            parmDefn = DALDSORG_LARGE;
          }
          if(parmDefn != DALDSORG_NULL) {
            rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALDSNT, configsCount, inputTextUnit);
          }
        }
      }
      
    }
    if(rc == -1) {
      break;
    }
    currentProp = jsonObjectGetNextProperty(currentProp);
    parmDefn = DALDSORG_NULL;
    type = TEXT_UNIT_NULL;
  }
  return rc;
}

static int createDataset(char* absolutePath, Json *attributesJSON, int* reasonCode) {
  #ifdef __ZOWE_OS_ZOS
  DatasetName datasetName;
  DatasetMemberName memberName;
  extractDatasetAndMemberName(absolutePath, &datasetName, &memberName);

  DynallocDatasetName daDatasetName;
  DynallocMemberName daMemberName;
  memcpy(daDatasetName.name, datasetName.value, sizeof(daDatasetName.name));
  memcpy(daMemberName.name, memberName.value, sizeof(daMemberName.name));

  DynallocDDName daDDName = {.name = "????????"};

  int daRC = RC_DYNALLOC_OK, daSysReturnCode = 0, daSysReasonCode = 0;

  bool isMemberEmpty = IS_DAMEMBER_EMPTY(daMemberName);

  if(!isMemberEmpty){
    printf("this test does not make dataset members yet\n");
    return 12;
  }

  int configsCount = 0;
  char ddNameBuffer[DD_NAME_LEN+1] = "MVD00000";
  TextUnit *inputTextUnit[TOTAL_TEXT_UNITS] = {NULL};
  printf("DDNAME buffer 0x%p '%s'\n",ddNameBuffer,ddNameBuffer);

  ShortLivedHeap *slh = makeShortLivedHeap(0x10000,0x10);
  char errorBuffer[2048];

  int returnCode = 0;
  if (jsonIsObject(attributesJSON)){
    JsonObject * jsonObject = jsonAsObject(attributesJSON);
    returnCode = setTextUnit(TEXT_UNIT_STRING, DATASET_NAME_LEN, &datasetName.value[0], 0, DALDSNAM, &configsCount, inputTextUnit);
    if(returnCode == 0) {
      returnCode = setTextUnit(TEXT_UNIT_STRING, DD_NAME_LEN, ddNameBuffer, 0, DALDDNAM, &configsCount, inputTextUnit);
    }
    if(returnCode == 0) {
      returnCode = setDatasetAttributesForCreation(jsonObject, &configsCount, inputTextUnit);
    }
  }

  if (returnCode == 0) {
    returnCode = dynallocNewDataset(inputTextUnit, configsCount, reasonCode);
    printf("dND ret=%d reason %d 0x%x\n",returnCode, *reasonCode, *reasonCode);
    int ddNumber = 1;
    while (*reasonCode==0x4100000 && ddNumber < 100000) {
      sprintf(ddNameBuffer, "MVD%05d", ddNumber);
      int ddconfig = 1;
      setTextUnit(TEXT_UNIT_STRING, DD_NAME_LEN, ddNameBuffer, 0, DALDDNAM, &ddconfig, inputTextUnit);
      returnCode = dynallocNewDataset(inputTextUnit, configsCount, reasonCode);
      ddNumber++;
    }
  }

  if (returnCode) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING,
            "error: ds alloc dsn=\'%44.44s\' dd=\'%8.8s\', sysRC=%d, sysRSN=0x%08X\n",
            daDatasetName.name, ddNameBuffer, returnCode, *reasonCode);
    printf("Unable to allocate a DD for Creating New Dataset\n");
    SLHFree(slh);
    return ERROR_ALLOCATING_DATASET;
  }

  memcpy(daDDName.name, ddNameBuffer, DD_NAME_LEN);
  daRC = dynallocUnallocDatasetByDDName(&daDDName, DYNALLOC_UNALLOC_FLAG_NONE, &returnCode, reasonCode);
  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING,
            "error: ds unalloc dsn=\'%44.44s\' dd=\'%8.8s\', rc=%d sysRC=%d, sysRSN=0x%08X\n",
            daDatasetName.name, daDDName.name, daRC, returnCode, *reasonCode);
    printf("Unable to deallocate DDNAME");
    SLHFree(slh);
    return ERROR_DEALLOCATING_DATASET;
  }
  SLHFree(slh);
  return 0;
  #endif
}


int main(int argc, char **argv){
  LoggingContext *loggingContext = makeLoggingContext();

#ifdef __ZOWE_OS_WINDOWS
  int stdoutFD = _fileno(stdout);
#else
  int stdoutFD = fileno(stdout);
#endif

  if (argc < 3){
    printf("please supply a dataset name and attributes filename\n");
  }
  char *datasetName = argv[1];
  char *attributesFilename = argv[2];
  int errorBufferSize = 1024;
  char *errorBuffer = safeMalloc(errorBufferSize,"ErrorBuffer");
  memset(errorBuffer,0,errorBufferSize);
  
  ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);
  
  Json *attributes = jsonParseFile2(slh,attributesFilename,errorBuffer,errorBufferSize);
  if (attributes == NULL){
    printf("Json parse fail in %s, with error '%s'\n",attributesFilename,errorBuffer);
    return 12;
  } else{
    printf("creation attributes parsed as:\n");
    jsonPrinter *p = makeJsonPrinter(stdoutFD);
    jsonEnablePrettyPrint(p);
    jsonPrint(p,attributes);
    int reasonCode = 0;
    int status = createDataset(datasetName, attributes, &reasonCode);
    printf("create status = %d reason=%d\n",status, reasonCode);
  }


  return 0;
}
