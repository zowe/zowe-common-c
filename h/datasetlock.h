/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __DATASETLOCK__
#define __DATASETLOCK__ 1

#define HEARTBEAT_DEFAULT 30
#define LOCK_EXPIRY 30
/* declare the table that maps datasets to semaphores */
#define N_SEM_TABLE_ENTRIES 101
#define HEARTBEAT_BACKBONE_SIZE 101

#define DSN_LEN 44
#define MEMBER_LEN 8
#define USERNAME_LEN 8

#define NO_MATCH_DSN -1
#define NO_MATCH_USER -2

#include "collections.h"
#include "pthread.h"

#define SEMTABLE_UNKNOWN_ERROR  0x01
#define SEMTABLE_CAPACITY_ERROR  0x02
#define SEMTABLE_SEMGET_ERROR  0x03
#define SEMTABLE_EXISTING_DATASET_LOCKED  0x04
#define SEMTABLE_EXISTING_SAME_USER  0x05
#define SEMTABLE_UNABLE_SET_SEMAPHORE  0x06
#define SEMTABLE_ENTRY_NOT_FOUND  0x07
#define SEMTABLE_SEM_DECREMENT_ERROR  0x08
#define LOCK_RESOURCE_CONFLICT  0x09
#define LOCK_EXCLUSIVE_ERROR  0x0A
#define SEMTABLE_SUCCESS  0x00

typedef int seconds;

typedef struct DsnMember_tag {
  char  dsn[DSN_LEN];
  char  membername[MEMBER_LEN];
} DsnMember;

typedef struct SemTable_type {
  DsnMember dsnMember;
  char user [USERNAME_LEN];
  int semId;
} SemEntry;

/* declare heartbeat table */
typedef struct DatasetLockService_tag {
  seconds expiry;
  seconds heartbeat;
  hashtable *hbtTable;
  hashtable *semTable;
  pthread_mutex_t datasetMutex;
} DatasetLockService;


// hbtTable hbtTable [N_HBT_TABLE_ENTRIES];  
DatasetLockService *lockService;     

//initialize
DatasetLockService* initLockResources(int heartbeat, int expiry);

//heartbeart
void addUserToHbt(DatasetLockService *lockService, char* user);
void resetTimeInHbt(DatasetLockService *lockService, char* user);
void heartbeatBackgroundHandler(DatasetLockService *lockService);

// semaphore
int sleepSemaphore(SemEntry* entry);
int findSemTableEntryByDatasetByUser(DatasetLockService *lockService, DsnMember *dsnMember, char* username, SemEntry **entryPtr);
int findSemTableEntryByDataset(DatasetLockService *lockService, DsnMember *dsnMember, SemEntry **entryPtr);
int semTableEnqueue(DatasetLockService *lockService, DsnMember *dsnMember, char *username,  SemEntry** entryPtr);
int semTableDequeue(DatasetLockService *lockService, DsnMember *dsnMember, char *username);
#endif
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/