

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_CRYPTO__
#define __ZOWE_CRYPTO__ 1

#define CRYPTO_SUCCESS          0
#define CRYPTO_GENERAL_FAILURE 12

/** \file
 *  \brief crypto.h is a set of platform-independent encryption and digesting utilities.
 *
 */

/*
  TBD:

  The API here is awkward; the caller is presumed to know the length of 
  the hash delivered by the selected algorithm, and allocate outputDigestBuffer
  as needed for the digestContextFinish() call. Some platforms (Windows, Linux)
  store the length of the buffer in DigestContest.outputHashLength, but AFTER
  the call to digestContextFinish.

  FWIW: The lengths of the currently-supported algorithms are:

  MD5  - 16 bytes
  SHA1 - 20 bytes
 */

#ifdef __ZOWE_OS_ZOS

#include "icsf.h"

typedef int CryptoDigestType;

#define CRYPTO_DIGEST_MD5  ICSF_DIGEST_MD5
#define CRYPTO_DIGEST_SHA1 ICSF_DIGEST_SHA1

#elif defined(__ZOWE_OS_WINDOWS)

#include <bcrypt.h>


typedef LPCWSTR CryptoDigestType;

#define CRYPTO_DIGEST_MD5  BCRYPT_MD5_ALGORITHM
#define CRYPTO_DIGEST_SHA1 BCRYPT_SHA1_ALGORITHM

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)

#include <openssl/md5.h>
#include <openssl/sha.h>

/*
  As much as I would like to use the mhash library, it's LGPL, and
  I don't want to fight that battle. SO - be cheesy, and just support
  MD5 and SHA1, which I can easily get from openssl.
 */
typedef int CryptoDigestType;

#define CRYPTO_DIGEST_MD5      1
#define CRYPTO_DIGEST_SHA1     2

#else
#error Unknown OS
#endif

typedef struct DigestContext_tag {
#ifdef __ZOWE_OS_ZOS
  ICSFDigest digest;
#elif defined(__ZOWE_OS_WINDOWS)
  BCRYPT_ALG_HANDLE       hAlgorithm;
  BCRYPT_HASH_HANDLE      hHash;
  PBYTE                   pbHashObject;
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  CryptoDigestType digestType;
  union {
    MD5_CTX md5;
    SHA_CTX sha;
  } state;
#else
#error Unknown OS
#endif
  int outputHashLength;
} DigestContext;

#if defined(__cplusplus)                                                        
extern "C" {                                                                    
#endif                                                                          

int digestContextInit(DigestContext *context, CryptoDigestType digestType);
int digestContextUpdate(DigestContext *context, char *data, int length);
int digestContextFinish(DigestContext *context, char *outputDigestBuffer);

#if defined(__cplusplus)               /* end of extern "C" for C++  */         
}                                                                               
#endif                                                                          

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

