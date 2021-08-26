/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __IBM_METAL__
#include <string.h>
#include <stdio.h>
#endif

#include "zowetypes.h"
#include "charsets.h"
#include "json.h"
#include "utils.h"
#include "xlate.h"
#include "alloc.h"
#include "timeutls.h"
#include "logging.h"


/* rscrypto */
#include "rs_icsfp11.h"
#include "rs_crypto_errors.h"

#include "jwt.h"

const char *RSJWT_ERROR_DESCRIPTIONS[] = {
  [RC_JWT_OK] = "OK",
  [RC_JWT_MEMORY_ERROR]  = "memory error",
  [RC_JWT_MISSING_PART]  = "missing JWT part",
  [RC_JWT_EXTRA_PART]  = "extra JWT part",
  [RC_JWT_INVALID_ENCODING]  = "invalid encoding",
  [RC_JWT_INVALID_JSON]  = "invalid JSON",
  [RC_JWT_UNKNOWN_ALGORITHM]  = "unknown algorithm",
  [RC_JWT_UNSUPPORTED_ALG]  = "unsupported algorithm",
  [RC_JWT_INVALID_SIGLEN] = "invalid signature length",
  [RC_JWT_CRYPTO_ERROR] = "crypto error",
  [RC_JWT_SIG_MISMATCH] = "signature mismatch",
  [RC_JWT_CONTEXT_ALLOCATION_FAILED] = "context allocation failed",
  [RC_JWT_CRYPTO_TOKEN_NOT_FOUND] = "crypto token not found",
  [RC_JWT_KEY_NOT_FOUND] = "key not found in crypto token",
  [RC_JWT_INSECURE] = "JWT is insecure"
};

#ifdef __ZOWE_EBCDIC
# define JSON_CCSID CCSID_IBM1047
#else
# define JSON_CCSID CCSID_ISO_8859_1
#endif

#define MAX_NPARTS 3
#define BASE64URL_EXTRA_BYTES 2
#define BASE64URL_EXTRA_BYTES_TOTAL (BASE64URL_EXTRA_BYTES * MAX_NPARTS)

#ifdef __ZOWE_EBCDIC
#  define BASE64_IS_EBCDIC 1
#else
#  define BASE64_IS_EBCDIC 0
#endif

#define ASCII_PERIOD 0x2e

static int jwtTrace = FALSE;

int setJwtTrace(int toWhat) {
  int was = jwtTrace;
#ifndef METTLE
  if(toWhat >= ZOWE_LOG_DEBUG){
    jwtTrace = toWhat;
  }
#endif
  return was;
}

/*
 * buf should have extra space for base64url -> base64 conversion
 */
static int extractParts(char base64Buf[], int maxParts,
                        char *dparts[],  int pLen[], char *decodedText) {
  char *tokenizer;
  unsigned int nParts, i = 0;
  char *part;

  for (part = strtok_r(base64Buf, ".", &tokenizer);
      (part != NULL) && (i < maxParts);
      part = strtok_r(NULL, ".", &tokenizer), i++) {
    dparts[i] = part;
    pLen[i] = strlen(part) + 1;
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "found part %s, len %u\n", dparts[i],  pLen[i]);
  }
  if ((part != NULL) && (i == maxParts)) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "error: too many token parts\n");
    return  RC_JWT_EXTRA_PART;
  }
  nParts = i;
  if (nParts > 0) {
    pLen[nParts - 1] += BASE64URL_EXTRA_BYTES;
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "part %d is now %u bytes\n", nParts - 1, pLen[nParts - 1]);
  }
  for (i = nParts - 1; i > 0; i--) {
    char *moved = dparts[i] + i * BASE64URL_EXTRA_BYTES;
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "adding extra space betwen parts: part %d %p -> %p\n", i, dparts[i],
          moved);
    memmove(moved, dparts[i], pLen[i]);
    dparts[i] = moved;
    pLen[i - 1] += BASE64URL_EXTRA_BYTES;
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "part %d is now %u bytes\n", i - 1, pLen[i - 1]);
  }
  for (i = 0; i < nParts; i++) {
    int base64Rc = 0;

    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "decoding part %d: %s[%u]...\n", i, dparts[i], pLen[i]);
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "calling base64urlToBase64()...\n");
    if ((base64Rc = base64urlToBase64(dparts[i], pLen[i])) < 0) {
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "error: invalid base64url %d\n", base64Rc);
      return RC_JWT_INVALID_ENCODING;
    }
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "got base64: %s\n", dparts[i]);
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "calling decodeBase64()...\n");
    base64Rc = decodeBase64(dparts[i], decodedText);
    if (base64Rc < 0) {
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "error: invalid base64\n");
      return RC_JWT_INVALID_ENCODING;
    }
    dparts[i] = decodedText;
    pLen[i] = (unsigned int)base64Rc;
    decodedText += (unsigned int)base64Rc;
    decodedText[0] = '\0';
    decodedText++;
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "decoded part %d\n", i);
  }
  return nParts;
}

