/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef H_JWT_H_
#define H_JWT_H_

#include "zowetypes.h"
#include "json.h"
#include "utils.h"
#include "rs_icsfp11.h"

#define RC_JWT_OK                0
#define RC_JWT_MEMORY_ERROR      1
#define RC_JWT_MISSING_PART      2
#define RC_JWT_EXTRA_PART        3
#define RC_JWT_INVALID_ENCODING  4
#define RC_JWT_INVALID_JSON      5
#define RC_JWT_UNKNOWN_ALGORITHM 6
#define RC_JWT_UNSUPPORTED_ALG   7
#define RC_JWT_INVALID_SIGLEN    8
#define RC_JWT_CRYPTO_ERROR      9
#define RC_JWT_SIG_MISMATCH      10
#define RC_JWT_CONTEXT_ALLOCATION_FAILED 11
#define RC_JWT_CRYPTO_TOKEN_NOT_FOUND   12
#define RC_JWT_KEY_NOT_FOUND     13
/*
 * RC_JWT_INSECURE is returned when a JWT is valid but has "alg": "none".
 * This is a source of security issues, so a distinct RC is needed.
 * If an app needs to accept insecure JWTs for some reason, it can do something
 * like:
#ifdef DISABLE_SECURITY
#  define IS_JWT_RC_OK($rc) (($rc) == RC_JWT_OK || ($rc) == RC_JWT_INSECURE)
#else
#  define IS_JWT_RC_OK($rc) (($rc) == RC_JWT_OK)
#endif
 */
#define RC_JWT_INSECURE          14

#define MAX_JWT_RC RC_JWT_INSECURE

extern const char *RSJWT_ERROR_DESCRIPTIONS[];
#define JWT_ERROR_DESCRIPTION($rc) ((0 <= ($rc) && ($rc) <= MAX_JWT_RC)? \
    RSJWT_ERROR_DESCRIPTIONS[$rc] : NULL)

#define WITH_JWT_ALGORITHMS($algHandler) \
  $algHandler(none)  /* No digital signature or MAC performed    Optional */ \
  $algHandler(HS256) /* HMAC using SHA-256              Required    */ \
  $algHandler(HS384) /* HMAC using SHA-384              Optional    */ \
  $algHandler(HS512) /* HMAC using SHA-512              Optional    */ \
  $algHandler(RS256) /* RSASSA-PKCS1-v1_5 using SHA-256         Recommended */ \
  $algHandler(RS384) /* RSASSA-PKCS1-v1_5 using SHA-384         Optional  */ \
  $algHandler(RS512) /* RSASSA-PKCS1-v1_5 using  SHA-512        Optional */ \
  $algHandler(ES256) /* ECDSA using P-256 and SHA-256   Recommended+ */ \
  $algHandler(ES384) /* ECDSA using P-384 and SHA-384   Optional */ \
  $algHandler(ES512) /* ECDSA using P-521 and SHA-512   Optional */ \
  $algHandler(PS256) /* RSASSA-PSS using SHA-256 and MGF1 with SHA-256  Optional */ \
  $algHandler(PS384) /* RSASSA-PSS using SHA-384 and  MGF1 with SHA-384   Optional  */ \
  $algHandler(PS512) /* RSASSA-PSS using SHA-512 and MGF1 with SHA-512   Optional  */

#define JWT_MAX_SIGNATURE_SIZE 2048

typedef enum JwsAlgorithm_tag {
#define JWT_ENUM_FIELD($alg) JWS_ALGORITHM_##$alg,
  WITH_JWT_ALGORITHMS(JWT_ENUM_FIELD)
#undef JWT_ENUM_FIELD
} JwsAlgorithm;


typedef struct JwsHeader_tag {
  JwsAlgorithm algorithm;
  char *keyId;
  /* optional stuff not implemented yet */
} JwsHeader;

typedef struct Jwt_tag {
  JwsHeader header;
  char *issuer;
  char *subject;
  char *audience;
  int64 expirationTime;
  int64 notBefore;
  int64 issuedAt;
  int64 jwtId;
  struct JwtClaim_tag *firstCustomClaim;
  char *signature;
} Jwt;

typedef struct JwtClaim_tag {
  char *name;
  Json *value;
  struct JwtClaim_tag *next;
} JwtClaim;

typedef struct JwtContext_tag JwtContext;

#define JWT_FOR_CLAIMS($jwt, $var) for ($var = ($jwt)->firstCustomClaim; \
    $var != NULL ;$var = ($var)->next)

int jwtParse(const char *base64Text,
             bool ebcdic,
             const ICSFP11_HANDLE_T *keyHandle,
             ShortLivedHeap *slh,
             Jwt **out);

int jwtEncode(const Jwt *jwt,
              bool ebcdic,
              const ICSFP11_HANDLE_T *keyHandle,
              ShortLivedHeap *slh,
              int *encodedSize,
              char **encoded);

Json *jwtGetCustomClaim(const Jwt *jwt, const char *name);

bool jwtAreBasicClaimsValid(const Jwt *jwt, const char *audience);

JwtContext *makeJwtContextForKeyInToken(const char *in_tokenName,
                                        const char *in_keyLabel,
                                        int class,
                                        int *out_rc,
                                        int *out_p11rc, int *out_p11rsn);

Jwt *jwtVerifyAndParseToken(const JwtContext *self,
                            const char *tokenText,
                            bool ebcdic,
                            ShortLivedHeap *slh,
                            int *rc);

char *jwtEncodeToken(const JwtContext *self,
                     const Jwt *jwt,
                     bool ebcdic,
                     ShortLivedHeap *slh,
                     int *encodedSize,
                     int *rc);

void jwtContextDestroy(JwtContext *self);

#endif /* H_JWT_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
