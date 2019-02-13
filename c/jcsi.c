

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

#include "csi.h"
#include "jcsi.h"
#define _EDC_ADD_ERRNO2 1

/* 
   c89 -o csi -Wl,ac=1 csitest.c                       

   c89 -DCSITEST=1 "-Wc,langlvl(extc99),gonum,goff,hgpr,roconst,ASM,asmlib('SYS1.MACLIB')" "-Wl,ac=1" -I ../h -o jcsi jcsi.c ../c/utils.c ../c/alloc.c 
  
   extattr +a csitest                            
  
   csi -filter <dsn_filter_spec> 

*/



typedef void (*void_fn_ptr)();
void_fn_ptr fetch(const char *name);

typedef int csi_fn(int* reason_code, void *param_block, void* work_area);
csi_fn *IGGCSI00;

#define TRUE 1
#define FALSE 0



void print_buffer(char *buffer_arg, int length_arg);




csi_parmblock *process_arguments(int argc, char **argv)
{
  int argindex;
  int arglen;

  csi_parmblock* csi_parms = (csi_parmblock*)safeMalloc(sizeof(csi_parmblock),"CSI Parmblock");
  memset(csi_parms,' ',sizeof(csi_parmblock));
  for (argindex=0; argindex<argc; argindex++) {
    if (0) {
    } else if (0 == strcmp(argv[argindex], "-filter")) {
      if (++argindex >= argc) {
        printf("missing argument after -filter\n");
	return 0;
      } else {
	arglen = strlen(argv[argindex]);
	strncpy(csi_parms->filter_spec,argv[argindex],arglen); /* do not copy null terminator */
        /* sscanf(argv[argindex],"%d",&ssgargl->buffer_length); */
      }
    }
  }
  csi_parms->num_fields = 4;
  strncpy(csi_parms->fields+  0,"NAME    ",8);
  strncpy(csi_parms->fields+  8,"TYPE    ",8);
  strncpy(csi_parms->fields+ 16,"VOLSER  ",8);
  strncpy(csi_parms->fields+ 24,"VOLFLG  ",8);

  return csi_parms;
}

char *csi(csi_parmblock* csi_parms, int *workAreaSize)
{
  int return_code;
  int reason_code;
  *workAreaSize = (*workAreaSize > 2048 ? *workAreaSize : WORK_AREA_SIZE);
  void* work_area = (void*)safeMalloc(*workAreaSize,"CSI Work Area Size");
  *((int*)work_area) = *workAreaSize;
  IGGCSI00 = (csi_fn *)fetch("IGGCSI00");
  if (0 == IGGCSI00) {
    printf("could not fetch IGGCSI00\n");
    return 0;
  }
  /* print_buffer((char*)work_area,256); */
  return_code = (*IGGCSI00)(&reason_code,csi_parms,work_area);
  if (return_code != 0) {
    printf("CSI failed ret=%d, rc=%d rchex=%x\n",return_code,reason_code,reason_code);
    free(work_area);
    return 0;
  } else {
    /* print_buffer((char*)work_area,256); */
    return work_area;
  }
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

  csi_parms = (csi_parmblock*)safeMalloc(sizeof(csi_parmblock),"CSI ParmBlock");

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
      printf("work area length used %d\n",workAreaUsedLength);
      dumpbuffer(workArea,workAreaUsedLength);

      while (entryPointer < endPointer){
        EntryData *entry = (EntryData*)entryPointer;

        printf("entry name %44.44s\n",entry->name);
        dumpbuffer(entryPointer,0x60);        
        if (entry->flags & CSI_ENTRY_ERROR){
          printf("entry has error\n");
          entryPointer += sizeof(EntryData);
        } else{
          switch (entry->type){
          case '0':
            {
              CSICatalogData *catalogData = (CSICatalogData*)(entry+14);
              printf("catalog name %44.44s\n",catalogData->name);
              entryPointer += sizeof(CSICatalogData);     
            }
            break;
          default:
            {
              int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
              printf("entry field data length = 0x%x fieldCount=%d\n",fieldDataLength,fieldCount);
              char *fieldData = entryPointer+sizeof(EntryData);
              unsigned short *fieldLengthArray = ((unsigned short *)(entryPointer+sizeof(EntryData)));
              if (entry->type == 'X'){
                printf("entry is catalog alias\n");
              } else if (entry->type == 'U'){
                printf("entry is user catalog connector\n");
              } else{
                char *fieldValueStart = entryPointer+sizeof(EntryData)+fieldCount*sizeof(short);
                for (i=0; i<fieldCount; i++){
                  printf("field %d: %8.8s, has length %d\n",i,fieldNames[i]);
                  if (fieldLengthArray[i]){
                    dumpbuffer(fieldValueStart,fieldLengthArray[i]);
                  }
                  fieldValueStart += fieldLengthArray[i];
                }
                /* printf("volser = %6.6s\n",fieldData); */
              }
              int advance = sizeof(EntryData)+fieldDataLength-4; /* -4 for the fact that the length is 4 from end of EntryData */
              entryPointer += advance;
            }
            break;
          }
        }
      }
    } else{
      printf("no entries, no look up failure either\n");
    }
    safeFree((char*)workArea,WORK_AREA_SIZE);
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

