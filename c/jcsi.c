

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

/*
  MVS Catalog search interface. This has no equivalent in Unixland, do not attempt to port.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "logging.h"
#include "csi.h"
#include "jcsi.h"

/* 
   c89 -o csi -Wl,ac=1 csitest.c                       

   c89 -DCSITEST=1 "-Wc,langlvl(extc99),gonum,goff,hgpr,roconst,ASM,asmlib('SYS1.MACLIB')" "-Wl,ac=1" -I ../h -o jcsi jcsi.c ../c/utils.c ../c/alloc.c 
  
   extattr +a csitest                            
  
   csi -filter <dsn_filter_spec> 

*/

typedef void* __ptr32 CsiFn;
static CsiFn csiFn = NULL;

int loadCsi() {
  int entryPoint = 0;
  int status = 0;

  if (csiFn != NULL) {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG2, "IGGCSI00 already loaded at 0x%p\n", csiFn);
    return 0;
  }

  __asm(ASM_PREFIX
        " LOAD EP=IGGCSI00 \n"
        " ST 15,%0 \n"
        " ST 0,%1 \n"
        :"=m"(status),"=m"(entryPoint)
        : :"r0","r1","r15");

  if (status == 0) {
    csiFn = (CsiFn)INT2PTR(entryPoint);
  }
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG2, "IGGCSI00 0x%p LOAD status = 0x%x\n", csiFn, status);
  return status;
}

static int callCsi(CsiFn *csiFn, void *__ptr32 paramList, char* __ptr32 saveArea) {
  int returnCode = 0;

  __asm(
      ASM_PREFIX
#ifdef __XPLINK__
      " L 13,%[saveArea] \n"
#endif
#ifdef _LP64
      " SAM31 \n"
      " SYSSTATE AMODE64=NO \n"
#endif
      " LR 1,%[paramList] \n"
      " CALL (%[csiFn]) \n"
#ifdef _LP64
      " SAM64 \n"
      " SYSSTATE AMODE64=YES \n"
#endif
      " ST 15,%[returnCode] \n"
      : [returnCode] "=m"(returnCode)
      : [csiFn] "r"(csiFn),
        [paramList] "r"(paramList),
        [saveArea] "m"(saveArea)
      : "r0", "r1",
#ifdef _LP64 
        "r13",
#endif
        "r15");

  return returnCode;
}

char * __ptr32 csi(csi_parmblock* __ptr32 csi_parms, int *workAreaSizeInOut) {
  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;
  int entryPoint = 0;
  char * __ptr32 workArea = NULL;
  int * __ptr32 reasonCodePtr = NULL;
  int workAreaSize = *workAreaSizeInOut > 2048 ? *workAreaSizeInOut : WORK_AREA_SIZE;

  ALLOC_STRUCT31(
    STRUCT31_NAME(paramList),
    STRUCT31_FIELDS(
      int * __ptr32 reasonCodePtr;
      csi_parmblock * __ptr32 paramBlock;
      char * __ptr32 workArea;
      char saveArea[72];
    )
  );

  do {
    if (!paramList) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "could not allocate memory for CSI paramList\n");
      break;
    }

    if (!csiFn) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "IGGCSI00 not loaded\n");
      break;
    }

    workArea = (char* __ptr32)safeMalloc31(workAreaSize, "CSI Work Area Size");
    if (!workArea) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "could not allocate memory for CSI workArea\n");
      break;
    }
    *((int*)workArea) = workAreaSize;

    reasonCodePtr = (int *__ptr32)safeMalloc31(sizeof(int), "CSI Reason Code");
    if (!reasonCodePtr) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "could not allocate memory for CSI reason code\n");
      break;
    }

    paramList->workArea = workArea;
    paramList->reasonCodePtr = reasonCodePtr;
    paramList->paramBlock = csi_parms;

    returnCode = callCsi(csiFn, paramList, paramList->saveArea);
    reasonCode = *reasonCodePtr;

    if (returnCode != 0) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "CSI failed ret=%d, reason=0x%x\n", returnCode, reasonCode);
      safeFree31((char*)workArea, workAreaSize);
      workArea = NULL;
      break;
    }
    *workAreaSizeInOut = workAreaSize;
  } while(0);

  if (reasonCodePtr) {
    safeFree31((char*)reasonCodePtr, sizeof(*reasonCodePtr));
  }
  if (paramList) {
    FREE_STRUCT31(STRUCT31_NAME(paramList));
  }
 
  return workArea;
}

