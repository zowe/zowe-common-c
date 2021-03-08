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
#define N_SEM_TABLE_ENTRIES 100
#define HEARTBEAT_BACKBONE_SIZE 101

#include "collections.h"
#include "pthread.h"

#define SEMTABLE_UNKNOWN_ERROR  0x01
#define SEMTABLE_CAPACITY_ERROR  0x02
#define SEMTABLE_SEMGET_ERROR  0x03
#define SEMTABLE_EXISTING_DATASET_LOCKED  0x04
#define SEMTABLE_UNABLE_SET_SEMAPHORE  0x05
#define SEMTABLE_ENTRY_NOT_FOUND  0x06
#define SEMTABLE_SEM_DECREMENT_ERROR  0x08
#define LOCK_RESOURCE_CONFLICT  0x09
#define LOCK_EXCLUSIVE_ERROR  0x0A
#define SEMTABLE_SUCCESS  0x00

typedef int seconds;

typedef struct Dsn_Member_tag {
  char  dsn[44];
  char  membername[8];
} Dsn_Member;

typedef struct SemTable_type {
  char dsn [44];
  char mem [8];
  char usr [8];
  time_t ltime;
  int  cnt;
  int semId;
} SemTable;

/* declare heartbeat table */
typedef struct DatasetLockService_tag {
  seconds expiry;
  seconds heartbeat;
  hashtable *hbtTable;
  hashtable *lockTable;
  pthread_mutex_t datasetMutex;
} DatasetLockService;


SemTable semTable [N_SEM_TABLE_ENTRIES];
// hbtTable hbtTable [N_HBT_TABLE_ENTRIES];  
DatasetLockService *lockService;     

//initialize
void initLockResources();

//heartbeart
void addUserToHbt(char* user);
void resetTimeInHbt(char* user);
void heartbeatBackgroundHandler(void* server);

// semaphore
int sleepSemaphore(int entryId);
int findSemTableEntryByDatasetByUser(Dsn_Member *dsn_member, char* username, SemTable **entryPtr);
int findSemTableEntryByDataset(Dsn_Member *dsn_member, SemTable **entryPtr);
int semTableEnqueue(Dsn_Member *dsn_member, char *username,  int* retEntryId);
int semTableDequeue(Dsn_Member *dsn_member, char *username);
#endif
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/