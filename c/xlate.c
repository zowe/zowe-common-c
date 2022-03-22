

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/string.h>
#include <metal/ctype.h>
#else
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "xlate.h"

typedef struct {
  const char* name_;
  const unsigned char* table_;
} TableDefinition;

#include "xlate_tables.c"

static TableDefinition tables[] = {
  {"ibm1047_to_iso88591", ibm1047_to_iso88591},
  {"iso88591_to_ibm1047", iso88591_to_ibm1047},
  {"ibm037_to_iso88591", ibm037_to_iso88591},
  {"iso88591_to_ibm037", iso88591_to_ibm037},
  {"e2atable", ebcdic_to_ascii},
  {"a2etable", ascii_to_ebcdic},
  {0,0}
};

const void* getTranslationTable(const char* name)
{
  char nameCpy[32] = {0};
  int nameLen = 0;
  
  nameLen = strlen(name);
  if (nameLen > 31)
  {
    return 0;
  }

  for (int i = 0; i < nameLen; i++)
  {
    nameCpy[i] = tolower(name[i]);
  }

  TableDefinition* td = tables;
  while (td->name_ != 0) {
    if (0 == strcmp(nameCpy, td->name_)) {
      return td->table_;
    }
    ++td;
  }
  return 0;
}

char* e2a(char *buffer, int len)
{
  translate(buffer, ebcdic_to_ascii, (uint32_t)len);
  return buffer;
}

char* a2e(char *buffer, int len)
{
  translate(buffer, ascii_to_ebcdic, (uint32_t)len);
  return buffer;
}

/** A Charset oracle is a string digester that can guess charsets
    based upon what it is fed.   It's heuristic, so YMMV.
*/


CharsetOracle *makeCharsetOracle(void){
  CharsetOracle *oracle = (CharsetOracle*)safeMalloc(sizeof(CharsetOracle),"CharsetOracle");
  memset(oracle,0,sizeof(CharsetOracle));
  return oracle;
}

void freeCharsetOracle(CharsetOracle *oracle){
  safeFree((char*)oracle,sizeof(CharsetOracle));
}

void charsetOracleDigest(CharsetOracle *oracle, char *s, int len){
  for (int i=0; i<len; i++){
    char c = s[i];
    oracle->rangeBuckets[(c&0xF0)>>4]++;
    oracle->charBuckets[c&0xff]++;
  }
  oracle->total += len;
}

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

int guessCharset(CharsetOracle *oracle, double *confidence){
  /* should also take variety of character */
  int *buckets = oracle->rangeBuckets;
  int sub20 = buckets[0] + buckets[1];
  int twenties = buckets[2];
  int thirties = buckets[3];
  int range40To7F = buckets[4] + buckets[5] + buckets[6] + buckets[7];
  int low = sub20 + twenties + thirties + range40To7F;
  int total = oracle->total;
  int range80ToFF = total-low;
  
  double lengthConfidence = min(1.0,((double)(oracle->total-50))/50);
  *confidence = lengthConfidence;
  if (range40To7F > range80ToFF){
    return CHARSET_ORACLE_UTF_FAMILY;
  } else {
    return CHARSET_ORACLE_EBCDIC_FAMILY;
  }
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

