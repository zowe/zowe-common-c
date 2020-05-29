

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

/**
 * @file radmin.c
 * @brief R_admin wrapper code.
 */


#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "zos.h"
#include "qsam.h"
#include "radmin.h"
#include "utils.h"

#ifndef METTLE
#ifdef _LP64
#pragma linkage(IRRSEQ00, OS)
#else
#pragma linkage(IRRSEQ00, OS)
#endif
#endif

#pragma enum(1)
typedef enum RadminFunctionCode_tag {
  RADMIN_FC_NA = -1,
  RADMIN_FC_ADD_USER = 0x01,
  RADMIN_FC_DEL_USER,
  RADMIN_FC_ALT_USER,
  RADMIN_FC_LST_USER,
  RADMIN_FC_RUN_COMD,
  RADMIN_FC_ADD_GROUP,
  RADMIN_FC_DEL_GROUP,
  RADMIN_FC_ALT_GROUP,
  RADMIN_FC_LST_GROUP,
  RADMIN_FC_CONNECT,
  RADMIN_FC_REMOVE,
  RADMIN_FC_ADD_GENRES,
  RADMIN_FC_DEL_GENRES,
  RADMIN_FC_ALT_GENRES,
  RADMIN_FC_LST_GENRES,
  RADMIN_FC_ADD_DS,
  RADMIN_FC_DEL_DS,
  RADMIN_FC_ALT_DS,
  RADMIN_FC_LST_DS,
  RADMIN_FC_PERMIT,
  RADMIN_FC_ALT_SETR,
  RADMIN_FC_XTR_SETR,
  RADMIN_FC_UNL_SETR,
  RADMIN_FC_XTR_PWENV,
  RADMIN_FC_XTR_USER,
  RADMIN_FC_XTR_NEXT_USER,
  RADMIN_FC_XTR_GROUP,
  RADMIN_FC_XTR_NEXT_GROUP,
  RADMIN_FC_XTR_CONNECT,
  RADMIN_FC_XTR_PPENV,
  RADMIN_FC_XTR_RESOURCE,
  RADMIN_FC_XTR_NEXT_RESOURCE,
  RADMIN_FC_XTR_RRSF
} RadminFunctionCode;
#pragma enum(reset)

#define RADMIN_WORK_AREA_SIZE 1024
typedef void RadminParmList;

#ifdef RADMIN_XMEM_MODE
#define RADMIN_RESULT_BUFFER_SUBPOOL 132
#else
#define RADMIN_RESULT_BUFFER_SUBPOOL 1
#endif

static void freeDataWithSubpool(void *data, unsigned int length, int subpool) {

  __asm(
      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
#ifdef _LP64
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         STORAGE RELEASE,LENGTH=(%0),ADDR=(%1),SP=(%2)"
#ifdef RADMIN_XMEM_MODE
      /* Since we use subpool 132, for cross-memory key 0 will be used. */
      ",CALLRKY=NO                                                             \n"
#else
      "                                                                        \n"
#endif
      "         SYSSTATE POP                                                   \n"
      :
      : "r"(length),
        "r"(data),
        "r"(subpool)
      : "r0", "r1", "r14", "r15"
  );

}

static void freeCommandOutput(RadminCommandOutput *data) {
  RadminCommandOutput *next = data;
  while (next != NULL) {
    RadminCommandOutput *curr = next;
    next = curr->next;
    freeDataWithSubpool(curr, curr->totalLength, curr->subpool);
  }
}

static void freeXTROutput(RadminXTROutput *data) {
  freeDataWithSubpool(data, data->header.totalLength, data->header.subpool);
  data = NULL;
}

/*
  linkage
  CSRTABLE has some well known slots, is IRRSEQ00 in there??

  CVT+0x220 (544) is the CSR table (CVTCSRT)

  0x18   CSR slot (BPX services)
  0x28   RACF (IRR) slot 
  0x48   GXL  XML services 
  
 */


int IRRSEQ00(
    char * __ptr32 workArea,
    int32_t * __ptr32 safReturnCodeALET, int32_t * __ptr32 safReturnCode,
    int32_t * __ptr32 racfReturnCodeALET, int32_t * __ptr32 racfReturnCode,
    int32_t * __ptr32 racfReasonCodeALET, int32_t * __ptr32 racfReasonCode,
    RadminFunctionCode * __ptr32 functionCode,
    RadminParmList * __ptr32 parmList,
    RadminUserID * __ptr32 userID,
    ACEE * __ptr32 * __ptr32 acee,
    uint8_t * __ptr32 outMessageSubpool,
    void * __ptr32 * __ptr32 outMessage)
#ifdef METTLE
__attribute__((amode31));
#else
;
#endif

#define DATA31_LENGTH 16

typedef struct IRRSEQ00Arguments_tag{
  char * __ptr32 workArea;
  int32_t * __ptr32 safReturnCodeALET;
  int32_t * __ptr32 safReturnCode;
  int32_t * __ptr32 racfReturnCodeALET;
  int32_t * __ptr32 racfReturnCode;
  int32_t * __ptr32 racfReasonCodeALET;
  int32_t * __ptr32 racfReasonCode;
  RadminFunctionCode * __ptr32 functionCode;
  RadminParmList * __ptr32 parmList;
  RadminUserID * __ptr32 userID;
  ACEE * __ptr32 * __ptr32 acee;
  uint8_t * __ptr32 outMessageSubpool;
  int  outMessage;   /* it's really a handle, but the VL bit and other issues just make this too annoying */

} IRRSEQ00Arguments;

  /*
  IRRSEQ00(
      workArea,
      &(status->alet), &(status->safRC),
      &(status->alet), &(status->racfRC),
      &(status->alet), &(status->racfRSN),
      functionCodeBox,
      parmList,
      &(callerAuthInfo->userID),
      &(callerAuthInfo->acee),
      subpoolBox,
      resultPtr
  );
...i...0.........0..... 
400280005F015F025F021F5F000F 
1007900280008F208F08E08F007F 
LA  0,X'27'
SLL 0,2
L   15,10(,0)
L   15,X'220'(,15)
L   15,X'28'(15) 
AHI 15,X'9C'
L   15,0(,15)
BASR 14,15
  */


/* The result doesn't need to be a 32-bit pointer, it just needs to be a
   31 bit chunk of memory that is being pointed at.
   */

