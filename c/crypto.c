

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE 
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>  
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#endif /* METTLE */

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "crypto.h"

#ifdef __ZOWE_OS_WINDOWS
#include <windows.h>

#pragma comment(lib, "bcrypt.lib")

#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)

#define STATUS_UNSUCCESSFUL         ((NTSTATUS)0xC0000001L)



/*
  ICSFDigest digest;

  int keyLength = strlen(key);
  int guidLength = strlen(wsGUID);
  char *asciiBuffer = SLHAlloc(slh,keyLength+guidLength);
  char *hash = SLHAlloc(slh,20);

  memcpy(asciiBuffer,key,keyLength);
  memcpy(asciiBuffer+keyLength,wsGUID,guidLength);
  e2a(asciiBuffer,keyLength+guidLength);
  
  icsfDigestInit(&digest,ICSF_DIGEST_SHA1);
  icsfDigestUpdate(&digest,asciiBuffer,keyLength+guidLength);

  icsfDigestFinish(&digest,hash);
*/



int digestContextInit(DigestContext *context, CryptoDigestType digestType){
  BCRYPT_ALG_HANDLE       hAlgorithm      = NULL;
  BCRYPT_HASH_HANDLE      hHash           = NULL;
  NTSTATUS                status          = STATUS_UNSUCCESSFUL;
  DWORD                   cbData          = 0,
    cbHash          = 0,
    cbHashObject    = 0;
  /* make sure the digest is totally reset */
  memset(context,0,sizeof(DigestContext));
  

  /* open an algorithm handle */
  if(!NT_SUCCESS(status = BCryptOpenAlgorithmProvider(&hAlgorithm,
                                                      digestType,
                                                      NULL,
                                                      0))){
    wprintf(L"**** Error 0x%x returned by BCryptOpenAlgorithmProvider\n", status);
    return CRYPTO_GENERAL_FAILURE;
  }

  printf("halgorithm handle=0x%x\n",hAlgorithm);
  fflush(stdout);
  /* calculate the size of the buffer to hold the hash object 
     
     BCryptGetProperty (and its mysterious parameters)

     NTSTATUS WINAPI BCryptGetProperty(
       _In_  BCRYPT_HANDLE hObject,      // algorithm object, etc
       _In_  LPCWSTR       pszProperty,  // null-termed unicode prop string
       _Out_ PUCHAR        pbOutput,     // buffer for output
       _In_  ULONG         cbOutput,     // size of pbOutput
       _Out_ ULONG         *pcbResult,   // actual length filled in pbOutput
       _In_  ULONG         dwFlags       // usually zero
     );

     
   */
  if(!NT_SUCCESS(status = BCryptGetProperty(hAlgorithm, 
                                            BCRYPT_OBJECT_LENGTH, 
                                            (PBYTE)&cbHashObject, 
                                            sizeof(DWORD), 
                                            &cbData, 
                                            0))){
    wprintf(L"**** Error 0x%x returned by BCryptGetProperty\n", status);
    return CRYPTO_GENERAL_FAILURE;
  }
  printf("cbHashObject=%d cbData=%d\n",cbHashObject,cbData);
  fflush(stdout);

  /* allocate the hash object on the heap */
  context->pbHashObject = (PBYTE)HeapAlloc (GetProcessHeap (), 0, cbHashObject);
  if(NULL == context->pbHashObject){
    wprintf(L"**** memory allocation failed\n");
    return CRYPTO_GENERAL_FAILURE;
  }

  /* calculate the length of the hash */
  if(!NT_SUCCESS(status = BCryptGetProperty(hAlgorithm, 
                                            BCRYPT_HASH_LENGTH, 
                                            (PBYTE)&cbHash, 
                                            sizeof(DWORD), 
                                            &cbData, 
                                            0))){
    wprintf(L"**** Error 0x%x returned by BCryptGetProperty\n", status);
    if(context->pbHashObject){
      HeapFree(GetProcessHeap(), 0, context->pbHashObject);
    }
    return CRYPTO_GENERAL_FAILURE;
  }
  printf("gotHashLength %d\n",(int)cbHash);
  fflush(stdout);
  context->outputHashLength = (int)cbHash;

  /* create a hash */
  if(!NT_SUCCESS(status = BCryptCreateHash(hAlgorithm, 
                                           &hHash, 
                                           context->pbHashObject, 
                                           cbHashObject, 
                                           NULL, 
                                           0, 
                                           0))){
    wprintf(L"**** Error 0x%x returned by BCryptCreateHash\n", status);
    if(context->pbHashObject){
      HeapFree(GetProcessHeap(), 0, context->pbHashObject);
    }
    return CRYPTO_GENERAL_FAILURE;
  }


  context->hAlgorithm = hAlgorithm;
  context->hHash = hHash;
  return CRYPTO_SUCCESS;
}
    
