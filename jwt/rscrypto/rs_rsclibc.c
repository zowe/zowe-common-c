/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include "rs_rsclibc.h"

#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>


/* LE mode */

unsigned long long rsclibc_currentSecondsSinceJan1970(void)
{
#ifdef __ZOWE_OS_ZOS
  return time64(NULL);
#else
  return time(NULL);
#endif
}

int rsclibc_currentSecondsAndMicrosSince1970(unsigned int *out_seconds,
                                              unsigned int *out_micros)
{
#ifdef DEBUG_FIXED_TIME_AND_RANDOM 
*out_seconds = 1442599995;
*out_micros = 1234;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *out_seconds = (unsigned int) (tv.tv_sec);
  *out_micros = (unsigned int) (tv.tv_usec);
#endif
  return 0;
}

char *rscDuplicateString(const char *s){
  int len = (int) strlen(s);
  char *copy = (char*)rscAlloc(len+1,"rscDuplicateString");
  memcpy(copy,s,len);
  copy[len] = 0; 
  return copy;
}

void *rscAlloc(size_t size, const char *label){
#ifdef DEBUG
  printf("RSCALLOC: %s, size=0x%x \n",label,(int)size);
#endif
#ifdef METTLE
  void *data = safeMalloc(size, label);
#else
  void *data = calloc(1, size);
#endif
#ifdef DEBUG
  printf("RSCALLOC: yielded 0x%x\n",(int)data);
#endif
  if (data == NULL){
    printf("*** WARNING *** failed alloc for label %s\n",label);
  }
  return data;
}

void rscFree(void *data, size_t size){
  memset(data, 0x00, size);
#ifdef METTLE
  safeFree(data, size);
#else
  free(data);
#endif
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
