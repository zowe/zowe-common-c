/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * test_collections.c - Unit tests for collections.h (hashtable, ArrayList, etc.)
 *
 * Build on z/OS: use the Makefile in this directory
 *   cd tests/unit && make test_collections && ./test_collections
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "collections.h"

#include "zowe_test.h"

/* ========================================================================
 *  Hashtable tests (string keys)
 * ======================================================================== */

void test_ht_create_destroy(void) {
  hashtable *ht = htCreate(17, stringHash, stringCompare, NULL, NULL);
  ASSERT_PTR_NOT_NULL(ht);
  ASSERT_INT_EQ(0, htCount(ht));
  htDestroy(ht);
}

void test_ht_put_and_get(void) {
  hashtable *ht = htCreate(17, stringHash, stringCompare, NULL, NULL);

  htPut(ht, "alpha", "one");
  htPut(ht, "beta",  "two");
  htPut(ht, "gamma", "three");

  ASSERT_STR_EQ("one",   (char *)htGet(ht, "alpha"));
  ASSERT_STR_EQ("two",   (char *)htGet(ht, "beta"));
  ASSERT_STR_EQ("three", (char *)htGet(ht, "gamma"));
  ASSERT_INT_EQ(3, htCount(ht));

  htDestroy(ht);
}

void test_ht_get_missing_key(void) {
  hashtable *ht = htCreate(17, stringHash, stringCompare, NULL, NULL);
  htPut(ht, "key1", "val1");
  ASSERT_PTR_NULL(htGet(ht, "nonexistent"));
  htDestroy(ht);
}

void test_ht_put_overwrite(void) {
  hashtable *ht = htCreate(17, stringHash, stringCompare, NULL, NULL);
  htPut(ht, "key", "value1");
  ASSERT_STR_EQ("value1", (char *)htGet(ht, "key"));

  int rc = htPut(ht, "key", "value2");
  ASSERT_INT_EQ(1, rc);  /* 1 means replaced */
  ASSERT_STR_EQ("value2", (char *)htGet(ht, "key"));
  ASSERT_INT_EQ(1, htCount(ht));

  htDestroy(ht);
}

void test_ht_remove(void) {
  hashtable *ht = htCreate(17, stringHash, stringCompare, NULL, NULL);
  htPut(ht, "x", "y");
  ASSERT_INT_EQ(1, htCount(ht));

  int removed = htRemove(ht, "x");
  ASSERT_INT_EQ(1, removed);
  ASSERT_PTR_NULL(htGet(ht, "x"));
  ASSERT_INT_EQ(0, htCount(ht));

  /* removing non-existent key */
  removed = htRemove(ht, "x");
  ASSERT_INT_EQ(0, removed);

  htDestroy(ht);
}

void test_ht_many_entries(void) {
  hashtable *ht = htCreate(7, stringHash, stringCompare, NULL, NULL);
  char keys[50][16];
  char vals[50][16];

  for (int i = 0; i < 50; i++) {
    sprintf(keys[i], "key_%d", i);
    sprintf(vals[i], "val_%d", i);
    htPut(ht, keys[i], vals[i]);
  }

  ASSERT_INT_EQ(50, htCount(ht));

  for (int i = 0; i < 50; i++) {
    char *got = (char *)htGet(ht, keys[i]);
    ASSERT_PTR_NOT_NULL(got);
    ASSERT_STR_EQ(vals[i], got);
  }

  htDestroy(ht);
}

static int mapCallCount = 0;
static void countVisitor(void *key, void *value) {
  mapCallCount++;
}

void test_ht_map(void) {
  hashtable *ht = htCreate(17, stringHash, stringCompare, NULL, NULL);
  htPut(ht, "a", "1");
  htPut(ht, "b", "2");
  htPut(ht, "c", "3");

  mapCallCount = 0;
  htMap(ht, countVisitor);
  ASSERT_INT_EQ(3, mapCallCount);

  htDestroy(ht);
}

/* ========================================================================
 *  Hashtable tests (integer keys)
 * ======================================================================== */

void test_ht_int_put_get(void) {
  hashtable *ht = htCreate(17, NULL, NULL, NULL, NULL);

  htIntPut(ht, 100, "hundred");
  htIntPut(ht, 200, "two hundred");
  htIntPut(ht, 0,   "zero");

  ASSERT_STR_EQ("hundred",     (char *)htIntGet(ht, 100));
  ASSERT_STR_EQ("two hundred", (char *)htIntGet(ht, 200));
  ASSERT_STR_EQ("zero",        (char *)htIntGet(ht, 0));
  ASSERT_PTR_NULL(htIntGet(ht, 999));

  htDestroy(ht);
}

