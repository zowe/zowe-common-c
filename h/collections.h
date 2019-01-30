

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __CLLCTN__
#define __CLLCTN__ 1

/** \file
 *  \brief collections.h define a set of collection "classes" (structs, really) that are indispensible for programming.
 *
 *  This set include hashtables of various types, fixed block management, lock-free queues, LRU cache managers, etc.
 */

#include "zowetypes.h"

#ifndef __ZOWE_OS_ZOS
#include "openprims.h"
#endif

#ifndef __LONGNAME__
#define fbMgrCreate FBMGRCRT
#define fbMgrAlloc FBMGRALC
#define fbMgrFree  FBMGRFRE
#define fbMgrDestroy FBMGRDST

#define htDestroy HTDSTROY
#define htUIntPut HTUINTPT
#define htUIntGet HTUINTGT
#define htIntGet HTINTGET
#define htIntPut HTINTPUT
#define htDestroy HTDSTROY

#define stringHash STRNGHSH
#define stringCompare STRNGCMP

#define makeLRUCache MKLRUCHE
#define destroyLRUCache DSLRUCHE

#define lhtCreate LNHTCRTE
#define lhtDestroy LNHDSTRY
#define lhtRemove LNHTREMV
#define makeQueue MAKELCFQ
#define destroyQueue DSTRLCFQ

#endif

typedef struct fixedBlockMgr_tag{
  void *owner;
  void *allExtents;
  void *freeList;
  char *currentBlock;
  int blockSize;
  int blocksPerExtend;
  int blocksRemaining;
} fixedBlockMgr;

fixedBlockMgr *fbMgrCreate(int blockSize, int blocksPerExtend,
                           void *owner);
void *fbMgrAlloc(fixedBlockMgr *mgr);
void fbMgrFree(fixedBlockMgr *mgr, void *block);
void fbMgrDestroy(fixedBlockMgr *);


typedef struct hashentry_tag{
  void *key;
  void *value;
  struct hashentry_tag *next;
} hashentry;

/**
 *  \brief a simple, general hashtable that allows general hashing, comparison and memory management.
 *
 *  This hashtable does not rehash or change base vector size.  It resolves collisions with chaining.
 *  Prime Numbers are recommended as backbone sizes.  If the reclaimers are not provided it is assumed that
 *  the memory management of keys and values is external to this hashtable.
 *
 */

typedef struct hashtable_tag{
  char eyecatcher[4];
  int backboneSize;
  hashentry **backbone;
  fixedBlockMgr *mgr;
  int (*hashFunction)(void *key);
  int (*comparator)(void *key1, void *key2);
  void (*keyReclaimer)(void *key);
  void (*valueReclaimer)(void *value);
  int (*keepEntry)(void *value);            // Determines what to do if a duplicate key is found
  int (*entryExpired)(void *value);         // Determines if an entry is still valid
  int (*removeEntry)(void *value);          // Determines if entry should be removed if it is found
} hashtable;

typedef struct LongHashEntry_tag{
  int64 key;
  void *value;
  struct LongHashEntry_tag *next;
} LongHashEntry;

typedef struct LongHashtable_tag{
  char eyecatcher[4];
  int backboneSize;
  LongHashEntry **backbone;
  fixedBlockMgr *mgr;
  void (*valueReclaimer)(void *value);
  int (*keepEntry)(void *value);            // Determines what to do if a duplicate key is found
  int (*entryExpired)(void *value);         // Determines if an entry is still valid
  int (*removeEntry)(void *value);          // Determines if entry should be removed if it is found
} LongHashtable;

LongHashtable *lhtCreate(int backboneSize,
                         void (*valueReclaimer)(void *value));
void lhtAlter(LongHashtable *lht,                         // Add processing functions to the hash table
              int (*keepEntry)(void *value),              // Determines what to do if a duplicate key is found
              int (*entryExpired)(void *value),           // Determines if a value is still valid
              int (*removeEntry)(void *value),            // Determines if a entry should be removed if found
              void *v1,void *v2,void *v3);                // Reserved for future functions
void lhtDestroy(LongHashtable *lht);
void *lhtGet(LongHashtable *lht, int64 key);
int lhtPut(LongHashtable *ht, int64 key, void *value);
int lhtRemove(LongHashtable *ht, int64 key);
void lhtMap(LongHashtable *ht, void (*visitor)(void *userData, int64, void *value), void *userData);

