

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#pragma options(inline(noauto,noreport,,),optimize(3))
#endif

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/string.h>
#include <metal/stdlib.h>
#include "zowetypes.h"
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zowetypes.h"

#ifdef __ZOWE_OS_WINDOWS
#include <windows.h>
#endif /* windows case */

#endif

#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "collections.h"

fixedBlockMgr *fbMgrCreate(int blockSize, int blocksPerExtend,
                           void *owner){
  fixedBlockMgr *mgr = (fixedBlockMgr*)safeMalloc(sizeof(fixedBlockMgr),"FixedBlockManager");

  mgr->owner = owner;
  /* intf("fbMgrCreateCalled bSize %d owner %x\n",
          blockSize,mgr->owner); */
  mgr->allExtents = NULL;
  mgr->freeList = NULL;
  mgr->currentBlock = NULL;
  mgr->blockSize = blockSize;
  mgr->blocksPerExtend = blocksPerExtend;
  mgr->blocksRemaining = 0;
  return mgr;
}

#define FB_HEADER_LENGTH 8

/* Needs external Monitor/Synchronization */
void *fbMgrAlloc(fixedBlockMgr *mgr){
  void *result = NULL;

  if (mgr == NULL){
    return NULL;
  }
  if (mgr->freeList != NULL){
    result = mgr->freeList;
    mgr->freeList = *((void**)result);
    return result;
  } else{
    if (mgr->blocksRemaining == 0){
      mgr->currentBlock
        = (void*)safeMalloc(mgr->blockSize*mgr->blocksPerExtend+FB_HEADER_LENGTH,"FixedBlockExtend");
      /* printf("fbmanager currentBlock %x\n",mgr->currentBlock); */
      mgr->blocksRemaining = mgr->blocksPerExtend;
      *((char**)mgr->currentBlock) = mgr->allExtents; /* extend the cleanup chain */
      mgr->allExtents = mgr->currentBlock;
      mgr->currentBlock += FB_HEADER_LENGTH;
    }
    result = (void*)(mgr->currentBlock);
    mgr->currentBlock += mgr->blockSize;
    mgr->blocksRemaining -= 1;
    return result;
  }
}

/* Needs external Monitor synchronization */
void fbMgrFree(fixedBlockMgr *mgr, void *block){
  void **freeElement = (void**)block;
  *freeElement = mgr->freeList;
  mgr->freeList = block;
}

void fbMgrDestroy(fixedBlockMgr *mgr){
  char *extent = mgr->allExtents;
  while (extent){
    char *nextExtent = *((char**)extent);
    safeFree(extent,mgr->blockSize*mgr->blocksPerExtend+FB_HEADER_LENGTH);
    extent = nextExtent;
  }
  safeFree((char*)mgr,sizeof(fixedBlockMgr));
}

/* If the hash function supplied is NULL,
     the "natural" hash function (int) is used.
   If the compare function is NULL, the natural comparison "==" is used
   If the keyReclaimer is NULL,
     the hashtable takes no responsibility to manage keys
   Same for the valueReclaimer
   */

hashtable *htCreate(int backboneSize,
                    int (*hash)(void *key),
                    int (*compare)(void *key1, void *key2),
                    void (*keyReclaimer)(void *key),
                    void (*valueReclaimer)(void *value)){
  hashtable *ht = (hashtable*)safeMalloc(sizeof(hashtable),"HashTable");
  int i;

  ht->mgr = fbMgrCreate(sizeof(hashentry),256,ht);
  ht->backboneSize = backboneSize;
  ht->backbone = (hashentry**)safeMalloc(sizeof(hashentry*)*backboneSize,"HT Backbone");
  ht->hashFunction = hash;
  ht->comparator = compare;
  ht->keyReclaimer = keyReclaimer;
  ht->valueReclaimer = valueReclaimer;
  ht->keepEntry = NULL;
  ht->entryExpired = NULL;
  ht->removeEntry = NULL;
  ht->eyecatcher[0] = 'H';
  ht->eyecatcher[1] = 'T';
  ht->eyecatcher[2] = 'B';
  ht->eyecatcher[3] = 'L';
  for (i=0; i<backboneSize; i++){
    ht->backbone[i] = NULL;
  }
  return ht;
}

void htAlter(hashtable *ht,
             int (*keepEntry)(void *value),              // Determines what to do if a duplicate key is found
             int (*entryExpired)(void *value),           // Determines if a value is still valid
             int (*removeEntry)(void *value),            // Determines if a entry should be removed if found
             void *v1,void *v2,void *v3){                // Reserved for future functions
  ht->keepEntry = keepEntry;
  ht->entryExpired = entryExpired;
  ht->removeEntry = removeEntry;
  return;
}

static void htDequeue(hashtable *ht, int place, hashentry *prev, hashentry *entry, int valueReclaim){
  if (prev == NULL){
    ht->backbone[place] = entry->next;
  } else{
    prev->next = entry->next;
  }
  if (ht->keyReclaimer != NULL){
    (ht->keyReclaimer)(entry->key);
  }
  entry->key = NULL;
  if (valueReclaim && ht->valueReclaimer != NULL){
    (ht->valueReclaimer)(entry->value);
  }
  entry->value = NULL;

  fbMgrFree(ht->mgr,entry);

  return;
}

void *htGet(hashtable *ht, void *key){
  int hashcode = ht->hashFunction ? (ht->hashFunction)(key) : INT_FROM_POINTER(key);
  hashcode = hashcode & 0x7FFFFFFF;
  int place = hashcode%(ht->backboneSize);
  hashentry *prev = NULL;
  hashentry *hashentry = ht->backbone[place];
  int loopCount = 0;

  if (hashentry != NULL){
    int (*compare)() = ht->comparator;
    while (hashentry != NULL){
      if (compare?
          (compare)(hashentry->key,key) :
          hashentry->key == key){
        void *value = hashentry->value;
        if (ht->entryExpired != NULL && (ht->entryExpired)(value)){
          htDequeue(ht,place,prev,hashentry,TRUE);  /* Entry is no longer valid */
          return NULL;
        }
        if (ht->removeEntry != NULL && (ht->removeEntry)(value)){
          htDequeue(ht,place,prev,hashentry,FALSE);  /* Delete entry from table */
        }
        return value;
      } else{
        prev = hashentry;
        hashentry = hashentry->next;
      }
      loopCount++;
    }
    return NULL;
  } else{
    return NULL;
  }
}

