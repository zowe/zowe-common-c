

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_OS_WINDOWS
#include <time.h>
#endif

#include <sys/sem.h>
#include "logging.h"
#include "isgenq.h"
#include "datasetlock.h"

static char* makeStringCopy(char *from) {
  size_t len = strlen(from);
  char* to=(char*)safeMalloc((len)*sizeof(char),"char");
  strcpy(to, from);
  return to;
}

static int semKeyHash(void *key) {
  DsnMember* k = (DsnMember*) key;
  char* dsn=(char*)k->dsn;
  char* membername=(char*)k->membername;
  int len1 = DSN_LEN;
  int len2 = MEMBER_LEN;
  int hash = 5381;
  int i;

  for (i=0; i<len1; i++){
    hash = (hash << 5) + dsn[i];
  }

  for (i=0; i<len2; i++){
    hash = (hash << 5) + membername[i];
  }

  return hash&0x7fffffff;
}

static int semKeyCompare(void *key1, void *key2) {
  // key1 come from table & key2 we pass in htGet as parameter
  DsnMember* k1 = (DsnMember*) key1;
  DsnMember* k2 = (DsnMember*) key2;
  return (memcmp(k1->dsn, k2->dsn, DSN_LEN)==0 
      && memcmp(k1->membername, k2->membername, MEMBER_LEN)==0);
}

static void freeKeySemTable(void* key) {
  DsnMember *dsn = key;
  safeFree((char*)key, sizeof(dsn));
}

static void freeValueSemTable(void *value) {
  SemEntry *entry = value;
  safeFree((char*)value, sizeof(entry));
}

static void freeKeyHeartbeatTable(void *key) {
  char *c = key;
  safeFree((char*)key, sizeof(c));
}

static void freeValueHeartbeatTable(void *value) {
  time_t *t = value;
  safeFree((char*)value, sizeof(t));
}

void addUserToHbt(DatasetLockService *lockService, char* user) {
  char* key = makeStringCopy(user);
  time_t *curr_time = (time_t*)safeMalloc(sizeof(time_t),"time_t");
  time(curr_time);
  // pthread_mutex_lock(&lockService->datasetMutex);
  // {
    htPut(lockService->hbtTable, key, curr_time);
  // }
  // pthread_mutex_unlock(&lockService->datasetMutex);
}

void resetTimeInHbt(DatasetLockService *lockService, char* user) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,"%s %s\n", __FUNCTION__, user);  
  addUserToHbt(lockService, user);
}

DatasetLockService* initLockResources(int heartbeat, int expiry) {
  /* Create semaphore table for datasets */
  DatasetLockService* lockService = (DatasetLockService*)safeMalloc(sizeof(DatasetLockService),"DatasetLockService");
  lockService->hbtTable = htCreate(HEARTBEAT_BACKBONE_SIZE, stringHash, stringCompare, NULL, freeValueHeartbeatTable);
  lockService->semTable = htCreate(N_SEM_TABLE_ENTRIES, semKeyHash, semKeyCompare, NULL, freeValueSemTable);
  pthread_mutex_init(&lockService->datasetMutex, NULL);
  if (heartbeat > 0) {
    lockService->heartbeat = heartbeat;
  } else {
    lockService->heartbeat = HEARTBEAT_DEFAULT;
  }

  if (expiry > 0) {
    lockService->expiry = expiry;
  } else {
    lockService->expiry = LOCK_EXPIRY;
  }
  return lockService;
}

static int searchUserInSem(DatasetLockService* lockService,char* user) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"%s %s\n", __FUNCTION__, user);
  
  int rc;
  SemEntry* value;
  hashtable* st = lockService->semTable;
  hashentry **backbone = st->backbone;
  if (backbone == NULL) return 0;
  int backboneSize = N_SEM_TABLE_ENTRIES;

  for (int j = 0; j < backboneSize; j++) {
    if (backbone[j] == NULL) continue;
    value = (SemEntry*)backbone[j]->value;
    if (value == NULL || value->semId == 0) continue;

    if (strcmp(value->user, user) == 0) {
      if (value->semId > 0) {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,"heartbeat expire user: %s dataset: %s %s semId: %d \n", value->user, value->dsnMember.dsn, value->dsnMember.membername, value->semId);
        wakeSempahore(value->semId);
      }
    }
  }

  return 0;
}