/**
 *  \brief This is the constructor for the hashtable.
 *
 *  \param hash   if provided is expected to produce a positive integer.  Otherwise the last 31 bits of the key pointer are used.
 *  \param compare  if provided is an equality test for keys, otherwise pointer equality is assumed.
 *
 */

hashtable *htCreate(int backboneSize,
                    int (*hash)(void *key),
                    int (*compare)(void *key1, void *key2),
                    void (*keyReclaimer)(void *key),
                    void (*valueReclaimer)(void *value));

/**
 *   \brief  The primary getter for the hashtable.  Returns NULL if key not found.
 */

void htAlter(hashtable *ht,                              // Add processing functions to the hash table
             int (*keepEntry)(void *value),              // Determines what to do if a duplicate key is found
             int (*entryExpired)(void *value),           // Determines if a value is still valid
             int (*removeEntry)(void *value),            // Determines if a entry should be removed if found
             void *v1,void *v2,void *v3);                // Reserved for future functions

void *htGet(hashtable *ht, void *key);
void *htIntGet(hashtable *ht, int key);

/**
 *  \brief  Adds a member to a hashtable.
 *
 *  If this key is replaced, the keyReclaimer (if supplied in constructor) is called.
 */

int htPut(hashtable *ht, void *key, void *value);
int htIntPut(hashtable *ht, int key, void *value);
int htUIntPut(hashtable *ht, unsigned int key, void *value);
void *htUIntGet(hashtable *ht, unsigned int key);
int htRemove(hashtable *ht, void *key);
void htMap(hashtable *ht, void (*visitor)(void *key, void *value));
void htMap2(hashtable *ht, void (*visitor)(void *userData, void *key, void *value), void *userData);
int htPrune(hashtable *ht,
            int (*matcher)(void *userData, void *key, void *value),
            void (*destroyer)(void *userData, void *value),
            void *userData);

/**
 *   \brief This is the destructor for the hashtable.
 *
 *   This will reclaim the memory of the hashtable.   If keyReclaimer and valueReclaimer have been supplied
 *   in the constructor they will be called on each key and value.
 */

void htDestroy(hashtable *ht);
int htCount(hashtable *ht);

/**
 *  \brief   A convenience hash function for the hashtable.
 *
 *  Generates a fairly unique hash value for a C terminated string.
 */

int stringHash(void *key);

/**
 *  \brief   A convenience compare function for the hashtable.
 *
 *  Adapts strcmp to returns TRUE,FALSE values for comparison.
 */

int stringCompare(void *key1, void *key2);

#define LRU_DIGEST_LENGTH 16

typedef struct LRUElement_tag {
  struct LRUElement_tag *newer;
  struct LRUElement_tag *older;
  char digest[LRU_DIGEST_LENGTH];
  void *data;
} LRUElement;

typedef struct LRUCache_tag{
  int size;
  int count;
  LRUElement *oldest;
  LRUElement *newest;
  hashtable *ht;
  int trace;
} LRUCache;


LRUCache *makeLRUCache(int size);
void destroyLRUCache(LRUCache *cache);
void *lruGet(LRUCache *cache, char *digest);
void *lruStore(LRUCache *cache, char *digest, void *thing);

/* Lock-free Queues */

#ifdef __ZOWE_OS_ZOS
ZOWE_PRAGMA_PACK
#endif

typedef struct QueueElement_tag{
  PAD_LONG(x00,void *data);
  PAD_LONG(x08,struct QueueElement_tag *next);
} QueueElement;

#define QUEUE_ALL_BELOW_BAR    0x0000
#define QUEUE_ANCHOR_BELOW_BAR 0x0001
#define QUEUE_LINKS_ABOVE_BAR  0x0002
#define QUEUE_DATA_ABOVE_BAR   0x0004
#define QUEUE_ALL_ABOVE_BAR    0x0007

typedef struct Queue_tag{
  char          eyecatcher[8]; /* LKFREQUE */
  PAD_LONG(x08, long counter);
  int           fillerx10;
  int           flags;
  PAD_LONG(x18, QueueElement *head);
  PAD_LONG(x20, QueueElement *tail);
#ifndef __ZOWE_OS_ZOS
  Mutex         mutex;
#endif
} Queue;

#ifdef __ZOWE_OS_ZOS
ZOWE_PRAGMA_PACK_RESET
#endif

#ifndef QueueAmode64
#define QueueAmode64
#endif
Queue *makeQueue(int flags) QueueAmode64;
void destroyQueue(Queue *q) QueueAmode64;
void qInsert(Queue *q, void *newData) QueueAmode64;
void *qRemove(Queue *q) QueueAmode64;

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