static void *invokeRadmin(char * __ptr32 workArea,
  RadminAPIStatus *__ptr32 status,
  RadminFunctionCode functionCode,
  RadminParmList * __ptr32 parmList,
  RadminCallerAuthInfo *__ptr32 callerAuthInfo,
  uint8_t resultSubpool
) {


  /* get the immediate data that needs to have 31 bit pointers into 31 bit area and
      make some pointers that point in. */
  char *__ptr32 data31 = (char *__ptr32)safeMalloc31(DATA31_LENGTH,"IRRSEQ00Data:31");
  memset(data31,0,DATA31_LENGTH);
  RadminFunctionCode *__ptr32 functionCodeBox = (RadminFunctionCode *__ptr32)data31;
  *functionCodeBox = functionCode;
  uint8_t *__ptr32 subpoolBox = (uint8_t*)(data31+4);
  /* make the handle and the pointer be in 3 and fourth slots of DAT31 */
  *subpoolBox = resultSubpool;
 
  IRRSEQ00Arguments *__ptr32 arguments = (IRRSEQ00Arguments *__ptr32)safeMalloc31(sizeof(IRRSEQ00Arguments),"IRRSEQ00Arguments:31");
  arguments->workArea = workArea;
  arguments->safReturnCodeALET = &(status->alet);
  arguments->safReturnCode = &(status->safRC);
  arguments->racfReturnCodeALET = &(status->alet);
  arguments->racfReturnCode = &(status->racfRC);
  arguments->racfReasonCodeALET = &(status->alet);
  arguments->racfReasonCode = &(status->racfRSN);
  arguments->functionCode = functionCodeBox;
  arguments->parmList = parmList;
  arguments->userID = &(callerAuthInfo->userID);
  arguments->acee = &(callerAuthInfo->acee);
  arguments->outMessageSubpool = subpoolBox;
  arguments->outMessage = ((int)(data31+8))|0x80000000;
  uint32_t argumentsAddress = (uint32_t)arguments;


#ifdef RADMIN_XMEM_MODE
  /* When in PC-cp PSW key != TCB key, depending on the subpool R_admin will
   * allocate the result buffer either using the TCB key or key 0.
   * To copy the results R_admin uses MVCDK and the caller's PSW key.
   * If the result buffer is in the TCB key or key 0, and the PSW key is not
   * equal to that key, we get an S0C4. To fix this have to go key 0. */
  int oldKey = setKey(0);
#endif


  printf("right before assembler code arguments at 0x%x\n",argumentsAddress);
  dumpbuffer((char*)arguments,sizeof(IRRSEQ00Arguments));
  printf("status block\n");
  dumpbuffer((char*)status,sizeof(RadminAPIStatus));
  printf("Data 31 at 0x%p\n",data31);
  dumpbuffer(data31,DATA31_LENGTH);

  __asm(ASM_PREFIX
        /* still need compiled C-code addressing mode to get this address */
        " XGR  1,1   \n"
        " L    1,%0  \n"
#ifdef _LP64
	" SAM31 \n"
#endif  
        " L    15,X'10'(,0)   \n"
        " L    15,X'220'(,15) \n"    /* CSR Table */
        " L    15,X'28'(,15)  \n"    /* IRR Slot  */
        " AHI  15,X'9C'       \n"    /* Make pointer int IRR Slot */
        " L    15,0(,15)      \n"    /* Load routine pointer for IRRSEQ00 */
        " BASR 14,15          \n"    
#ifdef _LP64
	" SAM64 \n"
#endif  
        : :
        "m"(argumentsAddress)
        : "r1", "r14", "r15");

#ifdef RADMIN_XMEM_MODE
  setKey(oldKey);
#endif

  void *result = (void*)((int*)data31)[2];
  printf("right after assembler code result=0x%p\n");
  dumpbuffer((char*)arguments,sizeof(IRRSEQ00Arguments));
  printf("status block\n");
  dumpbuffer((char*)status,sizeof(RadminAPIStatus));
  printf("Data 31\n");
  dumpbuffer(data31,DATA31_LENGTH);

  safeFree31(data31,DATA31_LENGTH);
  safeFree31((char*)arguments,sizeof(IRRSEQ00Arguments));
  return result;
}

static char *__ptr32 allocateWorkArea(){
  char *__ptr32 workArea = safeMalloc31(RADMIN_WORK_AREA_SIZE,"Radmin WorkArea");
  memset(workArea,0,RADMIN_WORK_AREA_SIZE);
  return workArea;
}

static void freeWorkArea(char *__ptr32 workArea){
  safeFree31(workArea,RADMIN_WORK_AREA_SIZE);
}

static RadminAPIStatus *__ptr32 allocateAPIStatus(){
  RadminAPIStatus *__ptr32 apiStatus = (RadminAPIStatus *__ptr32)safeMalloc31(sizeof(RadminAPIStatus),"RadminAPIStatus:31");
  memset(apiStatus,0,sizeof(RadminAPIStatus));
  return apiStatus;
}

static void freeAPIStatus(RadminAPIStatus *__ptr32 apiStatus){
  safeFree31((char *__ptr32)apiStatus,sizeof(RadminAPIStatus));
}

static RadminCallerAuthInfo *__ptr32  makeCallerAuthInfo31(RadminCallerAuthInfo callerAuthInfo){
  RadminCallerAuthInfo *__ptr32 authInfo31 =
    (RadminCallerAuthInfo *__ptr32)safeMalloc31(sizeof(RadminCallerAuthInfo),"RadminCallerAuthInfo31");
  memcpy(authInfo31,&callerAuthInfo,sizeof(RadminCallerAuthInfo));
  return authInfo31;
}

static void freeCallerAuthInfo31(RadminCallerAuthInfo *__ptr32 authInfo31){
  safeFree31((char*)authInfo31,sizeof(RadminCallerAuthInfo));
}

static RadminAPIStatus *__ptr32 runRACFCommand(
    RadminCallerAuthInfo callerAuthInfo,
    RadminRACFCommand *__ptr32 command,
    RadminCommandOutput * __ptr32 * __ptr32 result,
) {

  char *__ptr32 workArea = allocateWorkArea();

  RadminAPIStatus *__ptr32 status = allocateAPIStatus();
  RadminCallerAuthInfo *__ptr32 authInfo31 = makeCallerAuthInfo31(callerAuthInfo);

  RadminUserID callerUserID = callerAuthInfo.userID;
  ACEE * __ptr32 callerACEE = callerAuthInfo.acee;

  uint8_t resultSubpool = RADMIN_RESULT_BUFFER_SUBPOOL;

  void *output = invokeRadmin(
      workArea,
      status,
      RADMIN_FC_RUN_COMD,
      (RadminParmList *__ptr32)command,
      authInfo31,
      resultSubpool
  );
  *result = (RadminCommandOutput *__ptr32)output;

  freeWorkArea(workArea);
  freeCallerAuthInfo31(authInfo31);

  return status;
}