/* ========================================================================
 *  stringHash / stringCompare tests
 * ======================================================================== */

void test_string_hash_deterministic(void) {
  int h1 = stringHash("hello");
  int h2 = stringHash("hello");
  ASSERT_INT_EQ(h1, h2);
  ASSERT_TRUE(h1 >= 0);
}

void test_string_hash_different_strings(void) {
  int h1 = stringHash("abc");
  int h2 = stringHash("xyz");
  /* It's theoretically possible they collide, but extremely unlikely */
  ASSERT_TRUE(h1 != h2);
}

void test_string_compare(void) {
  ASSERT_TRUE(stringCompare("hello", "hello"));
  ASSERT_FALSE(stringCompare("hello", "world"));
  ASSERT_FALSE(stringCompare("abc", "abcd"));
}

/* ========================================================================
 *  ArrayList tests
 * ======================================================================== */

void test_arraylist_create(void) {
  ArrayList *list = makeArrayList();
  ASSERT_PTR_NOT_NULL(list);
  ASSERT_INT_EQ(0, list->size);
  arrayListFree(list);
}

void test_arraylist_add_and_get(void) {
  ArrayList *list = makeArrayList();

  arrayListAdd(list, "first");
  arrayListAdd(list, "second");
  arrayListAdd(list, "third");

  ASSERT_INT_EQ(3, list->size);
  ASSERT_STR_EQ("first",  (char *)arrayListElement(list, 0));
  ASSERT_STR_EQ("second", (char *)arrayListElement(list, 1));
  ASSERT_STR_EQ("third",  (char *)arrayListElement(list, 2));

  arrayListFree(list);
}

void test_arraylist_out_of_bounds(void) {
  ArrayList *list = makeArrayList();
  arrayListAdd(list, "only");

  ASSERT_PTR_NULL(arrayListElement(list, 1));
  ASSERT_PTR_NULL(arrayListElement(list, 100));

  arrayListFree(list);
}

void test_arraylist_grow_beyond_initial_capacity(void) {
  ArrayList *list = makeArrayList();

  /* Initial capacity is 8; add more than 8 to force a resize */
  for (int i = 0; i < 20; i++) {
    arrayListAdd(list, "item");
  }

  ASSERT_INT_EQ(20, list->size);
  ASSERT_TRUE(list->capacity >= 20);
  ASSERT_STR_EQ("item", (char *)arrayListElement(list, 19));

  arrayListFree(list);
}

static int intPtrCompare(const void *a, const void *b) {
  int va = *(int *)*(void **)a;
  int vb = *(int *)*(void **)b;
  return va - vb;
}

void test_arraylist_sort(void) {
  ArrayList *list = makeArrayList();
  int vals[] = {30, 10, 20, 50, 40};

  for (int i = 0; i < 5; i++) {
    arrayListAdd(list, &vals[i]);
  }

  arrayListSort(list, intPtrCompare);

  ASSERT_INT_EQ(10, *(int *)arrayListElement(list, 0));
  ASSERT_INT_EQ(20, *(int *)arrayListElement(list, 1));
  ASSERT_INT_EQ(30, *(int *)arrayListElement(list, 2));
  ASSERT_INT_EQ(40, *(int *)arrayListElement(list, 3));
  ASSERT_INT_EQ(50, *(int *)arrayListElement(list, 4));

  arrayListFree(list);
}

/* ========================================================================
 *  FixedBlockMgr tests
 * ======================================================================== */

void test_fbmgr_alloc_free(void) {
  fixedBlockMgr *mgr = fbMgrCreate(64, 10, NULL);
  ASSERT_PTR_NOT_NULL(mgr);

  void *block1 = fbMgrAlloc(mgr);
  ASSERT_PTR_NOT_NULL(block1);

  void *block2 = fbMgrAlloc(mgr);
  ASSERT_PTR_NOT_NULL(block2);
  ASSERT_TRUE(block1 != block2);

  fbMgrFree(mgr, block1);
  /* After freeing, next alloc should reuse the freed block */
  void *block3 = fbMgrAlloc(mgr);
  ASSERT_TRUE(block3 == block1);

  fbMgrDestroy(mgr);
}

void test_fbmgr_null_returns_null(void) {
  ASSERT_PTR_NULL(fbMgrAlloc(NULL));
}

