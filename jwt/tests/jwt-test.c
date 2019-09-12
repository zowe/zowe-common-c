/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __IBM_METAL__
#define _XOPEN_SOURCE 500
#include <string.h>
#include <stdio.h>
#endif

#include "zowetypes.h"
#include "charsets.h"
#include "json.h"
#include "utils.h"
#include "xlate.h"
#include "alloc.h"

/* rscrypto */
#include "rs_icsfp11.h"
#include "rs_crypto_errors.h"

#include "jwt.h"
#include <assert.h>

#ifdef JWT_DEBUG
#define DEBUG printf
#define DUMPBUF dumpbuffer
#else
#define DEBUG(...) (void)0
#define DUMPBUF(...) (void)0
#endif

/*
  the default example token from https://jwt.io/

{
  "alg": "HS256",
  "typ": "JWT"
}
{
  "sub": "1234567890",
  "name": "John Doe",
  "iat": 1516239022
}

The HMAC key is Secret123

The RSA key is secsrv.p12: import using gskkyman, the password is "password":

  gskkyman -i -t ZOWE.ZSS.JWTKEYS -l KEY_RS256 -p secsrv.p12

 */

const char TOKEN_NAME[] = "ZOWE.ZSS.JWTKEYS";

const char HMAC_KEY[] = {0x53, 0x65, 0x63, 0x72, 0x65, 0x74, 0x31, 0x32, 0x33};

static void shouldParseNativeJwt(void) {

  printf("\n *** should Parse Native JWT\n");

  static const char JWT_ALG_NONE[] = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_NONE, true,
      slh, &parseRc);
  assert(parseRc == RC_JWT_INSECURE);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  printf("subject '%s'\n", j->subject);
  assert(strcmp(j->subject, "1234567890") == 0);

  printf("issued at '%lld'\n", j->issuedAt);
  assert(j->issuedAt == 1516239022ll);

  Json *name = jwtGetCustomClaim(j, "name");
  if (name != NULL) {
    printf("name %s\n", jsonAsString(name));
  }
  assert(name != NULL);
  assert(strcmp(jsonAsString(name), "John Doe") == 0);

  SLHFree(slh);
}

static void shouldParseAsciiJWT(void) {

  printf("\n *** should Parse ASCII JWT\n");

#pragma convert("UTF8")
  static const char JWT_ALG_NONE_ASCII[] = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"
    ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ";
#pragma convert(pop)
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_NONE_ASCII, false,
      slh, &parseRc);
  assert(parseRc == RC_JWT_INSECURE);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  printf("subject '%s'\n", j->subject);
  assert(strcmp(j->subject, "1234567890") == 0);

  printf("issued at '%lld'\n", j->issuedAt);
  assert(j->issuedAt == 1516239022ll);

  Json *name = jwtGetCustomClaim(j, "name");
  if (name != NULL) {
    printf("name %s\n", jsonAsString(name));
  }
  assert(name != NULL);
  assert(strcmp(jsonAsString(name), "John Doe") == 0);

  SLHFree(slh);
}

static void shouldRejectMissingSignature(void) {

  printf("\n *** should reject missing signature\n");

  static const char JWT_ALG_HS256[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_HS256, true,
      slh, &parseRc);

  printf("rc %d\n", parseRc);
  assert(parseRc == RC_JWT_MISSING_PART || parseRc == RC_JWT_INVALID_SIGLEN);

  jwtContextDestroy(ctx);


  SLHFree(slh);
}

static void shouldVerifyHS256(void) {

  printf("\n *** should verify HS256\n");

  static const char JWT_ALG_HS256[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".iz6BHMT2SmuJCTvPFj9A1ZIrBqzk6TL3ff2Js1HIVDg";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_HS256, true,
      slh, &parseRc);
  assert(parseRc == 0);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  printf("subject '%s'\n", j->subject);
  assert(strcmp(j->subject, "1234567890") == 0);

  SLHFree(slh);
}