/*
  HERE

  need to show entry types and put show field values
  Catalog and alias entries might be messed up
  make buffer smaller to show of resumption feature

  Catalog root set
 */

int pseudoLS(char *dsn, int fieldCount, char **fieldNames){
  csi_parmblock* csi_parms = NULL;
  int dsnLen = strlen(dsn);
  char *workArea = NULL;

  if (dsnLen == 0){
    return DSN_ZERO_LENGTH_DSN;
  }

  csi_parms = (csi_parmblock*)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");

  /* prep parms */
  memset(csi_parms,' ',sizeof(csi_parmblock));
  strncpy(csi_parms->filter_spec,dsn,dsnLen); /* do not copy null terminator */
  csi_parms->num_fields = fieldCount;
  int i;
  for (i=0; i<fieldCount; i++){
    strncpy(csi_parms->fields+(i*8),fieldNames[i],8);
  }
  int workAreaSize = WORK_AREA_SIZE;
  workArea = csi(csi_parms,&workAreaSize);
  if (workArea){
    int result = 0;
    char volser[7];
    int workAreaUsedLength = ((workArea[9]&0xff)<<16)+((workArea[10]&0xff)<<8)+(workArea[11]&0xff);
    int numberOfFields = *((unsigned short *)workArea);
    if (workAreaUsedLength >= 0x64) { /* header + one catalog + one dataset entry not including data */

      char *entryPointer = workArea+14;
      char *endPointer = workArea+workAreaUsedLength;
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "work area length used %d\n",workAreaUsedLength);
      dumpbuffer(workArea,workAreaUsedLength);

      while (entryPointer < endPointer){
        EntryData *entry = (EntryData*)entryPointer;

        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "entry name %44.44s\n",entry->name);
        dumpbuffer(entryPointer,0x60);        
        if (entry->flags & CSI_ENTRY_ERROR){
          zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "entry has error\n");
          entryPointer += sizeof(EntryData);
        } else{
          switch (entry->type){
          case '0':
            {
              CSICatalogData *catalogData = (CSICatalogData*)(entry+14);
              zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "catalog name %44.44s\n",catalogData->name);
              entryPointer += sizeof(CSICatalogData);     
            }
            break;
          default:
            {
              int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
              zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "entry field data length = 0x%x fieldCount=%d\n",fieldDataLength,fieldCount);
              char *fieldData = entryPointer+sizeof(EntryData);
              unsigned short *fieldLengthArray = ((unsigned short *)(entryPointer+sizeof(EntryData)));
              if (entry->type == 'X'){
                zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "entry is catalog alias\n");
              } else if (entry->type == 'U'){
                zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "entry is user catalog connector\n");
              } else{
                char *fieldValueStart = entryPointer+sizeof(EntryData)+fieldCount*sizeof(short);
                for (i=0; i<fieldCount; i++){
                  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "field %d: %8.8s, has length %d\n",i,fieldNames[i]);
                  if (fieldLengthArray[i]){
                    dumpbuffer(fieldValueStart,fieldLengthArray[i]);
                  }
                  fieldValueStart += fieldLengthArray[i];
                }
                /* zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "volser = %6.6s\n",fieldData); */
              }
              int advance = sizeof(EntryData)+fieldDataLength-4; /* -4 for the fact that the length is 4 from end of EntryData */
              entryPointer += advance;
            }
            break;
          }
        }
      }
    } else{
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "no entries, no look up failure either\n");
    }
    safeFree31((char*)workArea,workAreaSize);
    return result;
  } else{
    return DSN_CSI_FAILURE;
  }
}

