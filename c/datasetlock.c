

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

#include "datasetlock.h"
#include <sys/sem.h>
#include "logging.h"
#include "isgenq.h"

static char* makeStringCopy(char *from) {
  size_t len = strlen(from);
  char* to=(char*)safeMalloc((len+1)*sizeof(char));
  memset(to, 0, len+1);
  strncpy(to, from, len);
  return to;
}

static void freeValueHeartbeatTable(void *value) {
  time_t *t = value;
  safeFree((time_t*)value, sizeof(t));
}

static void freeKeyHeartbeatTable(void *key) {
  char *c = key;
  safeFree((char*)key, sizeof(c));
}

void addUserToHbt(char* user) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,"%s %s\n", __FUNCTION__, user);  
  char* key = makeStringCopy(user);
  time_t *curr_time = (time_t*)safeMalloc(sizeof(time_t));
  time(curr_time);
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,"usr %s curr_time %s\n", key, ctime(curr_time));  
  // pthread_mutex_lock(&lockService->datasetMutex);
  // {
    htPut(lockService->hbtTable, key, curr_time);
  // }
  // pthread_mutex_unlock(&lockService->datasetMutex);
}

void resetTimeInHbt(char* user) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,"%s %s\n", __FUNCTION__, user);  
  addUserToHbt(user);
}

void initLockResources(int heartbeat, int expiry) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"%s\n", __FUNCTION__);  
  /* Create semaphore table for datasets */
  for(int i=0; i < N_SEM_TABLE_ENTRIES; i++) {
    semTable[i].semId = 0;  /* initialise */
  }
  
  lockService = (DatasetLockService*)safeMalloc(sizeof(DatasetLockService),"DatasetLockService");
  lockService->hbtTable = htCreate(HEARTBEAT_BACKBONE_SIZE, stringHash, stringCompare, NULL, freeValueHeartbeatTable);
  pthread_mutex_init(&lockService->datasetMutex, NULL);
  if (heartbeat > 0) {
    lockService->heartbeat=heartbeat;
  } else {
    lockService->heartbeat = HEARTBEAT_DEFAULT;
  }

  if (expiry > 0) {
    lockService->expiry=expiry;
  } else {
    lockService->expiry = LOCK_EXPIRY;
  }
  return;
}

int searchUserInSem(char* user) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,"%s %s\n", __FUNCTION__, user);
  
  int rc;
  for (int i = 0; i < N_SEM_TABLE_ENTRIES; i++) {
    if (semTable[i].semId == 0) continue;

    if (strcmp(semTable[i].usr, user) == 0) {
      if (semTable[i].semId > 0){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_INFO,"heartbeat expire %s dataset: %s %s\n", semTable[i].usr, semTable[i].dsn, semTable[i].mem);
        wakeSempahore(semTable[i].semId);
        semTable[i].semId=0;
      }
    }
  }

  return 0;
}

void heartbeatBackgroundHandler(void* server) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,"%s\n", __FUNCTION__);  
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
    for(int j=0; j <backboneSize; j++) {
      if (backbone[j] == NULL) continue;
      char* usr = (char *)backbone[j]->key;
      time_t* ltime = (time_t*)backbone[j]->value;
      if (ltime == NULL) continue;

      diff_t = difftime(curr_time, *ltime);
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, " user:%s time_diff:%f\n", usr,diff_t);                                                      
      if (diff_t > lockService->expiry) { 
          int rc = searchUserInSem(usr);
          freeValueHeartbeatTable(ltime);
          htPut(lockService->hbtTable, usr, NULL);
      }
    }
  // }
  // pthread_mutex_unlock(&lockService->datasetMutex);

  return;
}