EntryDataSet *returnEntries(char *dsn, char *typesAllowed, int typesCount, int workAreaSize, char **fields, int fieldCount, char *resumeName, char *resumeCatalogName, csi_parmblock *returnParms){
  csi_parmblock* csi_parms = returnParms;
  int dsnLen = strlen(dsn);
  char *workArea = NULL;

  workAreaSize = (workAreaSize > 0 ? workAreaSize : WORK_AREA_SIZE);
  
  EntryDataSet *entrySet;
  entrySet = (EntryDataSet*)safeMalloc(sizeof(EntryDataSet*),"Entry Data Set");
  entrySet->length = 0;

  if (dsnLen == 0){
    return NULL;
  }

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
      while (entryPointer < endPointer){
        EntryData *entry = (EntryData*)entryPointer;
        char type = entry->type;

        if (entry->flags & CSI_ENTRY_ERROR){
          printf("entry has error\n");
          entryPointer += sizeof(EntryData);
        } else{
          switch (entry->type){
          case '0':
            {
              CSICatalogData *catalogData = (CSICatalogData*)(entry+14);
              printf("catalog name %44.44s\n",catalogData->name);
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
      safeFree((char*)workArea,workAreaSize);
      return entrySet;
    } else{
      printf("no entries, no look up failure either\n");
    }
    safeFree((char*)workArea,workAreaSize);
    return entrySet;
  } else{
    return entrySet;
  }
}

static const char hlqFirstChar[] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','$','@','#'};


EntryDataSet *getHLQs(char *typesAllowed, int typesCount, int workAreaSize, char **fields, int fieldCount, csi_parmblock **returnParmsArray){
  int status = 0;
  char *searchTerm = safeMalloc(3,"HLQ Search Term");
  searchTerm[1] = '*';
  searchTerm[2] = '\0';
  EntryDataSet **entrySets = (EntryDataSet**)safeMalloc(29*sizeof(EntryDataSet*),"HLQ entries");
  int combinedLength = 0;

  for (int i = 0; i < 29; i++){
    searchTerm[0] = hlqFirstChar[i];
    returnParmsArray[i] = (csi_parmblock*)safeMalloc(sizeof(csi_parmblock),"CSI ParmBlock");
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


/*
int main(int argc, char **argv)
{
  
  EntryDataSet *hlqSet = getHLQs();
  for (int i = 0; i < hlqSet->length;i++){
    //printf("Checking entries pos=%d\n",i);
    if (hlqSet->entries[i] && hlqSet->entries[i]->name) {
    //dumpbuffer(hlqSet->entries[i]->name,44);
    printf("Found entry: %44.44s\n",hlqSet->entries[i]->name);
    fflush(stdout);
    }
  }
  return 0;
  
  int status = pseudoLS(argv[1],3,entriesFields);
  printf("status 0x%x\n",status);
  return status;
}
*/








/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