void *htIntGet(hashtable *ht, int key){
  int hashcode = key;
  hashcode = hashcode & 0x7FFFFFFF;
  int place = hashcode%(ht->backboneSize);
  /* printf("place = %d ht->backbone=0x%x, key=0x%x\n",place,ht->backbone,key); */
  hashentry *prev = NULL;
  hashentry *hashentry = ht->backbone[place];
  int loopCount = 0;

  /* printf("hashentry(top)->key=0x%x hashentry=0x%x\n",hashentry->key,hashentry); */
  if (hashentry != NULL){
    while (hashentry != NULL){
      /* printf("hashentry(loop)->key=0x%x\n",hashentry->key); */
      if (INT_FROM_POINTER(hashentry->key) == key){
        void *value = hashentry->value;
        if (ht->entryExpired != NULL && !(ht->entryExpired)(value)){
          htDequeue(ht,place,prev,hashentry,TRUE);  /* Entry is no longer valid */
          return NULL;
        }
        if (ht->removeEntry != NULL && (ht->removeEntry)(value)){
          htDequeue(ht,place,prev,hashentry,FALSE);  /* Delete entry from table */
        }
        return value;
      } else{
        prev = hashentry;
        hashentry = hashentry->next;
      }
      loopCount++;
    }
    return NULL;
  } else{
    return NULL;
  }
}

void *htUIntGet(hashtable *ht, unsigned int key){
  unsigned int hashcode = key;
  int place = (int)(hashcode%(ht->backboneSize));
  /* printf("place = %d ht->backbone=0x%x, key=0x%x\n",place,ht->backbone,key); */
  hashentry *prev = NULL;
  hashentry* hashentry = ht->backbone[place];
  int loopCount = 0;

  /* printf("hashentry(top)->key=0x%x hashentry=0x%x\n",hashentry->key,hashentry); */
  if (hashentry != NULL){
    while (hashentry != NULL){
      /* printf("hashentry(loop)->key=0x%x\n",hashentry->key); */
      if (UINT_FROM_POINTER(hashentry->key) == key){
        void *value = hashentry->value;
        if (ht->entryExpired != NULL && (ht->entryExpired)(value)){
          htDequeue(ht,place,prev,hashentry,TRUE);  /* Entry is no longer valid */
          return NULL;
        }
        if (ht->removeEntry != NULL && (ht->removeEntry)(value)){
          htDequeue(ht,place,prev,hashentry,FALSE);  /* Delete entry from table */
        }
        return value;
      } else{
        prev = hashentry;
        hashentry = hashentry->next;
      }
      loopCount++;
    }
    return NULL;
  } else{
    return NULL;
  }
}

/* allow up to 12-bytes keys under the storage
   management of the hashtable */

static void *htAllocSmallObject(hashtable *ht){
  return fbMgrAlloc(ht->mgr);
}

static void htFreeSmallObject(hashtable *ht, void *key){
  fbMgrFree(ht->mgr,key);
}

/* put returns non-zero if it replaces an old value
 */

static int htPut2(hashtable *ht, void *key, void *value){
  int hashcode =
    (ht->hashFunction != NULL) ? (ht->hashFunction)(key) : INT_FROM_POINTER(key);
  hashcode = hashcode & 0x7FFFFFFF;
  int place = hashcode%(ht->backboneSize);
  hashentry *entry = entry = ht->backbone[place];
  hashentry *tail = NULL;
  hashentry *newEntry = NULL;
  int (*compare)() = ht->comparator;

  if (entry != NULL){
    while (entry != NULL){
      if (compare ? (compare)(entry->key,key) : (entry->key == key)){
        if (ht->keepEntry != NULL && (ht->keepEntry)(entry->value))
          return 2;
        if (ht->keyReclaimer != NULL){
          (ht->keyReclaimer)(entry->key);
        }
        if (ht->valueReclaimer != NULL){
          (ht->valueReclaimer)(entry->value);
        }
        entry->key = key;
        entry->value = value;
        return 1;
      } else{
        entry = entry->next;
      }
    }
    tail = ht->backbone[place];
  }

  newEntry = (hashentry*)fbMgrAlloc(ht->mgr);
  newEntry->key = key;
  newEntry->value = value;
  newEntry->next = tail;
  ht->backbone[place] = newEntry;
  return 0;
}

static int htIntPut2(hashtable *ht, int key, void *value){
  int hashcode = key;
  hashcode = hashcode & 0x7FFFFFFF;
  int place = hashcode%(ht->backboneSize);
  hashentry *entry = entry = ht->backbone[place];
  hashentry *tail = NULL;
  hashentry *newEntry = NULL;

/*printf("htIntPut-ing table: %p, key: %p, value: %p\n", ht, (int) key, (int) value);*/
  if (entry != NULL){
    while (entry != NULL){
      if (INT_FROM_POINTER(entry->key) == key){
        if (ht->keepEntry != NULL && (ht->keepEntry)(entry->value))
          return 2;
        if (ht->keyReclaimer != NULL){
          (ht->keyReclaimer)(entry->key);
        }
        if (ht->valueReclaimer != NULL){
          (ht->valueReclaimer)(entry->value);
        }
        entry->key = POINTER_FROM_INT(key);
        entry->value = value;
#ifdef DEBUG
        /*printf("XXX DANGER replaced existing value at address: %p\n", value);*/
#endif
        return 1;
      } else{
        entry = entry->next;
      }
    }
    tail = ht->backbone[place];
  }

  newEntry = (hashentry*)fbMgrAlloc(ht->mgr);
  newEntry->key = POINTER_FROM_INT(key);
  newEntry->value = value;
  newEntry->next = tail;
  ht->backbone[place] = newEntry;
  return 0;
}

