/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#error Metal C not supported
#endif // METTLE

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "collections.h"
#include "storage.h"

#define STOARGE_LOCK_ERROR   (STORAGE_FIRST_CUSTOM_STATUS + 0)
#define STOARGE_UNLOCK_ERROR (STORAGE_FIRST_CUSTOM_STATUS + 1)
#define STORAGE_ALLOC_ERROR  (STORAGE_FIRST_CUSTOM_STATUS + 2)

typedef struct MemStorage_tag {
  hashtable *table;      // map: key -> value
  pthread_mutex_t lock;  // lock to protect hashtable
} MemStorage;

static char *memStorageAlloc(int size) {
  char *data = safeMalloc31(size + 4, "mem storage");
  if (!data) {
    return NULL;
  }
  *((int *)data) = size;
  return (data + 4);
}

static void memStorageFree(char *mem) {
  if (!mem) {
    return;
  }
  char *prefix = mem - 4;
  int allocSize = ((int *)prefix)[0];
  safeFree31(prefix, allocSize);
}

static void freeMemStorage(void *data) {
  memStorageFree(data);
}

static MemStorage *makeMemStorage() {
  MemStorage *storage = (MemStorage*)safeMalloc(sizeof(*storage), "MemStorage");
  if (!storage) {
    return NULL;
  }
  hashtable *table = htCreate(257, stringHash, stringCompare, freeMemStorage, freeMemStorage);
  if (!table) {
    safeFree((char*)storage, sizeof(*storage));
    return NULL;
  }
  storage->table = table;
  int rc = pthread_mutex_init(&storage->lock, NULL);
  if (rc) {
    safeFree((char*)storage, sizeof(*storage));
    htDestroy(table);
  }
  return storage;
}

static int memStorageLock(MemStorage *storage) {
  int rc = pthread_mutex_lock(&storage->lock);
  if (rc) {
    return STOARGE_LOCK_ERROR;
  }
  return STORAGE_OK;
}

static int memStorageUnlock(MemStorage *storage) {
  int rc = pthread_mutex_unlock(&storage->lock);
  if (rc) {
    return STOARGE_UNLOCK_ERROR;
  }
  return STORAGE_OK;
}

static char *memStorageCopyString(MemStorage *storage, const char *str) {
  const int len = strlen(str);
  char *newString = memStorageAlloc(len + 1);
  if (!newString) {
    return NULL;
  }
  memset(newString, '\0', len + 1);
  memcpy(newString, str, len);
  return newString;
}

static void memStorageSetString(MemStorage *storage, const char *key, const char *value, int *statusOut) {
  const char *keyCopy = memStorageCopyString(storage, key);
  const char *valueCopy = memStorageCopyString(storage, value);
  if (!keyCopy || !valueCopy) {
    if (keyCopy) {
      memStorageFree((char*)keyCopy);
    }
    if (valueCopy) {
      memStorageFree((char*)valueCopy);
    }
    *statusOut = STORAGE_ALLOC_ERROR;
    return;
  }
  int status = memStorageLock(storage);
  if (status) {
    *statusOut = status;
    return;
  }
  htPut(storage->table, (void*)keyCopy, (void*)valueCopy);
  status = memStorageUnlock(storage);
  *statusOut = status;
}

static const char *memStorageGetString(MemStorage *storage, const char *key, int *statusOut) {
  int status = memStorageLock(storage);
  if (status) {
    *statusOut = status;
    return NULL;
  }
  const char *value = htGet(storage->table, (void*)key);
  bool found = (value != NULL);
  status = memStorageUnlock(storage);
  *statusOut = !found ? STORAGE_KEY_NOT_FOUND : status;
  return value;
}

static void memStorageRemove(MemStorage *storage, const char *key, int *statusOut) {
  int status = memStorageLock(storage);
  if (status) {
    *statusOut = status;
    return;
  }
  int removed = htRemove(storage->table, (void*)key);
  status = memStorageUnlock(storage);
  *statusOut = !removed ? STORAGE_KEY_NOT_FOUND : status;
}

static const char *MESSAGES[] = {
  [STOARGE_LOCK_ERROR] = "Failed to lock storage",
  [STOARGE_UNLOCK_ERROR] = "Failed to unlock storage",
  [STORAGE_ALLOC_ERROR] = "Failed to allocate memory",
};

#define MESSAGE_COUNT sizeof(MESSAGES)/sizeof(MESSAGES[0])