static void shouldRejectInvalidHS256(void) {

  printf("\n *** should reject invalid HS256\n");

  static const char JWT_ALG_HS256[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".iz6CHMT2SmuJCTvPFj9A1ZIrBqzk6TL3ff2Js1HIVDg";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_HS256, true,
      slh, &parseRc);
  printf("rc %d\n", parseRc);
  assert(parseRc == RC_JWT_SIG_MISMATCH);

  jwtContextDestroy(ctx);

  SLHFree(slh);
}

static void shouldVerifyHS384(void) {

  printf("\n *** should verify HS384\n");

  static const char JWT_ALG_HS384[] = "eyJhbGciOiJIUzM4NCIsInR5cCI6IkpXVCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".aouHLlIOY8EJ9UBIBkA02XCJIN1gAi3KIYVotwAqtGq1xYuhcIeChxokNqahDVgI";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_HS384, true,
      slh, &parseRc);
  assert(parseRc == 0);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  printf("subject '%s'\n", j->subject);
  assert(strcmp(j->subject, "1234567890") == 0);

  SLHFree(slh);
}

static void shouldRejectInvalidHS384(void) {

  printf("\n *** should reject invalid HS384\n");

  static const char JWT_ALG_HS384[] = "eyJhbGciOiJIUzM4NCIsInR5cCI6IkpXVCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".aquHLlIOY8EJ9UBIBkA02XCJIN1gAi3KIYVotwAqtGq1xYuhcIeChxokNqahDVgI";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_HS384, true,
      slh, &parseRc);
  printf("rc %d\n", parseRc);
  assert(parseRc == RC_JWT_SIG_MISMATCH);

  jwtContextDestroy(ctx);


  SLHFree(slh);
}

static void shouldVerifyHS512(void) {
  printf("\n *** should verify HS512\n");

  static const char JWT_ALG_HS512[] = "eyJhbGciOiJIUzUxMiIsInR5cCI6IkpXVCJ9"
        ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
        ".LEaECa3B_W-ue8A_hHmn0BsdoaOKF4hILEOM-GaMWi8XRiDwBQXlOFI0V4wtRUw7dC_RLDD5pVk5rz8cZuQHhQ";

  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_HS512, true,
      slh, &parseRc);
  assert(parseRc == 0);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  printf("subject '%s'\n", j->subject);
  assert(strcmp(j->subject, "1234567890") == 0);

  SLHFree(slh);
}

static void shouldRejectInvalidHS512(void) {
  printf("\n *** should reject invalid HS512\n");

  static const char JWT_ALG_HS512[] = "eyJhbGciOiJIUzUxMiIsInR5cCI6IkpXVCJ9"
        ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
        ".LEaECa3B_Q-ue8A_hHmn0BsdoaOKF4hILEOM-GaMWi8XRiDwBQXlOFI0V4wtRUw7dC_RLDD5pVk5rz8cZuQHhQ";

  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_HS512, true,
      slh, &parseRc);
  printf("rc %d\n", parseRc);
  assert(parseRc == RC_JWT_SIG_MISMATCH);

  jwtContextDestroy(ctx);

  SLHFree(slh);
}