int htUIntPut(hashtable *ht, unsigned int key, void *value){
  unsigned int hashcode = key;
  int place = (int)(hashcode%(ht->backboneSize));
  hashentry *entry = entry = ht->backbone[place];
  hashentry *tail = NULL;
  hashentry *newEntry = NULL;

  if (entry != NULL){
    while (entry != NULL){
      if (UINT_FROM_POINTER(entry->key) == key){
        if (ht->keepEntry != NULL && (ht->keepEntry)(entry->value))
          return 2;
        if (ht->keyReclaimer != NULL){
          (ht->keyReclaimer)(entry->key);
        }
        if (ht->valueReclaimer != NULL){
          (ht->valueReclaimer)(entry->value);
        }
        entry->key = POINTER_FROM_UINT(key);
        entry->value = value;
        return 1;
      } else{
        entry = entry->next;
      }
    }
    tail = ht->backbone[place];
  }

  newEntry = (hashentry*)fbMgrAlloc(ht->mgr);
  newEntry->key = POINTER_FROM_UINT(key);
  newEntry->value = value;
  newEntry->next = tail;
  ht->backbone[place] = newEntry;
  return 0;
}

int htPut(hashtable *ht, void *key, void *value){
  return htPut2(ht,key,value);
}

int htIntPut(hashtable *ht, int key, void *value){
  return htIntPut2(ht,key,value);
}


/* for every item in the hashtable for which matcher returns non-zero,
 * remove that entry from the hashtable and call destroyer on it.
 * Returns the number of entries pruned. */
int htPrune(hashtable *ht,
            int (*matcher)(void *userData, void *key, void *value),
            void (*destroyer)(void *userData, void *value),
            void *userData)
{
  int i;
  int numpruned = 0;
  hashentry *prev = NULL;
  hashentry *entry = NULL;
  hashentry *tmpEntry = NULL;

  for (i=0; i < ht->backboneSize; i++)
  {
    hashentry *entry = ht->backbone[i];
    prev = NULL; /* whenever we move to a new backbone slot, init prev */

    if (entry != NULL) {
      while (entry != NULL) {
        if (matcher(userData, entry->key, entry->value))
        {
          void *tmpValue = entry->value;

          /* remove from list */
          if (prev == NULL) {
            ht->backbone[i] = entry->next;
          } else {
            prev->next = entry->next; /* list maint */
          }

          if (ht->keyReclaimer != NULL){
            (ht->keyReclaimer)(entry->key);
          }
          entry->key = NULL;
          entry->value = NULL;
          if (destroyer) {
            destroyer(userData, tmpValue);
          }

          tmpEntry = entry->next;
          fbMgrFree(ht->mgr,entry);
          numpruned++;
          entry = tmpEntry; /* loop maint */
        } else {
          /* didn't match */
          prev = entry;
          entry = entry->next;
        }
      } /* non-NULL entry */
    } /* non-NULL backbone slot */
  } /* loop over backbone */

  return numpruned;
}

/* remove returns non-zero if it really removes anything
 */
int htRemove(hashtable *ht, void *key){
  int hashcode = ht->hashFunction ? (ht->hashFunction)(key) : INT_FROM_POINTER(key);
  hashcode = hashcode & 0x7FFFFFFF;
  hashentry *prev = NULL;
  int place = hashcode%(ht->backboneSize);
  hashentry *entry = ht->backbone[place];
  int (*compare)() = ht->comparator;
  if (entry != NULL){
    while (entry != NULL){
      if (compare ? (compare)(entry->key,key) : (entry->key == key)){
        htDequeue(ht,place,prev,entry,TRUE);
        return 1;
      } else{
        prev = entry;
        entry = entry->next;
      }
    } /* non-NULL entry in list */
  } /* non-NULL entry at place */
  return 0;
}

int stringHash(void *key){
  char *s = (char*)key;
  int hash = 0;
  int i;
  int len = strlen(s);

  for (i=0; i<len; i++){
    hash = (hash << 4) + s[i];
  }
  return hash&0x7fffffff;
}


void htDump(hashtable *ht){
  int i;

  printf("ht backboneSize=%d\n",ht->backboneSize);
  fflush(stdout);
  int isString = (ht->hashFunction == stringHash);
  for (i=0; i<ht->backboneSize; i++){
    hashentry *entry = ht->backbone[i];
    if (entry != NULL){
      printf("in slot %d: entry=%x\n",i,entry);
      fflush(stdout);
      while (entry != NULL){
      if (isString){
       printf("  key=%x '%s' value: %x\n",entry->key,entry->key,entry->value);
      } else{
       printf("  key=%x value: %x\n",entry->key,entry->value);
      }
      fflush(stdout);
        entry = entry->next;
      }
    }
  }
}

int htCount(hashtable *ht){
  int i;
  int count = 0;

  for (i=0; i<ht->backboneSize; i++){
    hashentry *entry = ht->backbone[i];
    if (entry != NULL){
      /* printf("backbone place = %d\n",i); */
      while (entry != NULL){
        count++;
        entry = entry->next;
      }
    }
    /* print hash distribution statistics
         = 0
         > 0
         high
         low and > 0
         average and > 0
         average
         std deviation
    */
  }
  return count;
}


void htMap(hashtable *ht, void (*visitor)(void *key, void *value)){
  int i;

  for (i=0; i<ht->backboneSize; i++){
    hashentry *entry = ht->backbone[i];
    if (entry != NULL){
      /* printf("backbone place = %d\n",i); */
      while (entry != NULL){
        (visitor)(entry->key,entry->value);
        entry = entry->next;
      }
    }
  }
}

void htMap2(hashtable *ht, void (*visitor)(void *userData, void *key, void *value), void *userData){
  int i;

  for (i=0; i<ht->backboneSize; i++){
    hashentry *entry = ht->backbone[i];
    if (entry != NULL){
      while (entry != NULL){
        (visitor)(userData,entry->key,entry->value);
        entry = entry->next;
      }
    }
  }
}

int stringCompare(void *key1, void *key2){
  char *s1 = (char*)key1;
  char *s2 = (char*)key2;

  return !strcmp(s1,s2);
}