static int readHeader(char *jsonText, int len,  ShortLivedHeap *slh, Jwt *jwt) {
  char errBuf[128];
  int status;
  char *inputAlg;

  JwsHeader *head = &jwt->header;
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "readHeader: calling jsonParseUnterminatedUtf8String()...\n");
  Json *json = jsonParseUnterminatedUtf8String(slh, JSON_CCSID, jsonText,
      len, errBuf, sizeof (errBuf));
  if (json == NULL) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "error: %.128s\n", errBuf);
    return RC_JWT_INVALID_JSON;
  }
  JsonObject *obj = jsonAsObject(json);
  if (obj == NULL) {
     zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "readHeader: jsonAsObject failed()\n");
     return RC_JWT_INVALID_JSON;
   }
  inputAlg = jsonStringProperty(obj, "alg", &status);
  if (inputAlg == NULL) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "readHeader: no alg in the json\n");
    return RC_JWT_INVALID_JSON;
  }
#define SET_ALG_IF_MATCHES($alg)            \
  if (strcmp(#$alg, inputAlg) == 0) {       \
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "algorithm %s\n", #$alg);         \
    head->algorithm = JWS_ALGORITHM_##$alg; \
  } else

  WITH_JWT_ALGORITHMS(SET_ALG_IF_MATCHES) {
    /* if none matches */
    return RC_JWT_UNKNOWN_ALGORITHM;
  }
#undef SET_ALG_IF_MATCHES

  head->keyId = jsonStringProperty(obj, "kid", &status);
  if (status == JSON_PROPERTY_UNEXPECTED_TYPE) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "readHeader: header.kid type unexpected\n");
    return RC_JWT_INVALID_JSON;
  }
  return RC_JWT_OK;
}

static int readClaims(char *jsonText, int len, ShortLivedHeap *slh, Jwt *jwt) {
  char errBuf[128];
  Json *json;
  JsonObject *obj;
  JwtClaim **nextClaim = &jwt->firstCustomClaim;

  json = jsonParseUnterminatedUtf8String(slh, JSON_CCSID, jsonText,
        len, errBuf, sizeof (errBuf));
  if (json == NULL) {
    return RC_JWT_INVALID_JSON;
  }
  obj = jsonAsObject(json);
  if (obj == NULL) {
    return RC_JWT_INVALID_JSON;
  }
  for (JsonProperty *p = jsonObjectGetFirstProperty(obj);
      p != NULL;
      p = jsonObjectGetNextProperty(p)) {
    bool standard = FALSE;
    char *key = p->key;

    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "property %s, value at %p\n", p->key, p->value);
#define KEY_IS($val) (strncmp(key, $val, 4) == 0)

#define SET_STANDARD_STRING($name) do {\
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "standard string\n");          \
  standard = TRUE;                     \
  jwt->$name = jsonAsString(p->value); \
} while (0)

#define SET_STANDARD_NUMBER($name) do {\
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "standard number\n");          \
  standard = TRUE;                     \
  jwt->$name = jsonAsNumber(p->value); \
} while (0)

    if (KEY_IS("aud")) {
      SET_STANDARD_STRING(audience);
    } else if (KEY_IS("exp")) {
      SET_STANDARD_NUMBER(expirationTime);
    } else if (KEY_IS("iat")) {
      SET_STANDARD_NUMBER(issuedAt);
    } else if (KEY_IS("iss")) {
      SET_STANDARD_STRING(issuer);
    } else if (KEY_IS("jti")) {
      SET_STANDARD_NUMBER(jwtId);
    } else if (KEY_IS("nbf")) {
      SET_STANDARD_NUMBER(notBefore);
    } else if (KEY_IS("sub")) {
      SET_STANDARD_STRING(subject);
    }