static void shouldVerifyRS256(void) {

  printf("\n *** should verify RS256\n");

  static const char JWT_ALG_RS256[] = "eyJhbGciOiJSUzI1NiJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".sgqwWTaLPWAySpIAR3kpJ39JBdc3Y9iGgUmyF3i0kfoBPS2IyzVAUIqY_ucJNfcRLZhKxq-"
      "6ATZ6CKZNxZ-mmHvCBTlF6LWPDC3Jj7ysDjq00yDI_FEg8dMf0m8vaNSlRReLmlgyXCaGoo_"
      "g495AHr2bLsSnNvHcNWEJplXQF_w";
      /*"eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiYWRtaW4iOnRydWUsImlhdCI6MTUxNjIzOTAyMn0"
      ".POstGetfAytaZS82wHcjoTyoqhMyxXiWdR7Nn7A29DNSl0EiXLdwJ6xC6AfgZWF1bOsS"
      "_TuYI3OG85AmiExREkrS6tDfTQ2B3WXlrr-wp5AokiRbz3_oB4OxG-W9KcEEbDRcZc0nH"
      "3L7LzYptiy1PtAylQGxHTWZXtGz4ht0bAecBgmpdgXMguEIcoqPJ1n3pIWk_dUZegpqx0"
      "Lka21H6XxUTxiy8OcaarA8zdnPUnV6AmNP3ecFawIFYdvJB_cm-GvpCSbr8G8y_Mllj8f"
      "4x9nBH8pQux89_6gUY618iYv7tuPWBFfEbLxtF2pZS6YC1aSfLQxeNe8djT9YjpvRZA";*/
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_RS256",
      CKO_PUBLIC_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_RS256, true,
      slh, &parseRc);
  assert(parseRc == 0);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  printf("subject '%s'\n", j->subject);
  assert(strcmp(j->subject, "1234567890") == 0);

  SLHFree(slh);
}

static void shouldRejectInvalidRS256(void) {

  printf("\n *** should reject invalid RS256\n");

  static const char JWT_ALG_RS256[] = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiYWRtaW4iOnRydWUsImlhdCI6MTUxNjIzOTAyMn0"
      ".POstGetfAytaZS82wHcjoTyoqhMyxXiWdR7Nn7A29DNSl0EiXLdwJ6xC6AfgZWF1bOsS"
      "_TuYI3OG85AmiExREkrS6tDfTQ2B3WXlrr-wp5AokiRbz3_oB4OxG-W9KcEEbDRcZc0nH"
      "3L7LzYptiy1PtAylQGxHTWZXtGz4ht0bAecBgmpdgXMguEIcoqPJ1n3pIWk_dUZegpqx0"
      "Lka21H6XxUTxiy8OcaarA8zdnPUnV6AmNP3ecFawIFYdvJB_cm-GvpCSbr8G8y_Mllj8f"
      "5x9nBH8pQux89_6gUY618iYv7tuPWBFfEbLxtF2pZS6YC1aSfLQxeNe8djT9YjpvRZA";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_RS256",
      CKO_PUBLIC_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_ALG_RS256, true,
      slh, &parseRc);
  printf("rc %d\n", parseRc);
  assert(parseRc == RC_JWT_SIG_MISMATCH || parseRc == RC_JWT_CRYPTO_ERROR);

  jwtContextDestroy(ctx);


  SLHFree(slh);
}

static void shouldEncodeASimpleJWT(void) {

  printf("\n *** should Encode A Simple JWT\n");

  /* verified at jwt.io */
  static const char *const correctResult = "eyJhbGciOiJub25lIn0"
      ".eyJpc3MiOiJGeW9kb3IiLCJzdWIiOiIxMjM0NTY3ODkwIiwiYXVkIjoidW5pdCB0ZXN0Iiwi"
      "ZXhwIjoxMjM0NTY3ODksIm5iZiI6OTg3NjU0MzIxMCwiaWF0Ijo5OTk5OTk5OTksImp0aSI6M"
      "zMzMzMzMzMzfQ";

  const Jwt j = {
    .header = {
      .algorithm = JWS_ALGORITHM_none,
    },
    .issuer = "Fyodor",
    .subject = "1234567890",
    .audience = "unit test",
    .expirationTime = 123456789llu,
    .notBefore = 9876543210llu,
    .issuedAt = 999999999llu,
    .jwtId = 333333333lu
  };
  ShortLivedHeap *slh;
  int size;
  char *encoded;
  int rc;

  slh = makeShortLivedHeap(8192, 1024);
  rc = jwtEncode(&j, true, NULL, slh, &size, &encoded);
  assert(rc == 0);

  printf("encoded value: [%.*s]\n", size, encoded);
  assert(strcmp(encoded, correctResult) == 0);

  SLHFree(slh);
}