static int createSemaphore(int entryId) {

  int semaphoreID;
  pid_t pid = getpid();

  /* create semaphore */
  key_t key = pid + entryId;  /* key to pass to semget() */
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

int sleepSemaphore(int entryId) {

  int semaphoreRetcode;
  int semaphoreID = semTable[entryId].semId;
  /* define semaphore operation to be performed */
  struct sembuf semaphoreBuffer[1]; /* just one semaphore in the array */
  struct sembuf *semaphoreOps = &semaphoreBuffer[0];
  semaphoreBuffer[0].sem_num = 0; /* index of first and only semaphore */
  semaphoreBuffer[0].sem_op  = 0; /* 0 = wait */
  semaphoreBuffer[0].sem_flg = 0; /* 0 = sychronous + don't undo */

  semaphoreRetcode = semop(semaphoreID, semaphoreOps, 1);
  /* we are now waiting for our semaphore to be posted ... */

  if (semaphoreRetcode == 0 ) {
    /* destroy our semaphore */
    semaphoreRetcode = semctl(semaphoreID, 0, IPC_RMID);
    if (semaphoreRetcode != -1 ) {
        semTable[entryId].semId = 0;  /* mark as deleted */
    }
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "semctl destroy semaphore %d\n", semaphoreRetcode);
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
  return semaphoreRetcode;
}

static int takeExclusiveLock(Dsn_Member *dsn_member) {
  RName rname_parm; /* resource name; the name of the resource to acquire; the dataset name + optional member */
  static const QName MajorQNAME  = {"SPFEDIT "}; 

  ENQToken lockToken;
  int lockRC = 0, lockRSN = 0;

  memcpy(rname_parm.value, dsn_member, sizeof(*dsn_member) ); /* copy in dsn+membername */
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

static int fillSemTableEntry(int entryId, int semaphoreID, Dsn_Member *dsn_member, char* username) {
  /* got semaphore */
  /* fill the table slot */
  semTable[entryId].semId = semaphoreID;
  memcpy(semTable[entryId].mem, dsn_member->membername, 8);   /* copy in member */
  memcpy(semTable[entryId].dsn, dsn_member->dsn, 44); /* copy in dsn */
  memcpy(semTable[entryId].usr, username, 8); /* copy in user id */
  addUserToHbt(semTable[entryId].usr);   
  return 0;
}

int findSemTableEntryByDataset(Dsn_Member *dsn_member, SemTable** entryPtr) {
  int i;
  for(i=0; i<N_SEM_TABLE_ENTRIES; i++)
  {  
    if (semTable[i].semId != 0                                          /* semaphore exists */      
        && memcmp(semTable[i].dsn, dsn_member->dsn, 44) == 0                  /* DSN matches */
        && memcmp(semTable[i].mem, dsn_member->membername, 8) == 0                /* member matches */
        ) /* present */
    {
      /* dsn found in table at row  i */  
      *entryPtr=(semTable+i);
      return i;
    }
  }

  return i;
}

int findSemTableEntryByDatasetByUser(Dsn_Member *dsn_member, char* username, SemTable** entryPtr) {
  SemTable* entry;
  int entryId=findSemTableEntryByDataset(dsn_member, &entry);
  if (entryId >= N_SEM_TABLE_ENTRIES || entry==NULL || strcmp(entry->usr,username)!=0) {
    return N_SEM_TABLE_ENTRIES; 
  }

  *entryPtr=entry;
  return entryId;
}

static int accquireSemTableSlot(Dsn_Member *dsn_member, char* username) {
  int i;
  for(int i=0; i < N_SEM_TABLE_ENTRIES; i++)
  {  

    if (semTable[i].semId != 0                                  /* semaphore exists */      
        && memcmp(semTable[i].dsn, dsn_member->dsn, 44) == 0          /* DSN matches */
        && memcmp(semTable[i].mem, dsn_member->membername, 8) == 0   /* member matches */
        ) /* already present */
    {
      return -1;
    }
    if (semTable[i].semId == 0) /* found empty slot in table */
    {
      //book sem table slot
      semTable[i].semId=-1;
      return i;
    }

  } /* end FOR loop */

  return N_SEM_TABLE_ENTRIES;
}

static int releaseSemTableSlot(int entryId) {
  semTable[entryId].semId=0;
  memset(semTable[entryId].mem, 0, 8);   /* copy in member */
  memset(semTable[entryId].dsn, 0, 44); /* copy in dsn */
  memset(semTable[entryId].usr, 0, 8); /* copy in user id */
}

int accquireLockAndSemaphore(Dsn_Member *dsn_member, char *username, int entryId) {
  int lockRC = takeExclusiveLock(dsn_member);
  if ( lockRC != 0) {
    if (lockRC == 4) {
      return LOCK_RESOURCE_CONFLICT;
    } else {
      return LOCK_EXCLUSIVE_ERROR;
    }
  }

  int semaphoreID;
  /* not already present, do nothing if present */
  semaphoreID = createSemaphore(entryId);
  if (semaphoreID == -1 ) {
    return SEMTABLE_SEMGET_ERROR;
  }

  fillSemTableEntry(entryId, semaphoreID, dsn_member, username);

  int semaphoreRetcode=setSemaphore(semaphoreID);
  if (semaphoreRetcode == -1 ) {
    return SEMTABLE_UNABLE_SET_SEMAPHORE;
  }

  return SEMTABLE_SUCCESS;
}

int semTableEnqueue(Dsn_Member *dsn_member, char *username, int* retEntryId) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG," %s\n", __FUNCTION__);  

  int entryId;
  pthread_mutex_lock(&lockService->datasetMutex);
  {
    entryId = accquireSemTableSlot(dsn_member, username);
  }
  pthread_mutex_unlock(&lockService->datasetMutex);

  if (entryId >= N_SEM_TABLE_ENTRIES) {
    return SEMTABLE_CAPACITY_ERROR;
  }

  if (entryId == -1) { 
    return SEMTABLE_EXISTING_DATASET_LOCKED;
  }

  *retEntryId=entryId;

  int ret = accquireLockAndSemaphore(dsn_member, username, entryId);
  if (ret != SEMTABLE_SUCCESS) {
    semTable[entryId].semId = 0;  /* mark as deleted */
  }
  return ret;
}

int semTableDequeue(Dsn_Member *dsn_member, char *username) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG," %s\n", __FUNCTION__);  
    SemTable* entry;
    int entryId = findSemTableEntryByDatasetByUser(dsn_member, username, &entry);
    if (entryId >= N_SEM_TABLE_ENTRIES || !entry) {
      return SEMTABLE_ENTRY_NOT_FOUND; 
    }

    int semaphoreRetcode = wakeSempahore(entry->semId);
    if (semaphoreRetcode == -1 ) {
      return SEMTABLE_SEM_DECREMENT_ERROR;
    }

    return SEMTABLE_SUCCESS;         
}