#undef KEY_IS
#undef SET_STANDARD_STRING
#undef SET_STANDARD_NUMBER

    if ((slh != NULL) && !standard) {
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "making custom claim %s \n", p->key);
      JwtClaim *c = (void *)SLHAlloc(slh, sizeof (*c));
      if (c == NULL) {
        return RC_JWT_MEMORY_ERROR;
      }
      c->name = p->key;
      c->value = p->value;
      *nextClaim = c;
      nextClaim = &c->next;
    }
  }
  return 0;
}

/*
 * It is the responsibility of upstream code to acquire an ICSFP11_HANDLE_T*
 * that is appropriate for the JWT signer and the algorithm used.
 */
static int checkSignature(JwsAlgorithm algorithm,
                          int sigLen, const uint8_t signature[],
                          int msgLen, const uint8_t message[],
                          ICSFP11_HANDLE_T *keyHandle) {
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "Going to verify the signature of this message: \n");
  if (jwtTrace) {                            
    dumpbuffer(message, msgLen);
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature: %p\n", signature);
  if (signature != NULL && jwtTrace) {
    dumpbuffer(signature, sigLen);
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "keyHandle: %p\n", keyHandle);
  if (keyHandle != NULL && jwtTrace) {
    dumpbuffer((void*)keyHandle, sizeof(ICSFP11_HANDLE_T));
  }
  
  int sts = RC_JWT_OK;
  int p11rc=0, p11rsn=0;

  switch (algorithm) {
    case JWS_ALGORITHM_none: {
      if (sigLen == 0) {
        sts = RC_JWT_INSECURE;
      } else {
        sts = RC_JWT_INVALID_SIGLEN;
      }
      break;
    }
    case JWS_ALGORITHM_RS256: {
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature JWS_ALGORITHM_RS256\n");
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "message:\n");
      if (jwtTrace) {
        dumpbuffer(message, msgLen);
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature:\n");
      if (jwtTrace) {
        dumpbuffer(signature, sigLen);
      }
      sts = rs_icsfp11_RS256_verify(keyHandle,
                                    message, (int) msgLen,
                                    signature, (int) sigLen,
                                    &p11rc, &p11rsn);
      if ((0 != sts) || (0 != p11rc)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature: error from rs_icsfp11_RS256_verify "
              "(sts=%d, rc=%d, rsn=0x%x)\n",
              sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      break;
    }

    case JWS_ALGORITHM_HS256: {
      if (sigLen != ICSFP11_SHA256_HASHLEN) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "saw HS256 sig alg with unexpected signature length\n");
        sts = RC_JWT_INVALID_SIGLEN;
        break;
      }
      unsigned char hmacbuf[ICSFP11_SHA256_HASHLEN] = {0};
      int hmacbuflen = sizeof(hmacbuf);
      sts = rs_icsfp11_hmacSHA256(keyHandle,
                                  message, msgLen,
                                  hmacbuf, &hmacbuflen,
                                  &p11rc, &p11rsn);

      if ((0 != sts) ||
          (0 != p11rc) || (0 != p11rsn)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature: failed to produce comparison HMAC "
              "(sts:%d, p11rc:%d, p11rsn:0x%x)\n",
               sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "comparison HMAC value: \n");
      if (jwtTrace) {
        dumpbuffer(hmacbuf, sizeof(hmacbuf));
      }
      if (0 != memcmp(signature, hmacbuf, ICSFP11_SHA256_HASHLEN)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature verification failed\n");
        sts = RC_JWT_SIG_MISMATCH;
        break;
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature verified successfully\n");
      break;
    }

    case JWS_ALGORITHM_HS384: {
      if (sigLen != ICSFP11_SHA384_HASHLEN) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "saw HS256 sig alg with unexpected signature length\n");
        sts = RC_JWT_INVALID_SIGLEN;
        break;
      }
      unsigned char hmacbuf[ICSFP11_SHA384_HASHLEN] = {0};
      int hmacbuflen = sizeof(hmacbuf);

      sts = rs_icsfp11_hmacSHA384(keyHandle,
                                  message, msgLen,
                                  hmacbuf, &hmacbuflen,
                                  &p11rc, &p11rsn);

      if ((0 != sts) ||
          (0 != p11rc) || (0 != p11rsn)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature: failed to produce comparison HMAC "
              "(sts:%d, p11rc:%d, p11rsn:0x%x)\n",
               sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "comparison HMAC value: \n");
      if (jwtTrace) {  
        dumpbuffer(hmacbuf, sizeof(hmacbuf));
      }
      if (0 != memcmp(signature, hmacbuf, ICSFP11_SHA384_HASHLEN)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature verification failed\n");
        sts = RC_JWT_SIG_MISMATCH;
        break;
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature verified successfully\n");
      break;
    }

    case JWS_ALGORITHM_HS512: {
      if (sigLen != ICSFP11_SHA512_HASHLEN) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "saw HS256 sig alg with unexpected signature length\n");
        sts = RC_JWT_INVALID_SIGLEN;
        break;
      }
      unsigned char hmacbuf[ICSFP11_SHA512_HASHLEN] = {0};
      int hmacbuflen = sizeof(hmacbuf);
      sts = rs_icsfp11_hmacSHA512(keyHandle,
                                  message, msgLen,
                                  hmacbuf, &hmacbuflen,
                                  &p11rc, &p11rsn);
      if ((0 != sts) ||
          (0 != p11rc) || (0 != p11rsn))  {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature: failed to produce comparison HMAC "
              "(sts:%d, p11rc:%d, p11rsn:0x%x)\n",
               sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "comparison HMAC value: \n");
      if (jwtTrace) {
        dumpbuffer(hmacbuf, sizeof(hmacbuf));
      }
      if (0 != memcmp(signature, hmacbuf, ICSFP11_SHA512_HASHLEN)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature verification failed\n");
        sts = RC_JWT_SIG_MISMATCH;
        break;
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature verified successfully\n");
      break;
    }

    default: {
      sts = RC_JWT_UNSUPPORTED_ALG;
      break;
    }
  }

  return sts;
}