static void shouldSignWithRS256(void) {

  printf("\n *** should sign with RS256\n");

  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_RS256",
      CKO_PRIVATE_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  const Jwt jwt = {
    .header = (JwsHeader) {
      .algorithm = JWS_ALGORITHM_RS256
    },
    .subject = "1234567890",
    .issuedAt = 1516239022llu
  };
  int encodeRc;
  int encodedSize;
  char *token = jwtEncodeToken(ctx, &jwt, true, slh, &encodedSize,  &encodeRc);
  assert(encodeRc == 0);
  assert(token != NULL);
  //assert(strcmp(token, correctResult) == 0);

  printf("token: %s\n", token);

  jwtContextDestroy(ctx);
  SLHFree(slh);
}

static void shouldSignWithHS256(void) {

  printf("\n *** should sign with HS256\n");

  /* verified at jwt.io */
  static const char *const correctResult = "eyJhbGciOiJIUzI1NiJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".865VU7SWfWv0GeHgtWXd6Drwxb4WKs-WB9ztO9PyDRs";

  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  const Jwt jwt = {
    .header = (JwsHeader) {
      .algorithm = JWS_ALGORITHM_HS256
    },
    .subject = "1234567890",
    .issuedAt = 1516239022llu
  };
  int encodeRc;
  int encodedSize;
  char *token = jwtEncodeToken(ctx, &jwt, true, slh, &encodedSize,  &encodeRc);
  assert(encodeRc == 0);
  assert(token != NULL);

  printf("token: %s\n", token);
  assert(strcmp(token, correctResult) == 0);

  jwtContextDestroy(ctx);
  SLHFree(slh);
}

static void shouldSignWithHS384(void) {

  printf("\n *** should sign with HS384\n");


  /* verified at jwt.io */
  static const char *const correctResult = "eyJhbGciOiJIUzM4NCJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".o_-52tWowsL_hDd5XJXLklhhABKzl655iA0pcrzHtMHWdgP8RdnLFwX0yK-Q-7Ed";

  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  const Jwt jwt = {
    .header = (JwsHeader) {
      .algorithm = JWS_ALGORITHM_HS384
    },
    .subject = "1234567890",
    .issuedAt = 1516239022llu
  };
  int encodeRc;
  int encodedSize;
  char *token = jwtEncodeToken(ctx, &jwt, true, slh, &encodedSize,  &encodeRc);
  assert(encodeRc == 0);
  assert(token != NULL);

  printf("token: %s\n", token);
  assert(strcmp(token, correctResult) == 0);

  jwtContextDestroy(ctx);
  SLHFree(slh);
}

static void shouldSignWithHS512(void) {

  printf("\n *** should sign with HS512\n");


  /* verified at jwt.io */
  static const char *const correctResult = "eyJhbGciOiJIUzUxMiJ9"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwiaWF0IjoxNTE2MjM5MDIyfQ"
      ".r6_nsKv-s96EVIIbUmN_8vOhkFnjKLh0c_-BY2__KfR8VBwM2qv6KwL4XfNH29njHL-oAPyQuR-8l02QSm00tA";

  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  const Jwt jwt = {
    .header = (JwsHeader) {
      .algorithm = JWS_ALGORITHM_HS512
    },
    .subject = "1234567890",
    .issuedAt = 1516239022llu
  };
  int encodeRc;
  int encodedSize;
  char *token = jwtEncodeToken(ctx, &jwt, true, slh, &encodedSize,  &encodeRc);
  assert(encodeRc == 0);
  assert(token != NULL);

  printf("token: %s\n", token);
  assert(strcmp(token, correctResult) == 0);

  jwtContextDestroy(ctx);
  SLHFree(slh);
}