void htDestroy(hashtable *ht){

  if (ht->keyReclaimer != NULL || ht->valueReclaimer != NULL) {

    for (int i = 0; i < ht->backboneSize; i++) {
      hashentry *entry = ht->backbone[i];
      while (entry != NULL) {
        if (ht->keyReclaimer != NULL) {
          (ht->keyReclaimer)(entry->key);
        }
        if (ht->valueReclaimer != NULL) {
          (ht->valueReclaimer)(entry->value);
        }
        entry = entry->next;
      }
    }

  }

  fbMgrDestroy(ht->mgr);
  safeFree((char*)ht->backbone,sizeof(hashentry*)*ht->backboneSize);
  safeFree((char*)ht,sizeof(hashtable));
}



static int digestHash(void *key){
  char *digest = (char*)key;
  int hash = 0;
  int i;
  for (i=0; i<LRU_DIGEST_LENGTH; i+=sizeof(int)){
    hash = hash ^ *((int*)(digest+i));
  }
  return (hash & 0x7FFFFFFF);
}

static int digestCompare(void *k1, void *k2){
  char *digest1 = (char*)k1;
  char *digest2 = (char*)k2;

  return (memcmp(k1,k2,LRU_DIGEST_LENGTH) == 0);
}


LongHashtable *lhtCreate(int backboneSize,
                         void (*valueReclaimer)(void *value)){
  LongHashtable *lht = (LongHashtable*)safeMalloc(sizeof(LongHashtable),"LongHashtable");
  int i;

  lht->mgr = fbMgrCreate(sizeof(LongHashEntry),256,lht);
  lht->backboneSize = backboneSize;
  lht->backbone = (LongHashEntry**)safeMalloc(sizeof(LongHashEntry*)*backboneSize,"LHT Backbone");
  lht->valueReclaimer = valueReclaimer;
  lht->keepEntry = NULL;
  lht->entryExpired = NULL;
  lht->removeEntry = NULL;
  lht->eyecatcher[0] = 'L';
  lht->eyecatcher[1] = 'H';
  lht->eyecatcher[2] = 'T';
  lht->eyecatcher[3] = 'B';
  for (i=0; i<backboneSize; i++){
    lht->backbone[i] = NULL;
  }
  return lht;
}

void lhtAlter(LongHashtable *lht,
              int (*keepEntry)(void *value),              // Determines what to do if a duplicate key is found
              int (*entryExpired)(void *value),           // Determines if a value is still valid
              int (*removeEntry)(void *value),            // Determines if a entry should be removed if found
              void *v1,void *v2,void *v3){                // Reserved for future functions
  lht->keepEntry = keepEntry;
  lht->entryExpired = entryExpired;
  lht->removeEntry = removeEntry;
  return;
}

static void lhtDequeue(LongHashtable *lht, int place, LongHashEntry *prev, LongHashEntry *entry, int valueReclaim){
  if (prev == NULL){
    lht->backbone[place] = entry->next;
  } else{
    prev->next = entry->next;
  }
  entry->key = 0l;
  if (valueReclaim && lht->valueReclaimer != NULL){
    (lht->valueReclaimer)(entry->value);
  }
  entry->value = NULL;

  fbMgrFree(lht->mgr,entry);
  return;
}

void lhtDestroy(LongHashtable *lht){
  fbMgrDestroy(lht->mgr);
  safeFree((char *)lht->backbone, sizeof(LongHashEntry *) *lht->backboneSize);
  safeFree((char *)lht, sizeof(LongHashtable));
}

void *lhtGet(LongHashtable *lht, int64 key){
  int place = key%(lht->backboneSize);
  LongHashEntry *prev = NULL;
  LongHashEntry* hashentry = lht->backbone[place];
  int loopCount = 0;

  if (hashentry != NULL){
    while (hashentry != NULL){
      if (hashentry->key == key){
        void *value = hashentry->value;
        if (lht->entryExpired != NULL && (lht->entryExpired)(value)){
          lhtDequeue(lht,place,prev,hashentry,TRUE);  /* Entry is no longer valid */
          return NULL;
        }
        if (lht->removeEntry != NULL && (lht->removeEntry)(value)){
          lhtDequeue(lht,place,prev,hashentry,FALSE);  /* Delete entry from table */
        }
        return value;
      } else{
        prev = hashentry;
        hashentry = hashentry->next;
      }
      loopCount++;
    }
    return NULL;
  } else{
    return NULL;
  }
}

/* returns whether replaced */
int lhtPut(LongHashtable *ht, int64 key, void *value){
  int place = key%(ht->backboneSize);
  LongHashEntry *entry = entry = ht->backbone[place];
  LongHashEntry *tail = NULL;
  LongHashEntry *newEntry = NULL;

  if (entry != NULL){
    while (entry != NULL){
      if (entry->key == key){
        if (ht->keepEntry != NULL && (ht->keepEntry)(entry->value))
          return 2;
        if (ht->valueReclaimer != NULL){
          (ht->valueReclaimer)(entry->value);
        }
        entry->key = key;
        entry->value = value;
        return 1;
      } else{
        entry = entry->next;
      }
    }
    tail = ht->backbone[place];
  }

  newEntry = (LongHashEntry*)fbMgrAlloc(ht->mgr);
  newEntry->key = key;
  newEntry->value = value;
  newEntry->next = tail;
  ht->backbone[place] = newEntry;
  return 0;
}

/* remove returns non-zero if it really removes anything
 */
int lhtRemove(LongHashtable *ht, int64 key){
  int place = key%(ht->backboneSize);
  LongHashEntry *prev = NULL;
  LongHashEntry *entry = entry = ht->backbone[place];
  if (entry != NULL){
    while (entry != NULL){
      if (entry->key == key){
        lhtDequeue(ht,place,prev,entry,TRUE);
        return 1;
      } else{
        prev = entry;
        entry = entry->next;
      }
    } /* non-NULL entry in list */
  } /* non-NULL entry at place */
  return 0;
}