void heartbeatBackgroundHandler(DatasetLockService* lockService) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"%s\n", __FUNCTION__);  
  double diff_t;
  time_t curr_time;
  time(&curr_time);

  hashtable* ht = lockService->hbtTable;
  hashentry **backbone = ht->backbone;
  if (backbone == NULL) return;
  int backboneSize = HEARTBEAT_BACKBONE_SIZE;

  // TODO: big mutex blocking whole map every 30secs
  // pthread_mutex_lock(&lockService->datasetMutex);
  // {
    for (int j = 0; j < backboneSize; j++) {
      if (backbone[j] == NULL) continue;
      char* user = (char *)backbone[j]->key;
      time_t* ltime = (time_t*)backbone[j]->value;
      if (ltime == NULL) continue;

      diff_t = difftime(curr_time, *ltime);
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG2, " user:%s time_diff:%f\n", user,diff_t);                                                      
      if (diff_t >= lockService->expiry) { 
          int rc = searchUserInSem(lockService, user);
          freeValueHeartbeatTable(ltime);
          htPut(lockService->hbtTable, user, NULL);
      }
    }
  // }
  // pthread_mutex_unlock(&lockService->datasetMutex);

  return;
}

static int createSemaphore(DsnMember* dsnMember) {
  int semaphoreID;
  pid_t pid = getpid();

  /* create semaphore */
  key_t key = pid + semKeyHash(dsnMember);  /* key to pass to semget() */
  int nsems = 1;  /* number of semaphores in each 'array' of semaphores to be created */
  semaphoreID = semget(
        key,                  /* key value */
        nsems,                /* number of entries */
        IPC_CREAT | 0666      /* create a new semaphore with perms rw-rw-rw   */
        );

  return semaphoreID;
}

static int setSemaphore(int semaphoreID) {
  // semctl() changes permissions and other characteristics of a semaphore set
  union semun {
      int val;
      struct semid_ds *buf;
      unsigned short *array;
  } arg;
  arg.val = 1;
  
  int semnum = 0;    /* array index zero */
  int semaphoreRetcode;
  semaphoreRetcode = semctl(semaphoreID, semnum, SETVAL, arg); /* set the value of our semaphore */
  return semaphoreRetcode;
}

int sleepSemaphore(DatasetLockService* lockService, SemEntry* entry) {

  int semaphoreRetcode;
  int semaphoreID = entry->semId;
  /* define semaphore operation to be performed */
  struct sembuf semaphoreBuffer[1]; /* just one semaphore in the array */
  struct sembuf *semaphoreOps = &semaphoreBuffer[0];
  semaphoreBuffer[0].sem_num = 0; /* index of first and only semaphore */
  semaphoreBuffer[0].sem_op  = 0; /* 0 = wait */
  semaphoreBuffer[0].sem_flg = 0; /* 0 = sychronous + don't undo */

  semaphoreRetcode = semop(semaphoreID, semaphoreOps, 1);
  /* we are now waiting for our semaphore to be posted ... */

  if (semaphoreRetcode == 0 ) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"destroy semId:%d %s/n",semaphoreID, __FUNCTION__);
    /* destroy our semaphore */
    semaphoreRetcode = semctl(semaphoreID, 0, IPC_RMID);
    if (semaphoreRetcode != -1 ) {
      releaseSemTableSlot(lockService, entry);
    }
  }

  return semaphoreRetcode;
}

