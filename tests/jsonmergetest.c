#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"

/*
  Notes:

  (all work assumed to be done from shell in this directory)

  Windows Build ______________________________


  clang -I./src -I../h -I ../platform/windows -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -o jsonmergetest.exe jsonmergetest.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc


  ZOS Build _________________________________

  


  Running the Test ________________________________

     jsonschematest <overrideJsonFile> <baseJsonFile>

 */

int main(int argc, char *argv[])
{
#ifdef __ZOWE_OS_WINDOWS
  int stdoutFD = _fileno(stdout);
#else
  int stdoutFD = STDOUT_FILENO;
#endif

  if (argc < 4){
    printf("Usage  jsonmergetest <arrayMergePolicy> <overridesJson> <baseJson>\n");
    printf("  ArrayMergePolicy: \n");
    printf("    concat: take all base elements followed by all overrides - len(merge) = len(o)+len(b)\n");
    printf("    merge:  take from both arrays favoring overrides         - len(merge) = max(len(o),len(b))\n");
    printf("    base:   take base array only                             - len(merge) = len(b)\n");
    printf("    overrides: take overrides array only                     - len(merge) = len(a)\n");
  }
  char *arrayPolicy = argv[1];
  char *overridesFile = argv[2];
  char *baseFile = argv[3];
  int flags = 0;
  if (!strcmp(arrayPolicy,"concat")){
    flags = JSON_MERGE_FLAG_CONCATENATE_ARRAYS;
  } else if (!strcmp(arrayPolicy,"merge")){
    flags = JSON_MERGE_FLAG_MERGE_IN_PLACE;
  } else if (!strcmp(arrayPolicy,"base")){
    flags = JSON_MERGE_FLAG_TAKE_BASE;
  } else if (!strcmp(arrayPolicy,"overrides")){
    flags = JSON_MERGE_FLAG_TAKE_OVERRIDES;
  } else {
    printf("unknown array merge policy, '%s'\n",arrayPolicy);
  }
  int errorBufferSize = 1024;
  char *errorBuffer = safeMalloc(errorBufferSize,"ErrorBuffer");
  memset(errorBuffer,0,errorBufferSize);
  
  ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);
  
  Json *base = jsonParseFile2(slh,baseFile,errorBuffer,errorBufferSize);
  if (base == NULL){
    printf("Json parse fail in %s, with error '%s'\n",baseFile,errorBuffer);
    return 12;
  }
  printf("read base\n"); fflush(stdout);
  Json *overrides = jsonParseFile2(slh,overridesFile,errorBuffer,errorBufferSize);
  if (overrides == NULL){
    printf("Json parse fail in %s, with error '%s'\n",overridesFile,errorBuffer);
    return 12;
  }
  printf("read overrides\n"); fflush(stdout);
  jsonPrinter *p = makeJsonPrinter(stdoutFD);
  jsonEnablePrettyPrint(p);
  int status = 0;
  Json *merged = jsonMerge(slh,overrides,base,flags,&status);
  printf("merged\n"); fflush(stdout);
  if (merged){
    jsonPrint(p,merged);
    fflush(stdout);
  } else {
    printf("failed to merge, status = %d\n",status);
  }
  return 0;
}