void lhtMap(LongHashtable *ht, void (*visitor)(void *, int64, void *), void *userData){
  int i;

  for (i=0; i<ht->backboneSize; i++){
    LongHashEntry *entry = ht->backbone[i];
    if (entry != NULL){
      while (entry != NULL){
        (visitor)(userData,entry->key,entry->value);
        entry = entry->next;
      }
    }
  }
}


static void lruHashVisitor(void *key, void *value){
  LRUElement *element = (LRUElement*)value;
  printf("    %16.16s: 0x%x containing 0x%x\n",key,value,element->data);
}

LRUCache *makeLRUCache(int size){
  LRUCache *cache = (LRUCache*)safeMalloc(sizeof(LRUCache),"LRU Cache");
  cache->size = size;
  cache->count = 0;
  cache->oldest = NULL;
  cache->newest = NULL;
  cache->ht = htCreate(257,digestHash,digestCompare,NULL,NULL);
  return cache;
}

static void lruCacheDestroyVisitor(void *key, void *value){
  LRUElement *element = (LRUElement*)value;
  safeFree((char *)element, sizeof(LRUElement));
}

void destroyLRUCache(LRUCache *cache){
  htMap(cache->ht, lruCacheDestroyVisitor);
  htDestroy(cache->ht);
  safeFree((char *)cache, sizeof(LRUCache));
}

void lruDump(LRUCache *cache){
  printf("LRU Cache size=%d count=%d\n",cache->size,cache->count);
  LRUElement *element = cache->newest;
  printf("  Newest To Oldest:\n");
  while (element){
    printf("    Elt: %16.16s -> 0x%x\n",element->digest,element->data);
    element = element->older;
  }
  element = cache->oldest;
  printf("  Oldest To Newest:\n");
  while (element){
    printf("    Elt: %16.16s -> 0x%x\n",element->digest,element->data);
    element = element->newer;
  }
  printf("  In Hash Order:\n");
  htMap(cache->ht,lruHashVisitor);
}

/* can change recency */
void *lruGet(LRUCache *cache, char *digest){
  if (cache->trace){
    printf("LRU Get digest: 0x%x\n",digest);
    dumpbuffer(digest,16);
  }
  LRUElement *element = (LRUElement*)htGet(cache->ht,digest);

  if (element){
    return element->data;
  } else{
    return NULL;
  }
}

static LRUElement *allocLRUElement(LRUCache *cache){
  if (cache->count < cache->size){
    LRUElement *element = (LRUElement*)safeMalloc(sizeof(LRUElement),"LRU Element");
    cache->count++;
    return element;
  } else{
    return NULL; /* should not happen */
  }
}

/* returns element that falls out of cache if any:

   digest is copied, never stored
   thing poitner is stored and will be returned if de-cached

   */
void *lruStore(LRUCache *cache, char *digest, void *thing){
  if (cache->trace){
    printf("LRU Store digest: 0x%x\n",digest);
    dumpbuffer(digest,16);
  }

  hashtable *ht = cache->ht;
  LRUElement *existingElement = htGet(ht,digest);
  if (cache->trace){
    printf("lruStore existing = 0x%x\n",existingElement);
  }
  if (existingElement){
    if (existingElement == cache->newest){
      if (cache->trace){
        printf("recaching newest: no work to do\n");
      }
      return NULL;
    } else if (existingElement == cache->oldest){
      LRUElement *newOldest = cache->oldest->newer;
      if (cache->trace){
        printf("recaching oldest: rotate to front, newOldest = 0x%x\n",newOldest);
      }

      existingElement->newer = NULL;
      if (cache->count == 2){ /* special flipping case */
        newOldest->newer = existingElement;
      } else{
        cache->newest->newer = existingElement;
      }
      existingElement->older = cache->newest;
      newOldest->older = NULL;
      cache->oldest = newOldest;
      cache->newest = existingElement;
    } else{ /* found something that wasn't oldest or newest */
      LRUElement *prev = existingElement->older;
      LRUElement *next = existingElement->newer;
      if (cache->trace){
        printf("recaching middle, prun between older=0x%x and newer 0x%x\n",
        prev->data,next->data);
      }

      existingElement->newer = NULL;
      existingElement->older = cache->newest;
      cache->newest->newer = existingElement;
      cache->newest = existingElement;
      prev->newer = next;
      next->older = prev;
    }
    return NULL;
  } else if (cache->count < cache->size){
    LRUElement *newElement = allocLRUElement(cache);
    memcpy(newElement->digest,digest,LRU_DIGEST_LENGTH);
    newElement->data = thing;
    newElement->newer = NULL;

    if (cache->oldest && cache->newest){
      LRUElement *oldNewest = cache->newest;
      if (cache->trace){
        printf(">1 room case newest->data=0x%x\n",oldNewest->data);
      }
      newElement->older = cache->newest;
      cache->newest = newElement;
      oldNewest->newer = newElement;
    } else{
      if (cache->trace){
        printf("== 0 case, go for it, newElement=0x%x\n",newElement);
      }
      newElement->older = NULL;
      cache->newest = newElement;
      cache->oldest = newElement;
    }
    htPut(ht,newElement->digest,newElement);
    return NULL;
  } else if (cache->size >= 2){ /* count == size && size >= 2 */

    LRUElement *oldOldest = cache->oldest;
    LRUElement *oldNewest  = cache->newest;
    LRUElement *secondOldest = oldOldest->newer;
    LRUElement *recycledElement = (LRUElement*)htGet(ht,oldOldest->digest);
    if (cache->trace){
      printf("recycle case oldOldest=0x%x recycled = 0x%x\n",oldOldest,recycledElement);
    }
    void *decached = recycledElement->data;
    htRemove(ht,oldOldest->digest);
    memcpy(recycledElement->digest,digest,LRU_DIGEST_LENGTH);
    recycledElement->data = thing;
    recycledElement->newer = NULL;
    recycledElement->older = oldNewest;

    oldNewest->newer = recycledElement;
    cache->newest = recycledElement;
    cache->oldest = secondOldest;
    secondOldest->older = NULL;
    htPut(ht,recycledElement->digest,recycledElement);
    return decached;
  } else if (cache->size == 1){ /* count == size && size == 1 */

    LRUElement *recycledElement = cache->oldest;
    if (cache->trace){
      printf("recycle case (size=1) recycled = 0x%x\n",recycledElement);
    }
    void *decached = recycledElement->data;
    htRemove(ht,recycledElement->digest);
    memcpy(recycledElement->digest,digest,LRU_DIGEST_LENGTH);
    recycledElement->data = thing;
    htPut(ht,recycledElement->digest,recycledElement);
    return decached;

  } else{ /* count == size && size == 0 */
    if (cache->trace){
      printf("recycle case (size=0)\n");
    }
    return NULL;
  }
}


