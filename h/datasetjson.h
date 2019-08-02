

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __DATASETJSON__
#define __DATASETJSON__ 1


#include "stcbase.h"
#include "json.h"
#include "xml.h"
#include "jcsi.h"

#define DATA_STREAM_BUFFER_SIZE 4096

#define SAF_AUTHORIZATION_READ 0x04
#define SAF_AUTHORIZATION_UPDATE 0x08
#define MEMBER_MAX 8
#define DATASET_MEMBER_MAXLEN DATASET_NAME_LEN + MEMBER_MAX + 6

typedef struct MetadataQueryCache_tag{
  EntryDataSet *cachedHLQSet;
  csi_parmblock **cachedCSIParmblocks;
} MetadataQueryCache;

typedef struct serveVSAMCache_tag{
  hashtable *acbTable;
} serveVSAMCache;

typedef struct StatefulACB_tag {
  char *acb; /* contains 8-byte plist */
#define VSAM_TYPE_KSDS  1
#define VSAM_TYPE_ESDS  2
#define VSAM_TYPE_LDS   3
#define VSAM_TYPE_RRDS  4
  int type;
  union {
    char *key;
    int rba;
    int ci;
    int record;
  } argPtr;
} StatefulACB;

int streamDataset(Socket *socket, char *filename, int recordLength, jsonPrinter *jPrinter);
int streamVSAMDataset(HttpResponse* response, char *acb, int maxRecordLength, int maxRecords, int maxBytes, int keyLoc, int keyLen, jsonPrinter *jPrinter);
void addDetailedDatasetMetadata(char *datasetName, int nameLength,
                                char *volser, int volserLength,
                                jsonPrinter *jPrinter);
void addMemberedDatasetMetadata(char *datasetName, int nameLength,
                                char *volser, int volserLength,
                                char *memberQuery, int memberLength,
                                jsonPrinter *jPrinter,
                                int includeUnprintable);

void respondWithDatasetMetadata(HttpResponse *response);
void respondWithHLQNames(HttpResponse *response, MetadataQueryCache *metadataQueryCache);
void respondWithDataset(HttpResponse* response, char* fullPath, int jsonMode, HttpService* service);
void updateDataset(HttpResponse* response, char* fullPath, int jsonMode, HttpService* service);
void removeDatasetMember(HttpResponse* response, char* datasetPath, char* memberName);
void newDatasetMember(HttpResponse* response, char* datasetPath, char* memberName);
#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