/* ========================================================================
 *  ShortLivedHeap tests
 * ======================================================================== */

void test_slh_create_alloc_free(void) {
  ShortLivedHeap *slh = makeShortLivedHeap(4096, 10);
  ASSERT_PTR_NOT_NULL(slh);

  char *mem = SLHAlloc(slh, 100);
  ASSERT_PTR_NOT_NULL(mem);

  /* Allocated memory should be zero-initialized (SLH allocates from safeMalloc'd blocks) */
  /* Write into it to confirm it works */
  memset(mem, 0xAA, 100);

  char *mem2 = SLHAlloc(slh, 500);
  ASSERT_PTR_NOT_NULL(mem2);
  ASSERT_TRUE(mem != mem2);

  SLHFree(slh);
}

/* ========================================================================
 *  StringList tests
 * ======================================================================== */

void test_stringlist_basic(void) {
  ShortLivedHeap *slh = makeShortLivedHeap(4096, 10);
  StringList *list = makeStringList(slh);
  ASSERT_PTR_NOT_NULL(list);
  ASSERT_INT_EQ(0, stringListLength(list));

  addToStringList(list, "hello");
  addToStringList(list, "world");

  ASSERT_INT_EQ(2, stringListLength(list));
  ASSERT_TRUE(stringListContains(list, "hello"));
  ASSERT_TRUE(stringListContains(list, "world"));
  ASSERT_FALSE(stringListContains(list, "missing"));

  SLHFree(slh);
}

void test_stringlist_unique(void) {
  ShortLivedHeap *slh = makeShortLivedHeap(4096, 10);
  StringList *list = makeStringList(slh);

  addToStringListUnique(list, "abc");
  addToStringListUnique(list, "abc");
  addToStringListUnique(list, "def");

  ASSERT_INT_EQ(2, stringListLength(list));

  SLHFree(slh);
}

/* ========================================================================
 *  CharStream tests
 * ======================================================================== */

void test_charstream_buffer(void) {
  char *data = "Hello!";
  CharStream *cs = makeBufferCharStream(data, 6, 0);
  ASSERT_PTR_NOT_NULL(cs);

  ASSERT_FALSE(charStreamEOF(cs));
  ASSERT_INT_EQ('H', charStreamGet(cs, 0));
  ASSERT_INT_EQ('e', charStreamGet(cs, 0));
  ASSERT_INT_EQ('l', charStreamGet(cs, 0));
  ASSERT_INT_EQ('l', charStreamGet(cs, 0));
  ASSERT_INT_EQ('o', charStreamGet(cs, 0));
  ASSERT_INT_EQ('!', charStreamGet(cs, 0));
  ASSERT_TRUE(charStreamEOF(cs));

  charStreamClose(cs);
  charStreamFree(cs);
}

/* ========================================================================
 *  main
 * ======================================================================== */

int main(void) {
  TEST_SUITE_START("collections");

  /* Hashtable - string keys */
  RUN_TEST(test_ht_create_destroy);
  RUN_TEST(test_ht_put_and_get);
  RUN_TEST(test_ht_get_missing_key);
  RUN_TEST(test_ht_put_overwrite);
  RUN_TEST(test_ht_remove);
  RUN_TEST(test_ht_many_entries);
  RUN_TEST(test_ht_map);

  /* Hashtable - int keys */
  RUN_TEST(test_ht_int_put_get);

  /* stringHash / stringCompare */
  RUN_TEST(test_string_hash_deterministic);
  RUN_TEST(test_string_hash_different_strings);
  RUN_TEST(test_string_compare);

  /* ArrayList */
  RUN_TEST(test_arraylist_create);
  RUN_TEST(test_arraylist_add_and_get);
  RUN_TEST(test_arraylist_out_of_bounds);
  RUN_TEST(test_arraylist_grow_beyond_initial_capacity);
  RUN_TEST(test_arraylist_sort);

  /* FixedBlockMgr */
  RUN_TEST(test_fbmgr_alloc_free);
  RUN_TEST(test_fbmgr_null_returns_null);

  /* ShortLivedHeap */
  RUN_TEST(test_slh_create_alloc_free);

  /* StringList */
  RUN_TEST(test_stringlist_basic);
  RUN_TEST(test_stringlist_unique);

  /* CharStream */
  RUN_TEST(test_charstream_buffer);

  TEST_SUITE_END();
  return TEST_SUITE_RC();
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