/* Thread Safe Queues

   (and on ZOS) Lock-Free Queues


   Windows *might* support lock-free queues:

   "Interlocked Singly Linked Lists"

   https://msdn.microsoft.com/en-us/library/windows/desktop/ms684121(v=vs.85).aspx

 */

#ifdef __ZOWE_OS_ZOS
/* The queue routines are written so that the will work in either AMODE
   31 or 64 using the same code.  To do this the code is dependent on the
   compiler defining pointers and "long" variables as 32 bits in AMODE 31
   and 64 bits in AMODE 64.  The __asm generated code is passed the size of
   one of a long variable and then uses conditional assembly statements to
   generate either 32 bit operand or 64 bit operand instructions and PLO
   operation codes based on the size of the long variable.  The offset of
   the fields in the queue header are also passed to the generated to use
   when loading and storing data into the queue header and elements,
   making the __asm code insensitive to the AMODE.

   Serialization of the queue is performed using either-

    - Constrained Transactional Execution
    - Perform-Locked-Operation (PLO)

   Flag PSATXC is tested to determine if the constrained Transactional
   Execution facility can be used or not.  If it can be, the counter in the
   queue header is set to -1 to indicate to the insert and remove routines
   to use it.  When incrementing the counter value in the PLO path the
   counter must never be allowed to overflow into the sign bit, making
   the value become negative.  Instead, the value must be wrapped back to
   1.
*/

#define PSATXC ((*(char*)0x240)&0x04)
#endif

Queue *makeQueue(int flags){
  Queue *q = (Queue*) safeMalloc(sizeof(Queue),"Queue");
  memset(q,0,sizeof(Queue));
  memcpy(q->eyecatcher,"LKFREQUE",8);
  q->flags = flags;
#ifdef __ZOWE_OS_ZOS
  if (PSATXC)
    q->counter = -1;         /* Indicate to use Transactional Execution */
  else
    q->counter = 1;          /* Start counter at 1                      */
#endif
  q->head = NULL;
  q->tail = NULL;
#ifdef __ZOWE_OS_WINDOWS
  q->mutex = CreateMutexEx(NULL,NULL,0x00000000,MUTEX_ALL_ACCESS);
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  pthread_mutex_init(&(q->mutex),NULL); /* PTHREAD_MUTEX_INITIALIZER; */
#endif
  return q;
}

void destroyQueue(Queue *q) {
  safeFree((char*) q, sizeof(Queue));
}

#ifdef __ZOWE_OS_ZOS
ZOWE_PRAGMA_PACK

typedef struct CSTSTParms_tag{     /* PLO function 20/22 */
  long long filler00[7];           /* Offset 0x00 -   0  */
  PAD_LONG(x38,long thing1);       /* Offset 0x38 -  56  */
  int  fillerx40;                  /* Offset 0x40 -  64  */
  int  aletThing1;                 /* Offset 0x44 -  68  */
  PAD_LONG(x48,void* thing1Addr);  /* Offset 0x48 -  72  */
  long long filler50;              /* Offset 0x50 -  80  */
  PAD_LONG(x58,long thing2);       /* Offset 0x58 -  88  */
  int  fillerx60;                  /* Offset 0x60 -  96  */
  int  aletThing2;                 /* Offset 0x64 - 100  */
  PAD_LONG(x68,void* thing2Addr);  /* Offset 0x68 - 104  */
  long long fillerx70;             /* Offset 0x70 - 112  */
  PAD_LONG(x78,long thing3);       /* Offset 0x78 - 120  */
  int  fillerx80;                  /* Offset 0x80 - 128  */
  int  aletThing3;                 /* Offset 0x84 - 132  */
  PAD_LONG(x88,void* thing3Addr);  /* Offset 0x88 - 136  */
                                   /* Length 0x90 - 144  */
} CSTSTParms;

ZOWE_PRAGMA_PACK_RESET

#pragma inline(compareAndLoad)
static int compareAndLoad(long *oldCounter, long *counterAddress, long *sourceAddress, long *resultAddress){
  int status;
  __asm(ASM_PREFIX
        "&KGX(2)  SETC    'G'             Set 'G' for index #2\n"
        "&KF      SETA    %5/4            Set 'G'index value\n"
        "&KG      SETC    '&KGX(&KF)'     Set 'G' value\n"
        "&KL      SETC    'L&KG'          Set Load instruction\n"
        "&KST     SETC    'ST&KG'         Set Store instruction\n"
        "&KF      SETA    ((&KF-1)*2)     Set PLO function code\n"
        "&LX      SETA    &LX+1           Generate unique branch labels\n"
        "&L1      SETC    'TEXIT&LX'      \n"
        "         LA      1,0(,%2)        %2 - counterAddress\n"
        "         LA      0,&KF           Function code in R0\n"
        "         &KL     14,0(,%1)       %1 - *oldCounter\n"
        "         PLO     14,0(%2),15,0(%3) %2 - counterAddress, %3 - sourceAddress\n"
        "         &KST    14,0(,%1)       %1 - *oldCounter\n"
        "         &KST    15,0(,%4)       %4 - resultAddress\n"
        "         LA      %0,0            Return 0 for failure\n"
        "         JNZ     &L1             If not equal, failure\n"
        "         LA      %0,1            Return 1 for success\n"
        "&L1      DS      0H              \n"
        :
        "=r"(status), "+r"(oldCounter)
        :
        "r"(counterAddress), "r"(sourceAddress), "r"(resultAddress), "i"(sizeof(oldCounter))
        :
        "r0 r1 r14 r15");
  return status;
}