int jwtParse(const char *base64Text, bool ebcdic, ICSFP11_HANDLE_T *keyHandle,
             ShortLivedHeap *slh, Jwt **out) {
  char *base64TextCopy, *decodedText;
  char *decodedParts[MAX_NPARTS] = { NULL };
  int pLen[MAX_NPARTS] = { 0 };
  const int base64Len = strlen(base64Text);
  const int bufSize = base64Len + BASE64URL_EXTRA_BYTES_TOTAL + 1;
  int rc = RC_JWT_OK;
  int nparts;
  Jwt *j;

  if (slh == NULL) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "jwt slh missing\n");
    return RC_JWT_MEMORY_ERROR;
  }
  j = (void *)SLHAlloc(slh, sizeof (*j));
  if (j == NULL) {
    return RC_JWT_MEMORY_ERROR;
  }
  if (!BASE64_IS_EBCDIC && ebcdic) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "error: ebcdic on an ascii platform not supported\n");
    return RC_JWT_INVALID_ENCODING;
  }
  base64TextCopy = safeMalloc(bufSize, "JWT text copy");
  strncpy(base64TextCopy, base64Text, base64Len);
  decodedText = safeMalloc(bufSize, "decoded JWT text");

  if (BASE64_IS_EBCDIC && !ebcdic) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "converting to ebcdic...\n");
    a2e(base64TextCopy, base64Len);
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "calling extractParts()...\n");
  nparts = extractParts(base64TextCopy, MAX_NPARTS, decodedParts, pLen, decodedText);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "nparts %d\n", nparts);
  if (nparts < 0) {
    return nparts;
  }

  if (nparts < 1) {
    return RC_JWT_MISSING_PART;
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "calling readHeader()...\n");
  rc = readHeader(decodedParts[0], pLen[0], slh, j);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "readHeader() rc %d...\n", rc);
  if (rc != RC_JWT_OK) {
    goto exit;
  }

  if (nparts < 2) {
    return RC_JWT_MISSING_PART;
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "calling readClaims()...\n");
  rc = readClaims(decodedParts[1], pLen[1], slh, j);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "readClaims() rc %d...\n", rc);
  if (rc != RC_JWT_OK) {
    goto exit;
  }

  const char *asciiBase64 = NULL;
  int prefixLen = 0;
  if (nparts == 3) {
    int dotsNum = 0;

    if (!ebcdic) {
      asciiBase64 = base64Text;
    } else {
      strncpy(base64TextCopy, base64Text, base64Len);
      e2a(base64TextCopy, base64Len);
      asciiBase64 = base64TextCopy;
    }
    while ((dotsNum < 2) && (prefixLen < base64Len)) {
      if (asciiBase64[prefixLen++] == ASCII_PERIOD) {
        dotsNum++;
      }
    }
  }
  /*
   * note that the alg can be 'none' so we formally support the case when
   * `nparts < 3`: we just won't verify the signature, so everything except the
   * header will be ignored
   */
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "calling checkSignature()...\n");
  rc = checkSignature(j->header.algorithm, pLen[2], decodedParts[2],
      prefixLen - 1, asciiBase64, keyHandle);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature() rc %d...\n", rc);
  if (rc != RC_JWT_OK && rc != RC_JWT_INSECURE) {
    goto exit;
  }

  *out = j;