/* 
    Fields:

    DATACLAS  - SMS Data Class
    DSCRDT2   - Creation Date
    DSEXDT2   - Expiration Date
    GDGALTDT  - Alteration Date
    LRECL     - Logical Record Length
    MGMTCLAS  - SMS Management Class
    NAME      - Name of Associated Entry (Aliases only)
    NOEXTINT  - Number of Extents
    NVSMATTR  - Non VSAM Attribute
    NOTRKAU   - Number Tracks per Allocation Unit
    OWNERID   - Owner User ID 
    PRIMSPAC  - Primary Space 
    STORCLAS  - SMS Storage Class
    UDATASIZ  - User Data Size
    VOLSER    - Volumne Name
    VSAMTYPE  - VSAM details (see doc)
 */

static char *entriesFields[] ={ "NAME    ", "TYPE    "};
static char *myFields[] ={ "NAME    ", "LRECL   ", "TYPE    ","VOLSER  ","VOLFLG  "};

EntryDataSet *returnEntries(char *dsn, char *typesAllowed, int typesCount, int workAreaSize, char **fields, int fieldCount, char *resumeName, char *resumeCatalogName, csi_parmblock * __ptr32 returnParms){
  csi_parmblock* __ptr32 csi_parms = returnParms;
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "csi query for '%s'\n", dsn);
  int dsnLen = strlen(dsn);
  char *workArea = NULL;

  if (dsnLen == 0){
    return NULL;
  }

  workAreaSize = (workAreaSize > 0 ? workAreaSize : WORK_AREA_SIZE);
  
  EntryDataSet *entrySet;
  entrySet = (EntryDataSet*)safeMalloc(sizeof(EntryDataSet),"Entry Data Set");
  entrySet->length = 0;
  entrySet->size = 0;

  /* prep parms */
  memset(csi_parms,' ',sizeof(csi_parmblock));
  strncpy(csi_parms->filter_spec,dsn,dsnLen); /* do not copy null terminator */
  csi_parms->num_fields = fieldCount;
  int i;
  for (i=0; i<fieldCount; i++){
    strncpy(csi_parms->fields+(i*8),fields[i],8);
  }
  if (resumeName != NULL && resumeCatalogName != NULL) {
    int resumeNameLen = strlen(resumeName);
    if (resumeNameLen > 44) resumeNameLen = 44;
    int resumeCatalogNameLen = strlen(resumeCatalogName);
    if (resumeCatalogNameLen > 44) resumeCatalogNameLen = 44;
    strncpy(csi_parms->resume_name, resumeName, resumeNameLen);
    strncpy(csi_parms->catalog_name, resumeCatalogName, resumeCatalogNameLen);
    csi_parms->is_resume = 'Y';
  }
  workArea = csi(csi_parms,&workAreaSize);
  if (workArea){
    int result = 0;
    char volser[7];
    int workAreaUsedLength = ((workArea[9]&0xff)<<16)+((workArea[10]&0xff)<<8)+(workArea[11]&0xff);
    int numberOfFields = *((unsigned short *)workArea);

    if (workAreaUsedLength >= 0x64){ /* header + one catalog + one dataset entry not including data */

      char *entryPointer = workArea+14;
      char *endPointer = workArea+workAreaUsedLength;

      int pos = 0;
      int entriesLength = 100;
      EntryData **entries = (EntryData**)safeMalloc(entriesLength*sizeof(EntryData*),"Entry Datas");

      entrySet->entries = entries;
      entrySet->size = entriesLength;
      while (entryPointer < endPointer){
        EntryData *entry = (EntryData*)entryPointer;
        char type = entry->type;

        if (entry->flags & CSI_ENTRY_ERROR){
	  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "entry has error\n");
          entryPointer += sizeof(EntryData);
        } else{
          switch (entry->type){
          case '0':
            {
              CSICatalogData *catalogData = (CSICatalogData*)(entry+14);
              zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "catalog name %44.44s\n",catalogData->name);
              entryPointer += sizeof(CSICatalogData);     
            }
            break;
          default:
            {
              int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
              int advance = sizeof(EntryData)+fieldDataLength-4; /* -4 for the fact that the length is 4 from end of EntryData */

              for (int i = 0; i < typesCount; i++) {
                if (typesAllowed[i] == entry->type) {
                  if (pos == entriesLength-1){
                    EntryData **tempPtr = (EntryData**)safeMalloc(entriesLength*2*sizeof(EntryData*),"Entry Datas");
                   
                    for (int count = 0; count < pos; count++){
                      tempPtr[count] = entries[count];
                    }
                    
                    safeFree((char*)entries,entriesLength*sizeof(EntryData*)); 
                    entries = tempPtr;                   
                    entriesLength = entriesLength*2;
                    entrySet->entries = entries;
                    entrySet->size = entriesLength*2;
                  }
                  EntryData *entryCopy = (EntryData*)safeMalloc(advance,"Entry");
                  memcpy(entryCopy,entry,advance);
                  entries[pos] = entryCopy;
                  pos++;
                  entrySet->length = pos;
                  break;
                }
              }
              entryPointer += advance;
            }
            break;
          }
        }
      }
      safeFree31((char*)workArea,workAreaSize);
      return entrySet;
    } else{
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "no entries, no look up failure either\n");
    }
    safeFree31((char*)workArea,workAreaSize);
    return entrySet;
  } else{
    return entrySet;
  }
}