void shouldCorrectlyValidateBasicClaims(void) {
  printf("\n *** should Correctly Validate Basic Claims\n");

  static const char JWT_EXP_2038_AUD_TEST[] = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwiaWF0IjoxNTE2MjM5MDIyLCJleHAiOjIxNDc0ODM2NDcs"
      "ImF1ZCI6IlRlc3QifQ";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_EXP_2038_AUD_TEST, true,
      slh, &parseRc);
  assert(parseRc == RC_JWT_INSECURE);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  bool valid = jwtAreBasicClaimsValid(j, "Test");
  assert(valid);

  valid = jwtAreBasicClaimsValid(j, "Test1");
  assert(!valid);

  SLHFree(slh);
}

void shouldRejectExpiredJWT(void) {
  printf("\n *** should Reject Expired JWT\n");

  static const char JWT_EXPIRED[] = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"
      ".eyJzdWIiOiIxMjM0NTY3ODkwIiwiaWF0IjoxNTE2MjM5MDIyLCJleHAiOjE1MTYyMzkwMjIs"
      "ImF1ZCI6IlRlc3QifQ";
  int makeVerRc = 0, p11Rc = 0, p11Rsn = 0;
  ShortLivedHeap *const slh = makeShortLivedHeap(8192, 1024);
  JwtContext *const ctx = makeJwtContextForKeyInToken(
      TOKEN_NAME,
      "KEY_HMAC",
      CKO_SECRET_KEY,
      &makeVerRc,
      &p11Rc,
      &p11Rsn);
  assert(makeVerRc == 0);

  int parseRc;
  Jwt *const j = jwtVerifyAndParseToken(ctx, JWT_EXPIRED, true,
      slh, &parseRc);
  assert(parseRc == RC_JWT_INSECURE);
  assert(j != NULL);

  jwtContextDestroy(ctx);

  bool valid = jwtAreBasicClaimsValid(j, "Test");
  assert(!valid);

  SLHFree(slh);
}