exit:
  safeFree(base64TextCopy, bufSize);
  safeFree(decodedText, bufSize);
  return rc;
}

Json *jwtGetCustomClaim(const Jwt *jwt, const char *name) {
  JwtClaim *claim;

  JWT_FOR_CLAIMS (jwt, claim) {
    if (strcmp(claim->name, name) == 0) {
      return claim->value;
    }
  }
  return NULL;
}

bool jwtAreBasicClaimsValid(const Jwt *jwt, const char *audience) {
  if (jwt->expirationTime == 0) {
    return false;
  }
  int64_t stck; getSTCK(&stck);
  int64_t currentTime = stckToUnix(stck);

  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "Time now: %lld, valid after: %lld, expiration: %lld\n",
      currentTime, jwt->notBefore, jwt->expirationTime);
  if (!((jwt->notBefore <= currentTime) && (currentTime < jwt->expirationTime))) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "JWT expired or not yet active\n");
    return false;
  }

  if (currentTime  < jwt->issuedAt) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "JWT issued in the future???\n");
    return false;
  }

  if (audience != NULL) {
    if ((jwt->audience == NULL) || (strcmp(audience, jwt->audience) != 0)) {
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "JWT is not for %s\n", audience);
      return false;
    }
  }

  return true;
}

static int serializeHeader(const Jwt *j, JsonBuffer *jbuf) {
  int rc = RC_JWT_OK;
  const JwsHeader *h = &j->header;
  jsonPrinter *p = makeBufferJsonPrinter(JSON_CCSID, jbuf);
  if (p == NULL) {
    rc = RC_JWT_MEMORY_ERROR;
    goto json_printer_freed;
  }

  jsonStart(p);
  switch (h->algorithm) {

#define ALG_TO_JSON($alg)           \
  case JWS_ALGORITHM_##$alg:        \
    jsonAddString(p, "alg", #$alg); \
    break;

    WITH_JWT_ALGORITHMS(ALG_TO_JSON)

#undef ALG_TO_JSON
  }
  if (h->keyId != NULL) {
    jsonAddString(p, "kid", h->keyId);
  }
  jsonEnd(p);

exit:
  freeJsonPrinter(p);
json_printer_freed:
  return rc;
}

static int serializeClaims(const Jwt *j, JsonBuffer *jbuf) {
  int rc = RC_JWT_OK;
  jsonPrinter *p = makeBufferJsonPrinter(JSON_CCSID, jbuf);
  if (p == NULL) {
    rc = RC_JWT_MEMORY_ERROR;
    goto json_printer_freed;
  }

  jsonStart(p);

  if (j->issuer != NULL) {
    jsonAddString(p, "iss", j->issuer);
  }
  if (j->subject != NULL) {
    jsonAddString(p, "sub", j->subject);
  }
  if (j->audience != NULL) {
    jsonAddString(p, "aud", j->audience);
  }
  if (j->expirationTime != 0llu) {
    jsonAddInt64(p, "exp", j->expirationTime);
  }
  if (j->notBefore != 0llu) {
    jsonAddInt64(p, "nbf", j->notBefore);
  }
  if (j->issuedAt != 0llu) {
    jsonAddInt64(p, "iat", j->issuedAt);
  }
  if (j->jwtId != 0llu) {
    jsonAddInt64(p, "jti", j->jwtId);
  }

  jsonEnd(p);

exit:
  freeJsonPrinter(p);
json_printer_freed:
  return rc;
}

/*
 * It is the responsibility of upstream code to acquire an ICSFP11_HANDLE_T*
 * that is appropriate for the JWT signer and the algorithm used.
 */
static int generateSignature(JwsAlgorithm algorithm,
                             int msgLen, const uint8_t message[],
                             int *sigLen, uint8_t signature[],
                             const ICSFP11_HANDLE_T *keyHandle) {
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "Going to generate the signature for this message: \n");
  if (jwtTrace) {                               
    dumpbuffer(message, msgLen);
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "keyHandle: %p\n", keyHandle);
  if (keyHandle != NULL && jwtTrace) {
    dumpbuffer((void*)keyHandle, sizeof(ICSFP11_HANDLE_T));
  }
  int sts = RC_JWT_OK;
  int p11rc=0, p11rsn=0;

  switch (algorithm) {
    case JWS_ALGORITHM_none: {
      *sigLen = 0;
      break;
    }
    case JWS_ALGORITHM_RS256: {
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "generateSignature JWS_ALGORITHM_RS256\n");
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "message:\n");
      if (jwtTrace) {
        dumpbuffer(message, msgLen);
      }
      sts = rs_icsfp11_RS256_sign(keyHandle,
                                  message, msgLen,
                                  signature, sigLen,
                                  &p11rc, &p11rsn);
      if ((0 != sts) || (0 != p11rc)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "generateSignature: error from rs_icsfp11_RS256_verify "
              "(sts=%d, rc=%d, rsn=0x%x)\n",
              sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature:\n");
      if (jwtTrace) {
        dumpbuffer(signature, *sigLen);
      }  
      break;
    }

    case JWS_ALGORITHM_HS256: {
      sts = rs_icsfp11_hmacSHA256(keyHandle,
                                  message, msgLen,
                                  signature, sigLen,
                                  &p11rc, &p11rsn);
      if ((0 != sts) ||
          (0 != p11rc) || (0 != p11rsn)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "generateSignature: failed to produce comparison HMAC "
              "(sts:%d, p11rc:%d, p11rsn:0x%x)\n",
               sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      *sigLen = ICSFP11_SHA256_HASHLEN;
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature:\n");
      if (jwtTrace) {
        dumpbuffer(signature, *sigLen);
      }
      break;
    }

    case JWS_ALGORITHM_HS384: {
      sts = rs_icsfp11_hmacSHA384(keyHandle,
                                  message, msgLen,
                                  signature, sigLen,
                                  &p11rc, &p11rsn);
      if ((0 != sts) ||
          (0 != p11rc) || (0 != p11rsn)) {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature: failed to produce comparison HMAC "
              "(sts:%d, p11rc:%d, p11rsn:0x%x)\n",
               sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      *sigLen = ICSFP11_SHA384_HASHLEN;
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature:\n");
      if (jwtTrace) {  
        dumpbuffer(signature, *sigLen);
      }
      break;
    }

    case JWS_ALGORITHM_HS512: {
      sts = rs_icsfp11_hmacSHA512(keyHandle,
                                  message, msgLen,
                                  signature, sigLen,
                                  &p11rc, &p11rsn);
      if ((0 != sts) ||
          (0 != p11rc) || (0 != p11rsn))  {
        zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "checkSignature: failed to produce comparison HMAC "
              "(sts:%d, p11rc:%d, p11rsn:0x%x)\n",
               sts, p11rc, p11rsn);
        sts = RC_JWT_CRYPTO_ERROR;
        break;
      }
      *sigLen = ICSFP11_SHA512_HASHLEN;
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signature:\n");
      if (jwtTrace) {  
        dumpbuffer(signature, *sigLen);
      }
      break;
    }

    default: {
      sts = RC_JWT_UNSUPPORTED_ALG;
      break;
    }
  }

  return sts;
}

