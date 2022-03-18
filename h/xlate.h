

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef _ZOWE_XLATE_H_
#define _ZOWE_XLATE_H_

#include "zowetypes.h"

/*
  Encoding conversion between a few commonly used single-byte character sets. 
  For other conversions, use convertCharset in charset.h.
 */

/*
  General conversion function, using a 256-byte translation table.
 */
#if !defined(METTLE) && !defined(__ZOWE_OS_AIX)
static inline
#else
static
#endif
void translate(void* target, const void* table, uint32_t length)
{
  unsigned char* tgt = (unsigned char*) target;
  const unsigned char* tab = (const unsigned char*) table;
  for (uint32_t i = 0; i < length; ++i) {
    tgt[i] = tab[tgt[i]];
  }
}

/*
  Locate one of the built-in translation tables. The available
  tables are:

  ibm1047_to_iso88591 / iso88591_to_ibm1047
     These are the character sets supported by z/OS enhanced ASCII support. 
     Fully symmetric.

  ibm37_to_iso88591 / iso88591_to_ibm37
      IBM37 is similar to IBM1047 in that there is a fully symmetric mapping
      between it and ISO-8859-1; the handling of the undefined portions of
      ISO-8859-1 is different.

  e2aTable / a2eTable
      The historic COMMON translation tables. This is most similar to the
      IBM37/ISO-8859-1 mapping, but NOT fully symmetric.

  The names are case-insensitive.
 */

const void* getTranslationTable(const char* name);

/*
  Functions used elsewhere in COMMON, using the e2a/a2e tables
 */

#ifdef __cplusplus 
extern "C"
{
#endif /* __cplusplus */

char* e2a(char *buffer, int len);
char* a2e(char *buffer, int len);
  
typedef struct CharsetOracle_tag {
  int total;
  int charBuckets[0x100];
  int rangeBuckets[0x10];
} CharsetOracle;

#define CHARSET_ORACLE_EBCDIC_FAMILY  0x10000
#define CHARSET_ORACLE_UTF_FAMILY     0x20000

CharsetOracle *makeCharsetOracle(void);

void freeCharsetOracle(CharsetOracle *oracle);
void charsetOracleDigest(CharsetOracle *oracle, char *s, int len);
int guessCharset(CharsetOracle *oracle, double *confidence);

#ifdef __cplusplus 
}
#endif /* __cplusplus */
#endif /*  _ZOWE_XLATE_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