#pragma inline(compareAndSwapTriple)
static int compareAndSwapTriple(long *oldCounter, long newCounter, long *counterAddress, CSTSTParms *parms){
  int status;
  __asm(ASM_PREFIX
        "&KGX(2)  SETC    'G'             Set 'G' for index #2\n"
        "&KF      SETA    %5/4            Set 'G'index value\n"
        "&KG      SETC    '&KGX(&KF)'     Set 'G' value\n"
        "&KL      SETC    'L&KG'          Set Load instruction\n"
        "&KLR     SETC    'L&KG.R'        Set Load-register instruction\n"
        "&KST     SETC    'ST&KG'         Set Store instruction\n"
        "&KF      SETA    20+((&KF-1)*2)  Set PLO function code\n"
        "&LX      SETA    &LX+1           Generate unique branch labels\n"
        "&L1      SETC    'TEXIT&LX'      \n"
        "         LA      1,0(,%2)        %2 - counterAddress\n"
        "         LA      0,&KF           Function code in R0\n"
        "         &KL     14,0(,%1)       %1 - *oldCounter\n"
        "         &KLR    15,%4           %4 - newCounter\n"
        "         PLO     14,0(%2),0,0(%3) %2 - counterAddress, %3 - parms\n"
        "         &KST    14,0(,%1)       %1 - *oldCounter\n"
        "         LA      %0,0            Return 0 for failure\n"
        "         JNZ     &L1             If not equal, failure\n"
        "         LA      %0,1            Return 1 for success\n"
        "&L1      DS      0H              \n"
        :
        "=r"(status), "+r"(oldCounter)
        :
        "r"(counterAddress), "r"(parms), "r"(newCounter), "i"(sizeof(oldCounter))
        :
        "r0 r1 r14 r15");
  return status;
}

void qEnqueue(Queue *q, QueueElement *newElement) {

  union {
    long long alignit;
    CSTSTParms parms;
  };

  newElement->next = NULL;

  /* Note: The PLO compare value must alway be the first data fetched when
           preparing the PLO request in order for proper serialization of
           the data being updated to occur.  If the compare fails on the
           PLO request the new compare value is returned, just like when a
           Compare-and-Swap compare fails, and that value should be used
           as the compare value for the next attempt.
  */

  long lockCounter = q->counter;

  if (lockCounter < 0)            /* If the counter is negative, use TX    */
  {
    /* Insert a queue element at the tail of a queue using transactional
       execution.

       Because C does not have a built-in TBEGINC construct and the constraints
       placed on the number of instructions, referenced cache lines, and
       branching in a constrained transaction all of the code to add the element
       is done with generated assembler rather than using C statements.  A
       constrained transaction will either complete successfully or fail due to
       an unresolvable program check.
    */
    __asm(ASM_PREFIX
          "&KGX(2)  SETC    'G'             Set 'G' for index #2\n"
          "&KF      SETA    %5/4            Set 'G'index value\n"
          "&KG      SETC    '&KGX(&KF)'     Set 'G' value\n"
          "&KLT     SETC    'LT&KG'         Set Load-test instruction\n"
          "&KST     SETC    'ST&KG'         Set Store instruction\n"
          "&LX      SETA    &LX+1           Generate unique branch labels\n"
          "&L1      SETC    'TEXIT&LX'      \n"
          "&L2      SETC    'TEMPTY&LX'     \n"
          "*        TBEGINC 0,0             TM_BeginC - no registers saved\n"
          "         DC      X'E56100000000' (Use hex opcode to avoid OPTABLE constraint)\n"
          "         &KLT    1,%3(,%1)       .lastElement = q->tail\n"
          "         &KST    %0,%3(,%1)      .q->tail=newElement\n"
          "         JZ      &L1             .if (lastElement)\n"
          "         &KST    %0,%4(,1)       ..lastElement->next=newElement\n"
          "         J       &L2             .else\n"
          "&L1      &KST    %0,%2(,%1)      ..q->head=newElement\n"
          "&L2      DS      0H              \n"
          "*        TEND    ,               TM_End\n"
          "         DC      X'B2F80000'     \n"
          :
          /* No results */
          :
          "r"(newElement), "r"(q), "i"(offsetof(Queue,head)), "i"(offsetof(Queue,tail)), "i"(offsetof(QueueElement,next)), "i"(sizeof(q->counter))
          :
          "r1");
  }
  else
  {
    memset(&parms,0,sizeof(CSTSTParms));

    /* Insert a queue element at the tail of a queue using PLO.

       The PLO compare value must alway be the first data fetched when preparing
       the PLO request in order for proper serialization of the data being
       updated to occur.  If the compare fails on the PLO request the new
       compare value is returned, just like when a Compare-and-Swap compare
       fails, and that value should be used as the compare value for the next
       attempt.
    */

    while (TRUE){
      long newCounter = lockCounter+1;
      if (newCounter < 0)         /* Handle wrap into sign bit  */
        newCounter = 1;           /* Wrap back to 1             */

      void *desiredQHead = (q->head ? q->head : newElement);
      /* HEAD */
      parms.thing1 = (long)desiredQHead;
      parms.thing1Addr = &(q->head);
      /* TAIL */
      parms.thing2 = (long)newElement;
      parms.thing2Addr = &(q->tail);
      /* PENULTIMATE */
      parms.thing3 = (long)newElement;
      QueueElement *last = q->tail;
      parms.thing3Addr = (q->tail ? &(last->next) : &(q->tail));

      if (compareAndSwapTriple(&lockCounter,newCounter,&q->counter,&parms)){
        break;
      }
    }
  }
  return;
}

void qInsert(Queue *q, void *newData) {

  QueueElement *newElement = NULL;
  newElement = (QueueElement *)safeMalloc(sizeof(QueueElement), "Q Element");
  newElement->data = newData;

  qEnqueue(q, newElement);

}