static int wakeSempahore(int semaphoreID) {

  /* post semaphore semaphoreOps */
  struct sembuf semaphoreBuffer[1];
  struct sembuf *semaphoreOps = &semaphoreBuffer[0];
  semaphoreBuffer[0].sem_num = 0;
  semaphoreBuffer[0].sem_op  = -1;    /* decrement */
  semaphoreBuffer[0].sem_flg = 0;

  int semaphoreRetcode;
  semaphoreRetcode = semop(semaphoreID, semaphoreOps, 1); /* 0=wait, 1=increment */
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"semId:%d %d end %s/n",semaphoreID, semaphoreRetcode, __FUNCTION__);
  return semaphoreRetcode;
}

static int takeExclusiveLock(DsnMember *dsnMember) {
  RName rname_parm; /* resource name; the name of the resource to acquire; the dataset name + optional member */
  static const QName MajorQNAME = {"SPFEDIT "}; 

  ENQToken lockToken;
  int lockRC = 0, lockRSN = 0;

  memcpy(rname_parm.value, dsnMember, sizeof(*dsnMember) ); /* copy in dsn+membername */
  rname_parm.length = 52;

  /* Does not wait forever because CONTENTIONACT=FAIL */
  lockRC = isgenqTryExclusiveLock(
      &MajorQNAME, 
      &rname_parm, 
      ISGENQ_SCOPE_SYSTEMS, 
      &lockToken, 
      &lockRSN);   

  if ( lockRC != 0) {
    return lockRC;
  }

  /* This function WAITs in case of contention.  This should not be a problem because it's preceeded by isgenqTryExclusiveLock */
  /* the lock is ALWAYS released when the C program exits. */
  lockRC = isgenqGetExclusiveLock( 
      &MajorQNAME, 
      &rname_parm, 
      ISGENQ_SCOPE_SYSTEMS, 
      &lockToken, 
      &lockRSN);   

  return 0;
}

static SemEntry* makeSemTableEntry(int semaphoreID, DsnMember *dsnMember, char* username) {
  SemEntry* entry = (SemEntry*)safeMalloc(sizeof(SemEntry),"SemEntry");
  entry->semId = semaphoreID;
  memcpy(entry->dsnMember.dsn, dsnMember->dsn, DSN_LEN);/* copy in member */
  memcpy(entry->dsnMember.membername, dsnMember->membername, MEMBER_LEN); /* copy in dsn */
  memcpy(entry->user, username, strlen(username)); /* copy in user id */
  return entry;
}

static int fillSemTableEntry(DatasetLockService *lockService, int semaphoreID, DsnMember *dsnMember, char* username, SemEntry** entryPtr) {
  SemEntry* entry;
  entry = makeSemTableEntry(semaphoreID, dsnMember, username);
  *entryPtr = entry;
  int ret = htPut(lockService->semTable, &(entry->dsnMember), entry);
  SemEntry* val = (SemEntry*)htGet(lockService->semTable,&(entry->dsnMember));
  addUserToHbt(lockService, username);  
  return ret;
}

int findSemTableEntryByDataset(DatasetLockService *lockService, DsnMember *dsnMember, SemEntry** entryPtr) {
  SemEntry* entry = (SemEntry*)htGet(lockService->semTable, dsnMember);

  if (entry != NULL) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"dataset existing semId:%d dsn:%s %s\n", entry->semId, entry->dsnMember.dsn,__FUNCTION__);  
    *entryPtr = entry;
    return 0;
  } 

  return NO_MATCH_DSN;
}

int findSemTableEntryByDatasetByUser(DatasetLockService *lockService, DsnMember *dsnMember, char* username, SemEntry** entryPtr) {
  SemEntry* entry;
  int ret = findSemTableEntryByDataset(lockService, dsnMember, &entry);
  if (ret == 0 && entry != NULL) {
    if (strcmp(entry->user,username) == 0) {
       zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"dataset and user both match\n");
      *entryPtr = entry;
      return 0; 
    } else {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"dataset existing, but different user semId:%d dsn:%s user:%s  request user:%s %s\n", entry->semId, entry->dsnMember.dsn,entry->user, username, __FUNCTION__);  
      return NO_MATCH_USER;//only user don't match
    }
  }
  return NO_MATCH_DSN;//both dont match
}