int radminRunRACFCommand(
    RadminCallerAuthInfo callAuthInfo,
    const char *command,
    RadminCommandResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (command == NULL) {
    status->reasonCode = RSN_RADMIN_CMD_COMMAND_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  size_t commandLength = strlen(command);
  if (commandLength > RADMIN_CMD_MAX_COMMAND_LENGTH) {
    status->reasonCode = RSN_RADMIN_CMD_COMMAND_TOO_LONG;
    return RC_RADMIN_BAD_INPUT;
  }

  if (userHandler == NULL) {
    status->reasonCode = RSN_RADMIN_CMD_USER_HANDLER_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  RadminRACFCommand *__ptr32 internalCommand = 
    (RadminRACFCommand *__ptr32)safeMalloc31(sizeof(RadminRACFCommand),"RadminRACFCommand");
  internalCommand->length = (uint16_t)commandLength;
  memcpy(internalCommand->text, command, commandLength);

  int rc = RC_RADMIN_OK;
  RadminCommandOutput * __ptr32 result = NULL;
  RadminAPIStatus statusOnStack;
  RadminAPIStatus *__ptr32 apiStatus31 = runRACFCommand(callAuthInfo,
                                                        internalCommand,
                                                        &result);
  statusOnStack = *apiStatus31;

  if (result != NULL) {

    int visitRC = userHandler(statusOnStack, result, userHandlerData);
    if (visitRC != 0) {
      status->reasonCode = visitRC;
      rc = RC_RADMIN_NONZERO_USER_RC;
    }

  }

  safeFree31((char*)internalCommand,sizeof(RadminRACFCommand));

  if (apiStatus31->safRC > 0) {

    status->apiStatus = *apiStatus31;
    rc = RC_RADMIN_SYSTEM_ERROR;

  }

  if (result != NULL) {
    freeCommandOutput(result);
    result = NULL;
  }

  freeAPIStatus(apiStatus31);
  return rc;
}

static RadminFunctionCode getProfileXTRFunctionCode(RadminProfileType type,
                                                    bool isNext) {

  switch (type) {
  case RADMIN_PROFILE_TYPE_USER:
    if (isNext) {
      return RADMIN_FC_XTR_NEXT_USER;
    }
    return RADMIN_FC_XTR_USER;
  case RADMIN_PROFILE_TYPE_GROUP:
    if (isNext) {
      return RADMIN_FC_XTR_NEXT_GROUP;
    }
    return RADMIN_FC_XTR_GROUP;
  case RADMIN_PROFILE_TYPE_CONNECTION:
    return RADMIN_FC_XTR_CONNECT;
  case RADMIN_PROFILE_TYPE_GENRES:
    if (isNext) {
      return RADMIN_FC_XTR_NEXT_RESOURCE;
    }
    return RADMIN_FC_XTR_RESOURCE;
  default:
    return 0;
  }

}

static RadminAPIStatus extractProfileInternal(
    RadminCallerAuthInfo callerAuthInfo,
    RadminProfileType type,
    const char profileName[],
    size_t profileNameLength,
    const char className[],
    size_t classNameLength,
    bool isNext,
    bool *isProfileGeneric,
    RadminXTROutput **result,
) {

  char *__ptr32 workArea = allocateWorkArea();

  RadminAPIStatus *__ptr32 status = allocateAPIStatus();
  RadminCallerAuthInfo *__ptr32 authInfo31 = makeCallerAuthInfo31(callerAuthInfo);

  RadminFunctionCode functionCode = getProfileXTRFunctionCode(type, isNext);

  RadminXTRParmList *__ptr32 parmList = (RadminXTRParmList *__ptr32)safeMalloc31(sizeof(RadminXTRParmList),"RadminXTRParmList:31");
  parmList->header.profileNameLength = profileNameLength;
  parmList->header.flag = RADMIN_XTR_HDR_FLAG_BASE_SEG_ONLY;
  if (*isProfileGeneric == true) {
    parmList->header.flag |= RADMIN_XTR_HDR_FLAG_GENERIC;
  }
  memset(parmList->header.className, ' ', sizeof(parmList->header.className));
  memcpy(parmList->header.className, className, classNameLength);
  memcpy(parmList->profileName, profileName, profileNameLength);

  RadminUserID callerUserID = callerAuthInfo.userID;
  ACEE * __ptr32 callerACEE = callerAuthInfo.acee;

  uint8_t resultSubpool = RADMIN_RESULT_BUFFER_SUBPOOL;

  void *output = invokeRadmin(
      workArea,
      status,
      functionCode,
      (RadminParmList *__ptr32)parmList,
      authInfo31,
      resultSubpool
  );

  printf("output=0x%p\n",output);fflush(stdout);
  *result = output;

  freeWorkArea(workArea);
  freeCallerAuthInfo31(authInfo31);
  safeFree31((char*)parmList,sizeof(RadminXTRParmList));

  if (*result != NULL) {
    RadminXTRHeader *resultHeader = &(*result)->header;
    if (resultHeader->flag & RADMIN_XTR_HDR_FLAG_GENERIC) {
      *isProfileGeneric = true;
    }
  }

  RadminAPIStatus statusOnStack = *status;
  return statusOnStack;
}

#define IS_XTR_EODAT(apiStatus) \
  (apiStatus.safRC == 4 && apiStatus.racfRC == 4 && apiStatus.racfRSN == 4)

static size_t getMaxProfileSize(RadminProfileType type) {

  switch (type) {
  case RADMIN_PROFILE_TYPE_USER:
    return RADMIN_XTR_MAX_USER_ID_LENGTH;
  case RADMIN_PROFILE_TYPE_GROUP:
    return RADMIN_XTR_MAX_GROUP_LENGTH;
  case RADMIN_PROFILE_TYPE_CONNECTION:
    return RADMIN_XTR_MAX_CONNECTION_LENGTH;
  case RADMIN_PROFILE_TYPE_GENRES:
    return RADMIN_XTR_MAX_GENRES_LENGTH;
  default:
    return 0;
  }

}

int radminExtractProfiles(
    RadminCallerAuthInfo callAuthInfo,
    RadminProfileType type,
    const char *profileName,
    const char *className,
    size_t profilesToExtract,
    RadminProfileVisitor *profileVisitor,
    void *userData,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  RadminStatus localStatus = {0};
  const char *localProfileName = profileName ? profileName : " ";
  const char *localClassName = className ? className : "        ";

  int rc = RC_RADMIN_OK;

  do {

    size_t profileLength = strlen(localProfileName);
    if (profileLength > getMaxProfileSize(type)) {
      localStatus.reasonCode = RSN_RADMIN_XTR_PROFILE_TOO_LONG;
      rc = RC_RADMIN_BAD_INPUT;
      break;
    }

    size_t classNameLength = strlen(localClassName);
    if (classNameLength > RADMIN_XTR_MAX_CLASS_LENGTH) {
      localStatus.reasonCode = RSN_RADMIN_XTR_CLASS_TOO_LONG;
      rc = RC_RADMIN_BAD_INPUT;
      break;
    }

    if (profileVisitor == NULL) {
      localStatus.reasonCode = RSN_RADMIN_XTR_VISITOR_NULL;
      rc = RC_RADMIN_BAD_INPUT;
      break;
    }

    size_t currProfileNameLength = profileLength;
    char currProfileName[RADMIN_XTR_MAX_PROFILE_LENGTH];
    memcpy(currProfileName, localProfileName, currProfileNameLength);

    bool extractionCompleted = false;
    bool extractNext = false;
    bool isProfileGeneric = false;
    for (int profileID = 0; profileID < profilesToExtract; profileID++) {

      if (extractionCompleted) {
        break;
      }

      RadminXTROutput *result = NULL;

      RadminAPIStatus apiStatus = extractProfileInternal(callAuthInfo,
                                                         type,
                                                         currProfileName,
                                                         currProfileNameLength,
                                                         localClassName,
                                                         classNameLength,
                                                         extractNext,
                                                         &isProfileGeneric,
                                                         &result);
      localStatus.apiStatus = apiStatus;
      extractNext = true;

      if (apiStatus.safRC > 0) {
        if (!IS_XTR_EODAT(apiStatus)) {
          rc = RC_RADMIN_SYSTEM_ERROR;
        }
        extractionCompleted = true;
      }

      if (result == NULL) {
        extractionCompleted = true;
      }

      if (extractionCompleted == false) {

        int visitRC = profileVisitor(result, userData);
        if (visitRC != 0) {
          localStatus.reasonCode = visitRC;
          extractionCompleted = true;
          rc = RC_RADMIN_NONZERO_USER_RC;
        }
        currProfileNameLength =
            min(sizeof(currProfileName), result->header.profileNameLength);
        memcpy(currProfileName, result->profileName, currProfileNameLength);

      } else {

        /* check if no match has been found for the specified first profile,
         * if true, bump the limit and try the same request with
         * extractNext set to true */
        bool firstProfileNotFound = (profileID == 0) && (rc == RC_RADMIN_OK);
        if (firstProfileNotFound) {
          profilesToExtract += 1;
          extractionCompleted = false;
        }

      }

      if (result != NULL) {
        freeXTROutput(result);
        result = NULL;
      }

    }

  } while (0);

  *status = localStatus;
  return rc;
}

typedef struct BasicUserProfileVisitorData_tag {
  char eyecatcher[8];
#define BASIC_PROFILE_VISITOR_DATA_EYECATCHER "RSBPVEYE"
  size_t profilesExtracted;
  RadminBasicUserPofileInfo *data;
} BasicUserProfileVisitorData;

static int fillBasicUserProfileInfo(const RadminXTROutput *profile,
                                    void *userData) {

  const RadminXTRHeader *header = &profile->header;
  const char *profileName = profile->profileName;
  unsigned int profileNameLength = header->profileNameLength;

  BasicUserProfileVisitorData *visitorData = userData;
  RadminBasicUserPofileInfo *outInfo =
      &visitorData->data[visitorData->profilesExtracted];

  memset(outInfo, ' ', sizeof(RadminBasicUserPofileInfo));
  memcpy(outInfo->userID, profile->profileName,
         min(sizeof(outInfo->userID), profileNameLength));
  visitorData->profilesExtracted++;

  const RadminXTRSegmentDescriptor *segments =
      (RadminXTRSegmentDescriptor *)(profileName + profileNameLength);

  /* We're looking for the BASE segment. */
  const RadminXTRSegmentDescriptor *baseSegment = NULL;
  for (unsigned int segID = 0; segID < header->segmentCount; segID++) {
    const RadminXTRSegmentDescriptor *segment = &segments[segID];
    if (memcmp(segment->name, "BASE    ", sizeof(segment->name)) == 0) {
      baseSegment = segment;
      break;
    }
  }

  if (baseSegment == NULL) {
    return 0;
  }

  uint32_t firstFieldOffset = baseSegment->firstFieldOffset;

  const RadminXTRFieldDescriptor *fields =
      (RadminXTRFieldDescriptor *)((char *)header + firstFieldOffset);

  bool defaultGroupFound = false;
  bool nameFound = false;

  /* R_admin fields are not guaranteed to be returned in a certain order,
   * we have to iterate over all of them until we find ours. */
  for (unsigned int fieldID = 0; fieldID < baseSegment->fieldCount; fieldID++) {

    if (defaultGroupFound && nameFound) {
      break;
    }

    const RadminXTRFieldDescriptor *field = &fields[fieldID];
    if (!memcmp(field->name, "DFLTGRP ", sizeof(field->name))) {
      const char *fieldValue = (char *)header + field->dataOffset;
      memcpy(outInfo->defaultGroup, fieldValue,
             min(sizeof(outInfo->defaultGroup), field->dataLength));
      defaultGroupFound = true;
      continue;
    }

    if (!memcmp(field->name, "NAME    ", sizeof(field->name))) {
      const char *fieldValue = (char *)header + field->dataOffset;
      memcpy(outInfo->name, fieldValue,
          min(sizeof(outInfo->name), field->dataLength));
      nameFound = true;
      continue;
    }

  }

 return 0;
}

int radminExtractBasicUserProfileInfo(
    RadminCallerAuthInfo callerAuthInfo,
    const char *startProfile,
    size_t profilesToExtract,
    RadminBasicUserPofileInfo *result,
    size_t *profilesExtracted,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (result == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_RESULT_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (profilesExtracted == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_PROFILES_XTRED_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  BasicUserProfileVisitorData visitorData =  {
      .eyecatcher = BASIC_PROFILE_VISITOR_DATA_EYECATCHER,
      .profilesExtracted = 0,
      .data = result
  };

  int extractRC = radminExtractProfiles(callerAuthInfo,
                                        RADMIN_PROFILE_TYPE_USER,
                                        startProfile,
                                        NULL,
                                        profilesToExtract,
                                        fillBasicUserProfileInfo,
                                        &visitorData,
                                        status);
  if (extractRC != RC_RADMIN_OK) {
    return extractRC;
  }

  *profilesExtracted = visitorData.profilesExtracted;

  return RC_RADMIN_OK;
}

typedef struct BasicGenresProfileVisitorData_tag {
  char eyecatcher[8];
#define BASIC_PROFILE_VISITOR_DATA_EYECATCHER "RSBPVEYE"
  size_t profilesExtracted;
  RadminBasicGenresPofileInfo *data;
} BasicGenresProfileVisitorData;

static int fillBasicGenresProfileInfo(const RadminXTROutput *profile,
                                      void *userData) {

  const RadminXTRHeader *header = &profile->header;
  const char *profileName = profile->profileName;
  unsigned int profileNameLength = header->profileNameLength;

  BasicGenresProfileVisitorData *visitorData = userData;
  RadminBasicGenresPofileInfo *outInfo =
      &visitorData->data[visitorData->profilesExtracted];

  memset(outInfo, ' ', sizeof(RadminBasicGenresPofileInfo));
  memcpy(outInfo->profile, profile->profileName,
         min(sizeof(outInfo->profile), profileNameLength));
  visitorData->profilesExtracted++;

  const RadminXTRSegmentDescriptor *segments =
      (RadminXTRSegmentDescriptor *)(profileName + profileNameLength);

  /* We're looking for the BASE segment. */
  const RadminXTRSegmentDescriptor *baseSegment = NULL;
  for (unsigned int segID = 0; segID < header->segmentCount; segID++) {
    const RadminXTRSegmentDescriptor *segment = &segments[segID];
    if (memcmp(segment->name, "BASE    ", sizeof(segment->name)) == 0) {
      baseSegment = segment;
      break;
    }
  }

  if (baseSegment == NULL) {
    return 0;
  }

  uint32_t firstFieldOffset = baseSegment->firstFieldOffset;

  const RadminXTRFieldDescriptor *fields =
      (RadminXTRFieldDescriptor *)((char *)header + firstFieldOffset);

  bool ownerFound = false;

  /* R_admin fields are not guaranteed to be returned in a certain order,
   * we have to iterate over all of them until we find ours. */
  for (unsigned int fieldID = 0; fieldID < baseSegment->fieldCount; fieldID++) {

    if (ownerFound) {
      break;
    }

    const RadminXTRFieldDescriptor *field = &fields[fieldID];
    if (!memcmp(field->name, "OWNER   ", sizeof(field->name))) {
      const char *fieldValue = (char *)header + field->dataOffset;
      memcpy(outInfo->owner, fieldValue,
             min(sizeof(outInfo->owner), field->dataLength));
      ownerFound = true;
      continue;
    }

  }

 return 0;
}

int radminExtractBasicGenresProfileInfo(
    RadminCallerAuthInfo callerAuthInfo,
    const char *class,
    const char *startProfile,
    size_t profilesToExtract,
    RadminBasicGenresPofileInfo *result,
    size_t *profilesExtracted,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (result == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_RESULT_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (class == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_CLASS_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (profilesExtracted == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_PROFILES_XTRED_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  BasicGenresProfileVisitorData visitorData =  {
      .eyecatcher = BASIC_PROFILE_VISITOR_DATA_EYECATCHER,
      .profilesExtracted = 0,
      .data = result
  };

  int extractRC = radminExtractProfiles(callerAuthInfo,
                                        RADMIN_PROFILE_TYPE_GENRES,
                                        startProfile,
                                        class,
                                        profilesToExtract,
                                        fillBasicGenresProfileInfo,
                                        &visitorData,
                                        status);

  if (extractRC != RC_RADMIN_OK) {
    return extractRC;
  }

  *profilesExtracted = visitorData.profilesExtracted;

  return RC_RADMIN_OK;
}

typedef struct BasicGroupProfileVisitorData_tag {
  char eyecatcher[8];
#define BASIC_GROUP_VISITOR_DATA_EYECATCHER "RSBGVEYE"
  size_t profilesExtracted;
  RadminBasicGroupPofileInfo *data;
} BasicGroupProfileVisitorData;

static int fillBasicGroupProfileInfo(const RadminXTROutput *profile,
                                     void *userData) {

  const RadminXTRHeader *header = &profile->header;
  const char *profileName = profile->profileName;
  unsigned int profileNameLength = header->profileNameLength;

  BasicGroupProfileVisitorData *visitorData = userData;
  RadminBasicGroupPofileInfo *outInfo =
      &visitorData->data[visitorData->profilesExtracted];

  memset(outInfo, ' ', sizeof(RadminBasicGroupPofileInfo));
  memcpy(outInfo->group, profile->profileName,
         min(sizeof(outInfo->group), profileNameLength));
  visitorData->profilesExtracted++;

  const RadminXTRSegmentDescriptor *segments =
      (RadminXTRSegmentDescriptor *)(profileName + profileNameLength);

  /* We're looking for the BASE segment. */
  const RadminXTRSegmentDescriptor *baseSegment = NULL;
  for (unsigned int segID = 0; segID < header->segmentCount; segID++) {
    const RadminXTRSegmentDescriptor *segment = &segments[segID];
    if (memcmp(segment->name, "BASE    ", sizeof(segment->name)) == 0) {
      baseSegment = segment;
      break;
    }
  }

  if (baseSegment == NULL) {
    return 0;
  }

  uint32_t firstFieldOffset = baseSegment->firstFieldOffset;

  const RadminXTRFieldDescriptor *fields =
      (RadminXTRFieldDescriptor *)((char *)header + firstFieldOffset);

  bool ownerFound = false;
  bool superiorGroupFound = false;

  /* R_admin fields are not guaranteed to be returned in a certain order,
   * we have to iterate over all of them until we find ours. */
  for (unsigned int fieldID = 0; fieldID < baseSegment->fieldCount; fieldID++) {

    if (ownerFound && superiorGroupFound) {
      break;
    }

    const RadminXTRFieldDescriptor *field = &fields[fieldID];
    if (!memcmp(field->name, "OWNER   ", sizeof(field->name))) {
      const char *fieldValue = (char *)header + field->dataOffset;
      memcpy(outInfo->owner, fieldValue,
             min(sizeof(outInfo->owner), field->dataLength));
      ownerFound = true;
      continue;
    }
    if (!memcmp(field->name, "SUPGROUP", sizeof(field->name))) {
      const char *fieldValue = (char *)header + field->dataOffset;
      memcpy(outInfo->superiorGroup, fieldValue,
             min(sizeof(outInfo->superiorGroup), field->dataLength));
      superiorGroupFound = true;
      continue;
    }

  }

 return 0;
}

int radminExtractBasicGroupProfileInfo(
    RadminCallerAuthInfo callerAuthInfo,
    const char *startProfile,
    size_t profilesToExtract,
    RadminBasicGroupPofileInfo *result,
    size_t *profilesExtracted,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (result == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_RESULT_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (profilesExtracted == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_PROFILES_XTRED_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  BasicGroupProfileVisitorData visitorData =  {
      .eyecatcher = BASIC_GROUP_VISITOR_DATA_EYECATCHER,
      .profilesExtracted = 0,
      .data = result
  };

  int extractRC = radminExtractProfiles(callerAuthInfo,
                                        RADMIN_PROFILE_TYPE_GROUP,
                                        startProfile,
                                        NULL,
                                        profilesToExtract,
                                        fillBasicGroupProfileInfo,
                                        &visitorData,
                                        status);

  if (extractRC != RC_RADMIN_OK) {
    return extractRC;
  }

  *profilesExtracted = visitorData.profilesExtracted;

  return RC_RADMIN_OK;
}

typedef struct AccessListVisitorData_tag {
  char eyecatcher[8];
#define ACCESS_LIST_VISITOR_DATA_EYECATCHER "RSALSEYE"
  const char *profileNullTerm;
  size_t entriesFound;
  size_t entryListCapacity;
  RadminAccessListEntry *entries;
} AccessListVisitorData;

#define RC_ACCESSLIST_VISITOR_OK            0
#define RC_ACCESSLIST_VISITOR_INSUFF_SPACE  4

static void fillGenresAccessEntry(
    const RadminXTRHeader *header,
    const RadminXTRFieldDescriptor *accessFieldGroup,
    size_t groupSize,
    RadminAccessListEntry *result
) {

  memset(result, ' ', sizeof(RadminAccessListEntry));
  for (int i = 0; i < groupSize; i++) {
    const RadminXTRFieldDescriptor *field = &accessFieldGroup[i];
    if (memcmp(field->name, "ACLID   ", sizeof(field->name)) == 0) {
      char *data = (char *)header + field->dataOffset;
      memcpy(result->id, data,
             min(sizeof(result->id), field->dataLength));
      continue;
    }
    if (memcmp(field->name, "ACLACS  ", sizeof(field->name)) == 0) {
      char *data = (char *)header + field->dataOffset;
      memcpy(result->accessType, data,
             min(sizeof(result->accessType), field->dataLength));
      continue;
    }
  }

}

static int extractGenresAccessList(const RadminXTROutput *profileData,
                                   void *userData) {

  AccessListVisitorData *visitorData = userData;

  const RadminXTRHeader *header = &profileData->header;
  const char *profileName = profileData->profileName;
  unsigned int profileNameLength = header->profileNameLength;

  /* We might be called for a profile which doesn't match the specified one
   * if the initial profile name hasn't been found. Check if it's the profile
   * we need, otherwise leave. */
  if (profileNameLength != strlen(visitorData->profileNullTerm) ||
      memcmp(profileName, visitorData->profileNullTerm, profileNameLength)) {
    visitorData->entriesFound = 0;
    return RC_ACCESSLIST_VISITOR_OK;
  }

  const RadminXTRSegmentDescriptor *segments =
      (RadminXTRSegmentDescriptor *)(profileName + profileNameLength);
  for (unsigned int segID = 0; segID < header->segmentCount; segID++) {

    const RadminXTRSegmentDescriptor *segment = &segments[segID];
    if (memcmp(segment->name, "BASE    ", sizeof(segment->name))) {
      continue;
    }

    size_t firstFieldOffset = segment->firstFieldOffset;
    const RadminXTRFieldDescriptor *fields =
        (RadminXTRFieldDescriptor *)((char *)header + firstFieldOffset);

    for (unsigned int fieldID = 0; fieldID < segment->fieldCount; fieldID++) {

      const RadminXTRFieldDescriptor *field = &fields[fieldID];
      if (memcmp(field->name, "ACLCNT  ", sizeof(field->name))) {
        continue;
      }

      if (!(field->fieldType & RADMIN_XTR_FIELD_TYPE_REPEAT_FIELD_HEADER)) {
        break;
      }

      visitorData->entriesFound = field->repeatGroupCount;
      if (field->repeatGroupCount > visitorData->entryListCapacity) {
        return RC_ACCESSLIST_VISITOR_INSUFF_SPACE;
      }

      const RadminXTRFieldDescriptor *repeatGroups = field + 1;
      size_t repeatGroupSize = field->subfieldCount;
      size_t repeatGroupCount = field->repeatGroupCount;

      for (unsigned int groupID = 0; groupID < repeatGroupCount; groupID++) {

        const RadminXTRFieldDescriptor *repeatGroup =
            &repeatGroups[groupID * repeatGroupSize];
        RadminAccessListEntry *entryToFill = &visitorData->entries[groupID];
        fillGenresAccessEntry(header, repeatGroup, repeatGroupSize, entryToFill);

      }

      break;
    }

    break;
  }

  return RC_ACCESSLIST_VISITOR_OK;
}

int radminExtractGenresAccessList(
    RadminCallerAuthInfo callerAuthInfo,
    const char *class,
    const char *profile,
    RadminAccessListEntry *result,
    size_t resultCapacity,
    size_t *accessListSize,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (class == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_CLASS_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (profile == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_PROFILE_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (result == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_RESULT_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  AccessListVisitorData visitorData =  {
      .eyecatcher = ACCESS_LIST_VISITOR_DATA_EYECATCHER,
      .profileNullTerm = profile,
      .entriesFound = 0,
      .entryListCapacity = resultCapacity,
      .entries = result
  };

  int extractRC = radminExtractProfiles(callerAuthInfo,
                                        RADMIN_PROFILE_TYPE_GENRES,
                                        profile,
                                        class,
                                        1,
                                        extractGenresAccessList,
                                        &visitorData,
                                        status);
  if (extractRC != RC_RADMIN_OK) {

    if (extractRC == RC_RADMIN_NONZERO_USER_RC) {

      if (status->reasonCode == RC_ACCESSLIST_VISITOR_INSUFF_SPACE) {
        status->reasonCode = visitorData.entriesFound;
        return RC_RADMIN_INSUFF_SPACE;
      } else {
        return RC_RADMIN_NONZERO_USER_RC;
      }

    } else {
      return extractRC;
    }

  }

  *accessListSize = visitorData.entriesFound;

  return RC_RADMIN_OK;
}

static void fillGroupAccessEntry(
    const RadminXTRHeader *header,
    const RadminXTRFieldDescriptor *accessFieldGroup,
    size_t groupSize,
    RadminAccessListEntry *result
) {

  memset(result, ' ', sizeof(RadminAccessListEntry));
  for (int i = 0; i < groupSize; i++) {
    const RadminXTRFieldDescriptor *field = &accessFieldGroup[i];
    if (memcmp(field->name, "GUSERID ", sizeof(field->name)) == 0) {
      char *data = (char *)header + field->dataOffset;
      memcpy(result->id, data,
             min(sizeof(result->id), field->dataLength));
      continue;
    }
    if (memcmp(field->name, "GAUTH   ", sizeof(field->name)) == 0) {
      char *data = (char *)header + field->dataOffset;
      memcpy(result->accessType, data,
             min(sizeof(result->accessType), field->dataLength));
      continue;
    }
  }

}

static int extractGroupAccessList(const RadminXTROutput *profileData,
                                  void *userData) {

  AccessListVisitorData *visitorData = userData;

  const RadminXTRHeader *header = &profileData->header;
  const char *profileName = profileData->profileName;
  unsigned int profileNameLength = header->profileNameLength;

  /* We might be called for a profile which doesn't match the specified one
   * if the initial profile name hasn't been found. Check if it's the profile
   * we need, otherwise leave. */
  if (profileNameLength != strlen(visitorData->profileNullTerm) ||
      memcmp(profileName, visitorData->profileNullTerm, profileNameLength)) {
    visitorData->entriesFound = 0;
    return RC_ACCESSLIST_VISITOR_OK;
  }

  const RadminXTRSegmentDescriptor *segments =
      (RadminXTRSegmentDescriptor *)(profileName + profileNameLength);
  for (unsigned int segID = 0; segID < header->segmentCount; segID++) {

    const RadminXTRSegmentDescriptor *segment = &segments[segID];
    if (memcmp(segment->name, "BASE    ", sizeof(segment->name))) {
      continue;
    }

    size_t firstFieldOffset = segment->firstFieldOffset;
    const RadminXTRFieldDescriptor *fields =
        (RadminXTRFieldDescriptor *)((char *)header + firstFieldOffset);

    for (unsigned int fieldID = 0; fieldID < segment->fieldCount; fieldID++) {

      const RadminXTRFieldDescriptor *field = &fields[fieldID];
      if (memcmp(field->name, "CONNECTS", sizeof(field->name))) {
        continue;
      }

      if (!(field->fieldType & RADMIN_XTR_FIELD_TYPE_REPEAT_FIELD_HEADER)) {
        break;
      }

      visitorData->entriesFound = field->repeatGroupCount;
      if (field->repeatGroupCount > visitorData->entryListCapacity) {
        return RC_ACCESSLIST_VISITOR_INSUFF_SPACE;
      }

      const RadminXTRFieldDescriptor *repeatGroups = field + 1;
      size_t repeatGroupSize = field->subfieldCount;
      size_t repeatGroupCount = field->repeatGroupCount;

      for (unsigned int groupID = 0; groupID < repeatGroupCount; groupID++) {

        const RadminXTRFieldDescriptor *repeatGroup =
            &repeatGroups[groupID * repeatGroupSize];
        RadminAccessListEntry *entryToFill = &visitorData->entries[groupID];
        fillGroupAccessEntry(header, repeatGroup, repeatGroupSize, entryToFill);

      }

      break;
    }

    break;
  }

  return RC_ACCESSLIST_VISITOR_OK;
}

int radminExtractGroupAccessList(
    RadminCallerAuthInfo callerAuthInfo,
    const char *group,
    RadminAccessListEntry *result,
    size_t resultCapacity,
    size_t *accessListSize,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (group == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_PROFILE_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (result == NULL) {
    status->reasonCode = RSN_RADMIN_XTR_RESULT_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  AccessListVisitorData visitorData =  {
      .eyecatcher = ACCESS_LIST_VISITOR_DATA_EYECATCHER,
      .profileNullTerm = group,
      .entriesFound = 0,
      .entryListCapacity = resultCapacity,
      .entries = result
  };

  int extractRC = radminExtractProfiles(callerAuthInfo,
                                        RADMIN_PROFILE_TYPE_GROUP,
                                        group,
                                        NULL,
                                        1,
                                        extractGroupAccessList,
                                        &visitorData,
                                        status);
  if (extractRC != RC_RADMIN_OK) {

    if (extractRC == RC_RADMIN_NONZERO_USER_RC) {

      if (status->reasonCode == RC_ACCESSLIST_VISITOR_INSUFF_SPACE) {
        status->reasonCode = visitorData.entriesFound;
        return RC_RADMIN_INSUFF_SPACE;
      } else {
        return RC_RADMIN_NONZERO_USER_RC;
      }

    } else {
      return extractRC;
    }

  }

  *accessListSize = visitorData.entriesFound;

  return RC_RADMIN_OK;
}

static RadminFunctionCode mapResActionToRadminFunction(RadminResAction action) {
  switch (action) {
  case RADMIN_RES_ACTION_ADD_GENRES:
    return RADMIN_FC_ADD_GENRES;
  case RADMIN_RES_ACTION_DELETE_GENRES:
    return RADMIN_FC_DEL_GENRES;
  case RADMIN_RES_ACTION_ALTER_GENRES:
    return RADMIN_FC_ALT_GENRES;
  case RADMIN_RES_ACTION_LIST_GENRES:
    return RADMIN_FC_LST_GENRES;
  case RADMIN_RES_ACTION_ADD_DS:
    return RADMIN_FC_ADD_DS;
  case RADMIN_RES_ACTION_DELETE_DS:
    return RADMIN_FC_DEL_DS;
  case RADMIN_RES_ACTION_ALTER_DS:
    return RADMIN_FC_ALT_DS;
  case RADMIN_RES_ACTION_LIST_DS:
    return RADMIN_FC_LST_DS;
  case RADMIN_RES_ACTION_PERMIT:
  case RADMIN_RES_ACTION_PROHIBIT:
    return RADMIN_FC_PERMIT;
  default:
    return RADMIN_FC_NA;
  }
}

int radminPerformResAction(
    RadminCallerAuthInfo callerAuthInfo,
    RadminResAction action,
    RadminResParmList * __ptr32 actionParmList,
    RadminResResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (actionParmList == NULL) {
    status->reasonCode = RSN_RADMIN_RES_PARMLIST_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (userHandler == NULL) {
    status->reasonCode = RSN_RADMIN_RES_USER_HANDLER_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  RadminFunctionCode functionCode = mapResActionToRadminFunction(action);
  if (functionCode == RADMIN_FC_NA) {
    status->reasonCode = RSN_RADMIN_RES_UNKNOWN_ACTION;
    return RC_RADMIN_BAD_INPUT;
  }

  char *__ptr32 workArea = allocateWorkArea();
  RadminCallerAuthInfo *__ptr32 authInfo31 = makeCallerAuthInfo31(callerAuthInfo);
  int32_t statusALET = 0; /* primary */
  RadminAPIStatus *apiStatus = &status->apiStatus;
  RadminUserID callerUserID = callerAuthInfo.userID;
  ACEE * __ptr32 callerACEE = callerAuthInfo.acee;

  uint8_t resultSubpool = RADMIN_RESULT_BUFFER_SUBPOOL;
  RadminCommandOutput * __ptr32 result = invokeRadmin(
      workArea,
      apiStatus,
      functionCode,
      actionParmList,
      authInfo31,
      resultSubpool
  );

  freeWorkArea(workArea);
  freeCallerAuthInfo31(authInfo31);

  int rc = RC_RADMIN_OK;
  if (apiStatus->safRC != 0) {
    rc = RC_RADMIN_SYSTEM_ERROR;
  }

  if (result != NULL) {

    int handleRC = userHandler(status->apiStatus, result, userHandlerData);
    if (handleRC != 0) {
      status->reasonCode = handleRC;
      rc = RC_RADMIN_NONZERO_USER_RC;
    }

    freeCommandOutput(result);
    result = NULL;

  }

  return rc;
}

static RadminFunctionCode
mapGroupActionToRadminFunction(RadminGroupAction action)
{
  switch (action) {
  case RADMIN_GROUP_ACTION_ADD_GROUP:
    return RADMIN_FC_ADD_GROUP;
  case RADMIN_GROUP_ACTION_DELETE_GROUP:
    return RADMIN_FC_DEL_GROUP;
  case RADMIN_GROUP_ACTION_ALTER_GROUP:
    return RADMIN_FC_ALT_GROUP;
  case RADMIN_GROUP_ACTION_LIST_GROUP:
    return RADMIN_FC_LST_GROUP;
  default:
    return RADMIN_FC_NA;
  }
}

int radminPerformGroupAction(
    RadminCallerAuthInfo callerAuthInfo,
    RadminGroupAction action,
    RadminGroupParmList * __ptr32 actionParmList,
    RadminGroupResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (actionParmList == NULL) {
    status->reasonCode = RSN_RADMIN_GROUP_PARMLIST_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (userHandler == NULL) {
    status->reasonCode = RSN_RADMIN_GROUP_USER_HANDLER_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  RadminFunctionCode functionCode = mapGroupActionToRadminFunction(action);
  if (functionCode == RADMIN_FC_NA) {
    status->reasonCode = RSN_RADMIN_GROUP_UNKNOWN_ACTION;
    return RC_RADMIN_BAD_INPUT;
  }

  char *__ptr32 workArea = allocateWorkArea();
  RadminCallerAuthInfo *__ptr32 authInfo31 = makeCallerAuthInfo31(callerAuthInfo);
  int32_t statusALET = 0; /* primary */
  RadminAPIStatus *apiStatus = &status->apiStatus;
  RadminUserID callerUserID = callerAuthInfo.userID;
  ACEE * __ptr32 callerACEE = callerAuthInfo.acee;

  uint8_t resultSubpool = RADMIN_RESULT_BUFFER_SUBPOOL;
  RadminCommandOutput * __ptr32 result = invokeRadmin(
      workArea,
      apiStatus,
      functionCode,
      actionParmList,
      authInfo31,
      resultSubpool
  );

  freeWorkArea(workArea);
  freeCallerAuthInfo31(authInfo31);

  int rc = RC_RADMIN_OK;
  if (apiStatus->safRC != 0) {
    rc = RC_RADMIN_SYSTEM_ERROR;
  }

  if (result != NULL) {

    int handleRC = userHandler(status->apiStatus, result, userHandlerData);
    if (handleRC != 0) {
      status->reasonCode = handleRC;
      rc = RC_RADMIN_NONZERO_USER_RC;
    }

    freeCommandOutput(result);
    result = NULL;

  }

  return rc;
}

static RadminFunctionCode
mapConnectionActionToRadminFunction(RadminConnectionAction action)
{
  switch (action) {
  case RADMIN_CONNECTION_ACTION_ADD_CONNECTION:
    return RADMIN_FC_CONNECT;
  case RADMIN_CONNECTION_ACTION_REMOVE_CONNECTION:
    return RADMIN_FC_REMOVE;
  default:
    return RADMIN_FC_NA;
  }
}

int radminPerformConnectionAction(
    RadminCallerAuthInfo callerAuthInfo,
    RadminConnectionAction action,
    RadminConnectionParmList * __ptr32 actionParmList,
    RadminConnectionResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
) {

  if (status == NULL) {
    return RC_RADMIN_STATUS_DATA_NULL;
  }

  if (actionParmList == NULL) {
    status->reasonCode = RSN_RADMIN_CONNECTION_PARMLIST_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  if (userHandler == NULL) {
    status->reasonCode = RSN_RADMIN_CONNECTION_USER_HANDLER_NULL;
    return RC_RADMIN_BAD_INPUT;
  }

  RadminFunctionCode functionCode = mapConnectionActionToRadminFunction(action);
  if (functionCode == RADMIN_FC_NA) {
    status->reasonCode = RSN_RADMIN_CONNECTION_UNKNOWN_ACTION;
    return RC_RADMIN_BAD_INPUT;
  }

  char *__ptr32 workArea = allocateWorkArea();
  RadminCallerAuthInfo *__ptr32 authInfo31 = makeCallerAuthInfo31(callerAuthInfo);
  int32_t statusALET = 0; /* primary */
  RadminAPIStatus *apiStatus = &status->apiStatus;
  RadminUserID callerUserID = callerAuthInfo.userID;
  ACEE * __ptr32 callerACEE = callerAuthInfo.acee;

  uint8_t resultSubpool = RADMIN_RESULT_BUFFER_SUBPOOL;
  RadminCommandOutput * __ptr32 result = invokeRadmin(
      workArea,
      apiStatus,
      functionCode,
      actionParmList,
      authInfo31,
      resultSubpool
  );

  freeWorkArea(workArea);
  freeCallerAuthInfo31(authInfo31);

  int rc = RC_RADMIN_OK;
  if (apiStatus->safRC != 0) {
    rc = RC_RADMIN_SYSTEM_ERROR;
  }

  if (result != NULL) {

    int handleRC = userHandler(status->apiStatus, result, userHandlerData);
    if (handleRC != 0) {
      status->reasonCode = handleRC;
      rc = RC_RADMIN_NONZERO_USER_RC;
    }

    freeCommandOutput(result);
    result = NULL;

  }

  return rc;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