QueueElement *qDequeue(Queue *q) {

  union {
    long long alignit;
    CSTSTParms parms;
  };

  QueueElement *currentHead;

  /* Note: The PLO compare value must alway be the first data fetched when
           preparing the PLO request in order for proper serialization of
           the data being updated to occur.  If the compare fails on the
           PLO request the new compare value is returned, just like when a
           Compare-and-Swap compare fails, and that value should be used
           as the compare value for the next attempt.
  */

  long lockCounter = q->counter;

  if (lockCounter < 0)            /* If the counter is negative, use TX    */
  {
    /* Remove a queue element from the head of a queue using transactional
       execution.

       Because C does not have a built-in TBEGINC construct and the
       constraints placed on the number of instructions, referenced
       cache lines, and branching in a constrained transaction all of
       the code to remove the element is done with generated assembler
       rather than using C statements.  A constrained transaction will
       either complete successfully or fail due to an unresolvable
       program check.
    */
    __asm(ASM_PREFIX
          "&KGX(2)  SETC    'G'             Set 'G' for index #2\n"
          "&KF      SETA    %5/4            Set 'G'index value\n"
          "&KG      SETC    '&KGX(&KF)'     Set 'G' value\n"
          "&KLT     SETC    'LT&KG'         Set Load-test instruction\n"
          "&KST     SETC    'ST&KG'         Set Store instruction\n"
          "&LX      SETA    &LX+1           Generate unique branch labels\n"
          "&L0      SETC    'TSKIPQ&LX'     \n"
          "&L1      SETC    'TEXIT&LX'      \n"
          "&L2      SETC    'TEMPTY&LX'     \n"
          "         &KLT    %0,%2(%1)       if (currentHead=q->head)\n"
          "         JZ      &L0             \n"
          "*        TBEGINC 0,0             .TM_BeginC - no registers saved\n"
          "         DC      X'E56100000000' (Use hex opcode to avoid OPTABLE constraint)\n"
          "         &KLT    %0,%2(%1)       ..if (currentHead=q->head)\n"
          "         JZ      &L1             \n"
          "         &KLT    0,%4(%0)        ...q->head=currentHead->next\n"
          "         &KST    0,%2(%1)        \n"
          "         JNZ     &L2             ...if (!currentHead->next)\n"
          "         &KST    0,%3(%1)        ....q->tail=currentHead->next\n"
          "&L2      LA      0,0             ...currentHead->next = NULL\n"
          "         &KST    0,%4(%0)        \n"
          "&L1      DS      0H              \n"
          "*        TEND    ,               .TM_End\n"
          "         DC      X'B2F80000'     \n"
          "&L0      DS      0H              \n"
          :
          "=r"(currentHead)
          :
          "r"(q), "i"(offsetof(Queue,head)), "i"(offsetof(Queue,tail)), "i"(offsetof(QueueElement,next)), "i"(sizeof(q->counter))
          :
          "r0");

  }
  else
  {
    memset(&parms,0,sizeof(CSTSTParms));

    /* Remove a queue element from the head of a queue using PLO.

       The PLO compare value must alway be the first data fetched when preparing
       the PLO request in order for proper serialization of the data being
       updated to occur.  If the compare fails on the PLO request the new
       compare value is returned, just like when a Compare-and-Swap compare
       fails, and that value should be used as the compare value for the next
       attempt.
    */
    while ((currentHead = q->head) != NULL){

      long newCounter = lockCounter+1;
      if (newCounter < 0)         /* Handle wrap into sign bit  */
        newCounter = 1;           /* Wrap back to 1             */

      void *desiredQHead;         /* Set using compareAndLoad   */
      if (!compareAndLoad(&lockCounter,&q->counter,(long*)&currentHead->next,(long*)&desiredQHead))
        continue;
      void *desiredQTail = (desiredQHead ? q->tail : NULL);

      /* HEAD */
      parms.thing1 = (long)desiredQHead;
      parms.thing1Addr = &(q->head);
      /* TAIL */
      parms.thing2 = (long)desiredQTail;
      parms.thing2Addr = &(q->tail);
      /* CURRENT_HEAD */
   /* parms.thing3 = NULL -- was set to NULL when parmData was initialized */
      QueueElement *last = NULL;

      parms.thing3Addr = &(currentHead->next);

      if (compareAndSwapTriple(&lockCounter,newCounter,&q->counter,&parms))
        break;
    }
  }

  return currentHead;
}

void *qRemove(Queue *q) {

  void *result = NULL;
  QueueElement *element = qDequeue(q);
  if (element) {
    result = element->data;
    safeFree((char *)element, sizeof(QueueElement));
  }

  return result;
}

#elif defined(__ZOWE_OS_WINDOWS) || defined(__ZOWE_OS_LINUX)|| defined(__ZOWE_OS_AIX) /* END OF ZOS SPECIFIC STUFF */

void qInsert(Queue *q, void *newData){
  mutexLock(q->mutex);

  QueueElement *newElement = (QueueElement*)safeMalloc(sizeof(QueueElement),"Q Element");
  newElement->data = newData;
  newElement->next = NULL;
  if (q->head == NULL){
    q->head = newElement;
    q->tail = newElement;
  } else {
    q->tail->next = newElement;
    q->tail = newElement;
  }

  mutexUnlock(q->mutex);
}

void *qRemove(Queue *q){
  mutexLock(q->mutex);
  QueueElement *foundElement = NULL;
  void *foundData = NULL;

  if (q->head == NULL){
    /* do nothing */
  } else if (q->head == q->tail){ /* one-element case */
    QueueElement *foundElement = q->head;
    q->head = NULL;
    q->tail = NULL;
    foundData = foundElement->data;
    safeFree((char*)foundElement,sizeof(QueueElement));
  } else {
    foundElement = q->head;
    q->head = q->head->next;
    foundData = foundElement->data;
    safeFree((char*)foundElement,sizeof(QueueElement));
  }

  mutexUnlock(q->mutex);

  return foundData;
}

#else

/* do POSIX STUFF HERE */

#endif /* END OF OS-VARIANT Queue stuff */



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

