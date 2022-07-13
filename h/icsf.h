

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ICSF__
#define __ICSF__

#define ICSF_HASH_CONTEXT_LENGTH 128
#define ICSF_HASH_BLOCK_LENGTH 64

typedef struct ICSFDigest_tag{
  int type;
  int ruleArrayCount;
  char *ruleArray;
  int hashLength;
  int leftoverFill;
  char leftovers[ICSF_HASH_BLOCK_LENGTH];
  char context[ICSF_HASH_CONTEXT_LENGTH];
  char hash[32]; /* big enough for many hash types */
} ICSFDigest;

#define ICSF_DIGEST_MD5  1
#define ICSF_DIGEST_SHA1 2
/* SHA-2 Familt algoithms */
#define ICSF_DIGEST_SHA224 3
#define ICSF_DIGEST_SHA256 4
#define ICSF_DIGEST_SHA384 5
#define ICSF_DIGEST_SHA512 6

#define icsfDigestFully  CFSDGFUL
#define icsfDigestInit   CSFDGINI
#define icsfDigestUpdate CSFDGUPD
#define icsfDigestFinish CSFDGFIN

int icsfRandom(char *data, int len);

int icsfDigestFully(char *digestType, char *s, int len);
int icsfDigestInit(ICSFDigest *digest, int type);
int icsfDigestUpdate(ICSFDigest *digest, char *data, int len);
int icsfDigestFinish(ICSFDigest *digest, char *hash);

int icsfEncipher(const void *key, unsigned int keyLength,
                 const char *text, unsigned int textLength,
                 char *resultBuffer, unsigned int resultBufferLength,
                 int *reasonCode);
int icsfDecipher(const void *key, unsigned int keyLength,
                 const char *text, unsigned int textLength,
                 char *resultBuffer, unsigned int resultBufferLength,
                 int *reasonCode);
int icsfGenerateRandomNumber(void *result, int resultLength, int *reasonCode);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