int jwtEncode(const Jwt *jwt, bool ebcdic,
              const ICSFP11_HANDLE_T *keyHandle, ShortLivedHeap *slh,
              int *encodedSize, char **encoded) {
  int rc = RC_JWT_OK;

  JsonBuffer *const jbufHead = makeJsonBuffer();
  if (jbufHead == NULL) {
    rc = RC_JWT_MEMORY_ERROR;
    goto json_buffer_head_freed;
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "Serializing the header...\n");
  rc = serializeHeader(jwt, jbufHead);
  if (rc != RC_JWT_OK) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "fail, rc %d\n", rc);
    goto exit;
  }

  JsonBuffer *const jbufClaims = makeJsonBuffer();
  if (jbufClaims == NULL) {
    rc = RC_JWT_MEMORY_ERROR;
    goto json_buffer_claims_freed;
  }
  if (jwtTrace) {
    dumpbuffer(jbufHead->data, jbufHead->len);
  }
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "Serializing the claims...\n");
  rc = serializeClaims(jwt, jbufClaims);
  if (rc != RC_JWT_OK) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "fail, rc %d\n", rc);
    goto exit;
  }
  if (jwtTrace) {
    dumpbuffer(jbufClaims->data, jbufClaims->len);
  }
  const int resultSize = BASE64_ENCODE_SIZE(jbufHead->len) + 1
      + BASE64_ENCODE_SIZE(jbufClaims->len) + 1
      + BASE64_ENCODE_SIZE(JWT_MAX_SIGNATURE_SIZE) + 1;
  char *const result = SLHAlloc(slh, resultSize);
  if (result == NULL) {
    rc = RC_JWT_MEMORY_ERROR;
    goto exit;
  }
  memset(result, 0, resultSize);

  int partsLen;
  int base64Size;
  encodeBase64NoAlloc(jbufHead->data, jbufHead->len, result, &base64Size,
                      BASE64_IS_EBCDIC);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "base64 header: %s[%u]\n", result, base64Size);
  partsLen = base64ToBase64url(result);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "base64url header: %s[%u]\n", result, partsLen);
  result[partsLen++] = '.';
  encodeBase64NoAlloc(jbufClaims->data, jbufClaims->len, &result[partsLen],
                      &base64Size, BASE64_IS_EBCDIC);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "base64 claims: %s[%u]\n", &result[partsLen], base64Size);
  partsLen += base64ToBase64url(&result[partsLen]);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "base64url header + claims: %.*s[%d]\n", partsLen, result, partsLen);

  if (jwt->header.algorithm != JWS_ALGORITHM_none) {
    int signRc = 0;
    uint8_t *const signatureBuf = (void *)safeMalloc(JWT_MAX_SIGNATURE_SIZE,
        "JWT signature");
    if (signatureBuf == NULL) {
      signRc = RC_JWT_MEMORY_ERROR;
      goto signatureBuf_freed;
    }

    int signatureLength = JWT_MAX_SIGNATURE_SIZE;
    if (!ebcdic) {
      signRc = generateSignature(jwt->header.algorithm,
          partsLen, result,
          &signatureLength, signatureBuf,
          keyHandle);
    } else {
      char *asciiBase64 = (void *)safeMalloc(partsLen + 1, "JWT ASCII base64");
      if (asciiBase64 == NULL) {
        signRc = RC_JWT_MEMORY_ERROR;
        goto asciiBase64_freed;
      }
      strncpy(asciiBase64, result, partsLen + 1);
      e2a(asciiBase64, partsLen);
      signRc = generateSignature(jwt->header.algorithm,
           partsLen, asciiBase64,
           &signatureLength, signatureBuf,
           keyHandle);
      safeFree(asciiBase64, partsLen + 1);
asciiBase64_freed:
      ;
    }

    if (signRc == RC_JWT_OK) {
      zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "signatureLength %d\n", signatureLength);
      result[partsLen++] = '.';
      encodeBase64NoAlloc(signatureBuf, signatureLength, &result[partsLen],
            &base64Size, BASE64_IS_EBCDIC);
      partsLen += base64ToBase64url(&result[partsLen]);
    }
    safeFree(signatureBuf, JWT_MAX_SIGNATURE_SIZE);