static int getOrCreateHMACKey(
                           const char* in_tokenName, const char* in_keyLabel,
                           const unsigned char *in_keydata, int in_keydata_len,
                           ICSFP11_HANDLE_T **out_tokenHandle,
                           ICSFP11_HANDLE_T **out_keyHandle,
                           int *out_p11rc, int *out_p11rsn)
{
  int sts = 0;
  ICSFP11_HANDLE_T *tokenHandle = NULL;
  ICSFP11_HANDLE_T *keyHandle = NULL;
  ICSFP11_HANDLE_T *foundHandles = NULL;
  int numfound = 0;

#define ZOWETOKEN_MANUFACTURER "Rocket Software"
#define ZOWETOKEN_MODEL        "RSICSFP11"
#define ZOWETOKEN_SERIAL       "1"
  ICSFP11_TOKENATTRS_T zoweTokenAttrs = ICSFP11_TOKENATTRS_INITIALIZER;
  memcpy(&(zoweTokenAttrs.manufacturer[0]), ZOWETOKEN_MANUFACTURER, strlen(ZOWETOKEN_MANUFACTURER));
  memcpy(&(zoweTokenAttrs.model[0]), ZOWETOKEN_MODEL, strlen(ZOWETOKEN_MODEL));
  memcpy(&(zoweTokenAttrs.serial[0]), ZOWETOKEN_SERIAL, strlen(ZOWETOKEN_SERIAL));

  do {
    DEBUG("attempting to address the desired token...\n");
    sts = rs_icsfp11_findToken(in_tokenName, &tokenHandle, out_p11rc, out_p11rsn);
    if ((0 == sts) && (0 == *out_p11rc) && (0 == *out_p11rsn))
    {
      DEBUG("token found\n");
      *out_tokenHandle = tokenHandle;
      break;
    }
    DEBUG("attempting to create the token...\n");
    sts = rs_icsfp11_createToken(in_tokenName, &zoweTokenAttrs,
                                 &tokenHandle, out_p11rc, out_p11rsn);
    if ((0 == sts) && (0 == *out_p11rc) && (0 == *out_p11rsn)) {
      DEBUG("token created\n");
      *out_tokenHandle = tokenHandle;
    }
  } while(0);

  if ((0 != sts) || (NULL == *out_tokenHandle)) {
    return sts;
  }

  do {
    DEBUG("attempting to address the key record...\n");
    sts = rs_icsfp11_findObjectsByLabel(tokenHandle, in_keyLabel, NULL,
                                        &foundHandles, &numfound,
                                        out_p11rc, out_p11rsn);
    if ((0 == sts) && (0 == *out_p11rc) && (0 == *out_p11rsn) && (0 < numfound))
    {
      if (1 < numfound) {
        printf("Warning: More than one key record found for label: %s\n", in_keyLabel);
      }
      DEBUG("key record found\n");
      keyHandle = (ICSFP11_HANDLE_T*) safeMalloc(sizeof(ICSFP11_HANDLE_T), "keyhandle");
      memcpy(keyHandle, &(foundHandles[0]), sizeof(ICSFP11_HANDLE_T));
      *out_keyHandle = keyHandle;
      break;
    }

    if ((NULL != in_keydata) && (0 < in_keydata_len)) {
      DEBUG("attempting to create the key record...\n");
      sts = rs_icsfp11_hmacTokenKeyCreateFromRaw(tokenHandle, in_keyLabel, NULL,
                                                 in_keydata, in_keydata_len,
                                                 &keyHandle, out_p11rc, out_p11rsn);
      if ((0 == sts) && (0 == *out_p11rc) && (0 == *out_p11rsn))
      {
        DEBUG("record created\n");
        *out_keyHandle = keyHandle;
        break;
      }
    }
  } while(0);

  if (foundHandles && numfound) {
    safeFree(foundHandles, numfound * sizeof(ICSFP11_HANDLE_T));
  }

  return sts;
}

void initializeTokenForHMAC(void) {
  ICSFP11_HANDLE_T *out_tokenHandle;
  ICSFP11_HANDLE_T *out_keyHandle;
  int out_p11rc, out_p11rsn;

  printf(" *** Initializing the HMAC key...\n");
  const int rc = getOrCreateHMACKey(TOKEN_NAME,
      "KEY_HMAC",
      HMAC_KEY,
      sizeof (HMAC_KEY),
      &out_tokenHandle,
      &out_keyHandle,
      &out_p11rc,
      &out_p11rsn);
  assert (rc == 0);

  safeFree(out_tokenHandle, sizeof (*out_tokenHandle));
  safeFree(out_keyHandle, sizeof (*out_keyHandle));
}

int main(int argc, char **argv) {

  initializeTokenForHMAC();

  shouldParseNativeJwt();
  shouldParseAsciiJWT();
  shouldRejectMissingSignature();

  shouldVerifyHS256();
  shouldRejectInvalidHS256();
  shouldVerifyHS384();
  shouldRejectInvalidHS384();
  shouldVerifyHS512();
  shouldRejectInvalidHS512();
  shouldVerifyRS256();
  shouldRejectInvalidRS256();

  shouldEncodeASimpleJWT();

  shouldSignWithRS256();
  shouldSignWithHS256();
  shouldSignWithHS384();
  shouldSignWithHS512();

  shouldCorrectlyValidateBasicClaims();
  shouldRejectExpiredJWT();

#ifdef TRACK_MEMORY
  int showOutstanding(void); /* enabled in alloc.c with -DTRACK_MEMORY */
  printf("\n *** Checking for memory leaks...\n");
  showOutstanding();
#else
  printf("\n *** Compile with -DTRACK_MEMORY to check for memory leaks...\n");
#endif /* TRACK_MEMORY */
  return 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