static const char hlqFirstChar[] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','$','@','#'};


EntryDataSet *getHLQs(char *typesAllowed, int typesCount, int workAreaSize, char **fields, int fieldCount, csi_parmblock * __ptr32 * __ptr32 returnParmsArray){
  int status = 0;
  char *searchTerm = safeMalloc(3,"HLQ Search Term");
  searchTerm[1] = '*';
  searchTerm[2] = '\0';
  EntryDataSet **entrySets = (EntryDataSet**)safeMalloc31(29*sizeof(EntryDataSet*),"HLQ entries");
  int combinedLength = 0;

  for (int i = 0; i < 29; i++){
    searchTerm[0] = hlqFirstChar[i];
    returnParmsArray[i] = (csi_parmblock* __ptr32)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");
    EntryDataSet *entrySet = returnEntries(searchTerm, typesAllowed, typesCount, workAreaSize, fields, fieldCount, NULL, NULL, returnParmsArray[i]);
    entrySets[i] = entrySet;
    combinedLength = combinedLength + entrySet->length;
  }
  safeFree(searchTerm,3);
  EntryDataSet *combinedEntrySet = (EntryDataSet*)safeMalloc(sizeof(EntryDataSet),"HLQ Combined Entries Set");
  combinedEntrySet->length = combinedLength;
  combinedEntrySet->entries = (EntryData**)safeMalloc(sizeof(EntryData*)*combinedLength,"HLQ Combined Entries");
  int pos = 0;  
  for (int i = 0; i < 29; i++){
    if (entrySets[i]->length > 0) {
      for (int j = 0; j < entrySets[i]->length; j++){
        combinedEntrySet->entries[pos++]=entrySets[i]->entries[j];
      }
    }
  }
  return combinedEntrySet;
}

void freeEntryDataSet(EntryDataSet *entrySet) {
  if (entrySet) {
    if (entrySet->entries) {
      for (int i = 0; i < entrySet->length; i++) {
        EntryData *currentEntry = entrySet->entries[i];
        int fieldDataLength = currentEntry->data.fieldInfoHeader.totalLength;
        int entrySize = sizeof(EntryData) + fieldDataLength - 4;
        memset((char*)(currentEntry),0, entrySize);
        safeFree((char*)(currentEntry), entrySize);
      }
      safeFree((char*)entrySet->entries, entrySet->size * sizeof(EntryData*));
      entrySet->entries = NULL;
    }
    entrySet->size = 0;
    entrySet->length = 0;
    safeFree((char*)entrySet, sizeof(entrySet));
  }
}


#ifdef TEST_JCSI
int main(int argc, char **argv)
{
  int status = 0;
  if ((status = loadCsi()) != 0) {
    fprintf (stderr, "failed to load IGGCSI00, status = 0x%x\n", status);
    return EXIT_FAILURE;
  }
  int status = pseudoLS(argv[1],3,entriesFields);
  printf("status 0x%x\n",status);
}
#endif







/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