signatureBuf_freed:
    ;
    rc = signRc;
    if (signRc != RC_JWT_OK) {
      goto exit;
    }
  }

  *encodedSize = partsLen + 1;
  *encoded = result;
exit:
  freeJsonBuffer(jbufClaims);
json_buffer_claims_freed:
  freeJsonBuffer(jbufHead);
json_buffer_head_freed:
  return rc;
}


struct JwtContext_tag {
  ICSFP11_HANDLE_T *tokenHandle;
  ICSFP11_HANDLE_T *keyHandle;
};

JwtContext *makeJwtContextForKeyInToken(const char *in_tokenName,
                                         const char *in_keyLabel,
                                         int class,
                                         int *out_rc,
                                         int *out_p11rc, int *out_p11rsn) {

  logConfigureComponent(NULL, LOG_COMP_JWT, "JWT", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  if (jwtTrace) {
    logSetLevel(NULL, LOG_COMP_JWT, jwtTrace);
  }
  JwtContext *result = (void *)safeMalloc(sizeof (*result), "JwtContext");
  if (result == NULL) {
    *out_rc = RC_JWT_CONTEXT_ALLOCATION_FAILED;
    goto context_freed;
  }

  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "attempting to address the token %s...\n", in_tokenName);
  ICSFP11_HANDLE_T *tokenHandle = NULL;
  const int findTokenRc = rs_icsfp11_findToken(in_tokenName, &tokenHandle,
      out_p11rc, out_p11rsn);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "findTokenRc %d, out_p11rc %d, out_p11rsn %d\n", findTokenRc, *out_p11rc,
      *out_p11rsn);
  if (!((0 == findTokenRc) && (0 == *out_p11rc) && (0 == *out_p11rsn))) {
    *out_rc = RC_JWT_CRYPTO_TOKEN_NOT_FOUND;
    goto token_freed;
  }
  if (jwtTrace) {  
    dumpbuffer((void*)tokenHandle, sizeof (*tokenHandle));
  }
  result->tokenHandle = tokenHandle;

  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "attempting to address the key record %s(%d)...\n", in_keyLabel, class);
  ICSFP11_HANDLE_T *const keyHandle = (void *) safeMalloc(sizeof (*keyHandle),
      "keyhandle");
  if (keyHandle == NULL) {
    *out_rc = RC_JWT_CONTEXT_ALLOCATION_FAILED;
    goto key_copy_freed;
  }

  ICSFP11_HANDLE_T *foundHandles = NULL;
  int numfound = 0;
  const int findKeyRc = rs_icsfp11_findObjectsByLabelAndClass(tokenHandle, in_keyLabel,
      class,
      NULL, &foundHandles, &numfound,
      out_p11rc, out_p11rsn);
  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "findKeyRc %d, out_p11rc %d, out_p11rsn %d, numfound %d\n",
      findKeyRc, *out_p11rc, *out_p11rsn, numfound);
  if (!((0 == findKeyRc) && (0 == *out_p11rc) && (0 == *out_p11rsn) && (numfound > 0))) {
    *out_rc = RC_JWT_KEY_NOT_FOUND;
    goto key_not_found;
  }

  zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "key record found\n");
  if (jwtTrace) {  
    dumpbuffer((void *)foundHandles, numfound * sizeof (foundHandles[0]));
  }

  if (1 < numfound) {
    zowelog(NULL, LOG_COMP_JWT, ZOWE_LOG_DEBUG, "Warning: More than one key record found for label: %s\n", in_keyLabel);
  }
  memcpy(keyHandle, &(foundHandles[0]), sizeof (foundHandles[0]));
  safeFree((void *)foundHandles, numfound * sizeof (foundHandles[0]));
  result->keyHandle = keyHandle;
  *out_rc = RC_JWT_OK;
  return result;