static int acquireSemTableSlot(DatasetLockService* lockService, DsnMember *dsnMember, char* username, SemEntry** entryPtr) {
  SemEntry *entry;
  int ret = findSemTableEntryByDatasetByUser(lockService, dsnMember, username, &entry);
  
  if (ret == NO_MATCH_DSN) {
    ret = fillSemTableEntry(lockService, 0, dsnMember, username, &entry);
  } 

  *entryPtr = entry;
  return ret;
}

static int releaseSemTableSlot(DatasetLockService *lockService, SemEntry* entry) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"%s user: %s dsn: %s mem: %s\n", __FUNCTION__, entry->user, entry->dsnMember.dsn,  entry->dsnMember.membername); 
  int ret = -1;
  pthread_mutex_lock(&lockService->datasetMutex);
  {
    ret = htRemove(lockService->semTable, &entry->dsnMember);
  }
  pthread_mutex_unlock(&lockService->datasetMutex);

  return ret;
}

int accquireLockAndSemaphore(SemEntry* entry) {
  int lockRC = takeExclusiveLock(&(entry->dsnMember));
  if ( lockRC != 0) {
    if (lockRC == 4) {
      return LOCK_RESOURCE_CONFLICT;
    } else {
      return LOCK_EXCLUSIVE_ERROR;
    }
  }

  int semaphoreID;
  /* not already present, do nothing if present */
  semaphoreID = createSemaphore(&(entry->dsnMember));
  if (semaphoreID == -1 ) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"SEMTABLE_SEMGET_ERROR %s\n", __FUNCTION__);  
    return SEMTABLE_SEMGET_ERROR;
  }

  entry->semId = semaphoreID;
  int semaphoreRetcode = setSemaphore(semaphoreID);
  if (semaphoreRetcode == -1 ) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2,"SEMTABLE_UNABLE_SET_SEMAPHORE %s\n", __FUNCTION__);  
    return SEMTABLE_UNABLE_SET_SEMAPHORE;
  }

  return SEMTABLE_SUCCESS;
}

int semTableEnqueue(DatasetLockService *lockService, DsnMember *dsnMember, char *username, SemEntry** entryPtr) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2," %s\n", __FUNCTION__);  

  int ret;
  SemEntry* entry;
  pthread_mutex_lock(&lockService->datasetMutex);
  {
    ret = acquireSemTableSlot(lockService, dsnMember, username, &entry);
  }
  pthread_mutex_unlock(&lockService->datasetMutex);

  if (ret == NO_MATCH_USER) {
    return SEMTABLE_EXISTING_DATASET_LOCKED;
  }

  if (ret == 0 && entry->semId > 0) {
    return SEMTABLE_EXISTING_SAME_USER;
  }

  ret = accquireLockAndSemaphore(entry);
  if (ret != SEMTABLE_SUCCESS) {
    releaseSemTableSlot(lockService, entry);
  }
  *entryPtr=entry;
  return ret; 
}

int semTableDequeue(DatasetLockService *lockService, DsnMember *dsnMember, char *username) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2," %s\n", __FUNCTION__);  
  SemEntry* entry;
  int ret = findSemTableEntryByDatasetByUser(lockService, dsnMember, username, &entry);
  if (ret == NO_MATCH_USER) {
    return SEMTABLE_EXISTING_DATASET_LOCKED; 
  } else if (ret == NO_MATCH_DSN) {
    return SEMTABLE_ENTRY_NOT_FOUND;
  }

  int semaphoreRetcode = wakeSempahore(entry->semId);
  if (semaphoreRetcode == -1 ) {
    return SEMTABLE_SEM_DECREMENT_ERROR;
  }

  return SEMTABLE_SUCCESS;         
}