static const char *memStorageGetStrStatus(MemStorage *storage, int status) {
  int adjustedStatus = status - STORAGE_FIRST_CUSTOM_STATUS;
  if (adjustedStatus >= MESSAGE_COUNT || adjustedStatus < 0) {
    return "Unknown status code";
  }
  const char *message = MESSAGES[adjustedStatus];
  if (!message) {
    return "Unknown status code";
  }
  return message;
}

Storage *makeMemoryStorage() {
  Storage *storage = (Storage*)safeMalloc(sizeof(*storage), "Storage");
  if (!storage) {
    return NULL;
  }
  MemStorage *memStorage = makeMemStorage();
  if (!memStorage) {
    safeFree((char*)storage, sizeof(*storage));
    return NULL;
  }
  storage->userData = memStorage;
  storage->set = (StorageSet) memStorageSetString; 
  storage->get = (StorageGet) memStorageGetString; 
  storage->remove = (StorageRemove) memStorageRemove;
  storage->strStatus = (StorageGetStrStatus) memStorageGetStrStatus;
  return storage;
}

#ifdef _TEST

/*

Build:

c89 \
  -D_TEST=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -Wc,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list\(\),so\(\),off,xref \
  -I ../h \
  -o ./storage \
  alloc.c \
  collections.c \
  storage.c \
  storage_mem.c \
  utils.c \
  timeutls.c 

Run: ./storage

*/

int main(int argc, char *argv[]) {
  int status = 0;
  Storage *storage = makeMemoryStorage();
  if (!storage) {
    printf ("failed to create storage\n");
    exit(1);
  }
  const char *keyA = "keyA";
  const char *sVal = "string-value";
  storageSetString(storage, keyA, sVal, &status);
  printf ("set str ['%s'] = '%s', status %d - %s\n", keyA, sVal, status, storageGetStrStatus(storage, status));
  
  const char *keyB = "keyB";
  int iVal = 123;
  storageSetInt(storage, keyB, iVal, &status);
  printf ("set int ['%s'] = %d, status %d - %s\n", keyB, iVal, status, storageGetStrStatus(storage, status));
  
  const char *keyC = "keyC";
  bool bVal = false;
  storageSetBool(storage, keyC, bVal, &status);
  printf ("set bool ['%s'] = %s, status %d - %s\n", keyC, bVal ? "true" : "false", status, storageGetStrStatus(storage, status));

  const char *keyD = "keyD";
  double dVal = 123.456;
  storageSetDouble(storage, keyD, dVal, &status);
  printf ("set double ['%s'] = %f, status %d - %s\n", keyD, dVal, status, storageGetStrStatus(storage, status));

  sVal = storageGetString(storage, keyA, &status);
  printf ("get str ['%s'] = '%s', status %d - %s\n", keyA, sVal, status, storageGetStrStatus(storage, status));
  
  iVal = storageGetInt(storage, keyB, &status);
  printf ("get int ['%s'] = %d, status %d - %s\n", keyB, iVal, status, storageGetStrStatus(storage, status));
  
  iVal = storageGetInt(storage, keyA, &status);
  printf ("get bad int ['%s'] = %d, status %d - %s\n", keyA, iVal, status, storageGetStrStatus(storage, status));
  
  const char *badKey = "badKey";
  iVal =  storageGetInt(storage, badKey, &status);
  printf ("get int ['%s'] = %d, status %d - %s\n", badKey, iVal, status, storageGetStrStatus(storage, status));
  
  bVal =  storageGetBool(storage, keyC, &status);
  printf ("get bool ['%s'] = %s, status %d - %s\n", keyC, bVal ? "true" : "false", status, storageGetStrStatus(storage, status));

  dVal =  storageGetDouble(storage, keyD, &status);
  printf ("get double ['%s'] = %f, status %d - %s\n", keyD, dVal, status, storageGetStrStatus(storage, status));

  dVal = storageGetDouble(storage, keyA, &status);
  printf ("get bad double ['%s'] = %f, status %d - %s\n", keyA, dVal, status, storageGetStrStatus(storage, status));
  
  storageRemove(storage, keyC, &status);
  printf ("remove '%s', status %d - %s\n", keyC, status, storageGetStrStatus(storage, status));
  
  bVal = storageGetBool(storage, keyC, &status);
  printf ("get bool ['%s'] = %s, status %d - %s\n", keyC, bVal ? "true" : "false", status, storageGetStrStatus(storage, status));
  
  storageRemove(storage, keyC, &status);
  printf ("remove '%s', status %d - %s\n", keyC, status, storageGetStrStatus(storage, status));
}
#endif // _TEST

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/