key_not_found:
  safeFree((void *)keyHandle, sizeof (*keyHandle));
key_copy_freed:
  safeFree((void *)tokenHandle, sizeof (*tokenHandle));
token_freed:
  safeFree((void *)result, sizeof (*result));
context_freed:
  return NULL;
}

Jwt *jwtVerifyAndParseToken(const JwtContext *self,
                            const char *tokenText,
                            bool ebcdic,
                            ShortLivedHeap *slh,
                            int *rc) {
  Jwt *out;
  *rc = jwtParse(tokenText, ebcdic, self->keyHandle, slh, &out);
  return out;
}

char *jwtEncodeToken(const JwtContext *self,
                     const Jwt *jwt,
                     bool ebcdic,
                     ShortLivedHeap *slh,
                     int *encodedSize,
                     int *rc) {
  char *out;
  *rc = jwtEncode(jwt, ebcdic, self->keyHandle, slh, encodedSize, &out);
  return out;
}

void jwtContextDestroy(JwtContext *self) {
  if (self->tokenHandle != NULL) {
    safeFree((void *)self->tokenHandle, sizeof (*self->tokenHandle));
  }
  if (self->keyHandle != NULL) {
    safeFree((void *)self->keyHandle, sizeof (*self->keyHandle));
  }
  safeFree((void *)self, sizeof (*self));
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