int digestContextUpdate(DigestContext *context, char *data, int length){
  if(!NT_SUCCESS(BCryptHashData(context->hHash,data,length,0))){
    return CRYPTO_GENERAL_FAILURE;
  } else {
    return CRYPTO_SUCCESS;
  }
}

int digestContextFinish(DigestContext *context, char *outputDigestBuffer){
  /* close the hash */
  if(!NT_SUCCESS(BCryptFinishHash(context->hHash, 
                                  outputDigestBuffer,
                                  context->outputHashLength,
                                  0))){
    return CRYPTO_GENERAL_FAILURE;
  } else {

    /* cleanup all the stuff */
    if(context->hAlgorithm){
      BCryptCloseAlgorithmProvider(context->hAlgorithm,0);
    }
    if (context->hHash){
      BCryptDestroyHash(context->hHash);
    }
    if (context->pbHashObject){
      HeapFree(GetProcessHeap(), 0, context->pbHashObject);
    }
    return CRYPTO_SUCCESS;
  }
}

#elif defined(__ZOWE_OS_ZOS)

int digestContextInit(DigestContext *context, CryptoDigestType digestType){
  return icsfDigestInit(&(context->digest),(int)digestType);
}

int digestContextUpdate(DigestContext *context, char *data, int length){
  return icsfDigestUpdate(&(context->digest),data,length);
}

int digestContextFinish(DigestContext *context, char *outputDigestBuffer){
  return icsfDigestFinish(&(context->digest),outputDigestBuffer);
}



#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)

#include <openssl/md5.h>
#include <openssl/sha.h>

int digestContextInit(DigestContext *context, CryptoDigestType digestType){
  memset(context, 0, sizeof(DigestContext));
  context->digestType = digestType;
  int result = CRYPTO_GENERAL_FAILURE;
  switch(digestType) {
  case CRYPTO_DIGEST_MD5:
    context->outputHashLength = MD5_DIGEST_LENGTH;
    result = MD5_Init(&(context->state.md5)) ? CRYPTO_SUCCESS : result;
    break;

  case CRYPTO_DIGEST_SHA1:
    context->outputHashLength = SHA_DIGEST_LENGTH;
    result = SHA1_Init(&(context->state.sha)) ? CRYPTO_SUCCESS : result;
    break;

  default:
    break;
  }
  return result;
}

int digestContextUpdate(DigestContext *context, char *data, int length){
  int result = CRYPTO_GENERAL_FAILURE;
  switch(context->digestType) {
  case CRYPTO_DIGEST_MD5:
    result = MD5_Update(&(context->state.md5), data, (unsigned long) length)
      ? CRYPTO_SUCCESS : result;
    break;

  case CRYPTO_DIGEST_SHA1:
    result = SHA1_Update(&(context->state.sha), data, (unsigned long) length)
      ? CRYPTO_SUCCESS : result;
    break;

  default:
    break;
  }
  return result;
}

int digestContextFinish(DigestContext *context, char *outputDigestBuffer){
  int result = CRYPTO_GENERAL_FAILURE;
  switch(context->digestType) {
  case CRYPTO_DIGEST_MD5:
    result = MD5_Final((unsigned char*) outputDigestBuffer, &(context->state.md5))
      ? CRYPTO_SUCCESS : result;
    break;

  case CRYPTO_DIGEST_SHA1:
    result = SHA1_Final((unsigned char*) outputDigestBuffer, &(context->state.sha))
      ? CRYPTO_SUCCESS : result;
    break;

  default:
    break;
  }
  return result;
}

#else

#error Unknown OS

#endif /* the whole file is an OS-cased thing */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

