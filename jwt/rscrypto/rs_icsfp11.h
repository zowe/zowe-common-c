/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef RS_ICSFP11_H_
#define RS_ICSFP11_H_

#ifdef __ZOWE_OS_ZOS
#include <csnpdefs.h>
#include <csfbext.h>
#else
typedef char CK_UTF8CHAR;
typedef char CK_CHAR;
#endif

typedef struct RS_ICSFP11_HANDLE_tag {
  char tokenName[32];
  char sequenceNumber[8];
  char id[4];
} ICSFP11_HANDLE_T;

static const ICSFP11_HANDLE_T ICSFP11_HANDLE_INITIALIZER = {
    {' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' '}, /* 32 blanks */
    {' ',' ',' ',' ',' ',' ',' ',' '}, /*  8 blanks */
    {' ',' ',' ',' '}          /*  4 blanks */
  };

typedef struct RS_ICSFP11_TOKEN_ATTRS_tag {
  CK_UTF8CHAR   manufacturer[32];    /* blank padded */
  CK_UTF8CHAR   model[16];           /* blank padded */
  CK_CHAR       serial[16];          /* blank padded */
  CK_CHAR       reserved[4];         /* must be hex zeroes */
} ICSFP11_TOKENATTRS_T;

static const ICSFP11_TOKENATTRS_T ICSFP11_TOKENATTRS_INITIALIZER = {
    {' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' '}, /* 32 blanks */
    {' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' '}, /* 16 blanks */
    {' ',' ',' ',' ',' ',' ',' ',' ',
     ' ',' ',' ',' ',' ',' ',' ',' '}, /* 16 blanks */
    {0x00, 0x00, 0x00, 0x00}           /* 4 hex zeroes */
  };


typedef struct RS_ICSFP11_ENVIRONMENT_tag {

  /* CSFIQF2[6] output, byte 10, bit 2 */
  int p11SecureKeyAvailable;

  /* CSFIQF[6] ICSFSP11 keyword outputs */
  int p11MasterKeyActive;
  int fipsEnforcementMode; /* 0 - none, 1 - FIPS compat, 2 - FIPS mode */

  /* CSFIQF[6] STATP11 keywork outputs */
  //int p11NMKState; /* 1 - clear, 2 - uncommitted, 3 - committed */
  //int p11CMKState; /* 1 - clear, 2 - contains a key */
  //int complianceMode; /* 8-byte hex digit sum of 1, 2, 4, 8: FIPS'09,BSI'09,FIPS'11,BSI'11 respectively */
  //int firmwareVersion; /* 8-byte hex value */
  //int serialNumber; /* factory-assigned serial number of the coprocessor */

  /* information gleaned from Token Record List service */
  int numTokensAccessible;
  ICSFP11_HANDLE_T *tokensHandlesArray;

} ICSFP11_ENV_T;

typedef enum ICSFP11_DIGEST_ALG_tag {
  ICSFP11_ALG_INVALID=0,
  ICSFP11_SHA1=1,
  ICSFP11_SHA256=2,
  ICSFP11_SHA384=3,
  ICSFP11_SHA512=4
} ICSFP11_DIGEST_ALG;

#define ICSFP11_SHA1_HASHLEN    20
#define ICSFP11_SHA256_HASHLEN  32
#define ICSFP11_SHA384_HASHLEN  48
#define ICSFP11_SHA512_HASHLEN  64

/**
 * Use the ICSF Query Facilities and Token Record List callable service to
 * get a holistic view of the ICSF PKCS#11 Environment.  What special
 * functions are available, as well as accessible virtual tokens.
 */
int rs_icsfp11_getEnvironment(ICSFP11_ENV_T **out_env,
                              int *out_rc, int *out_reason);

void rs_icsfp11_envDescribe(const ICSFP11_ENV_T *in_env);
void rs_icsfp11_envDestroy(const ICSFP11_ENV_T *in_env);

/**
 * Generate the requested number of random bytes using the PRNG method,
 * and copy them to the caller-supplied buffer.
 */
int rs_icsfp11_getRandomBytes(const ICSFP11_HANDLE_T *token_handle,
                              unsigned char *randbuf, int *randbuf_len,
                              int *out_rc, int *out_reason);

/* JOE - this is a duplicate of a later header */
/*
int rs_icsfp11_createToken(const char *in_name,
                           ICSFP11_TOKENATTRS_T *in_tokenattrs,
                           ICSFP11_HANDLE_T **out_handle,
                           int *out_rc, int *out_reason);
*/

/**
 * If the token with the specified name exists, and the current user has access it,
 * return a handle to it.
 */
int rs_icsfp11_findToken(const char *in_token_name,
                         ICSFP11_HANDLE_T **out_handle,
                         int *out_rc, int *out_reason);

/**
 * rs_icsfp11_findObjectsByLabel
 * rs_icsfp11_findObjectsByLabelAndClass
 *
 * Get token object handles (e.g. the handle to a key) using
 * a search label (and optionally, an app name).
 *
 * The expected / hoped-for output value for num_found is 1, but
 * could be as many as 10.  If the calling application expect there
 * to be exactly one object in the token for the given label, but
 * a value greater than 1 is returned in numfound, that's a condition
 * worth noting.
 *
 * The size of the buffer behind out_handles (if numfound > 0) will
 * always be numfound * sizeof(ICSFP11_HANDLE_T).  This buffer is
 * allocated inside the call and should be freed by the caller.
 *
 * The rs_icsfp11_findObjectsByLabelAndClass can be used to tighten
 * the search scope by "object class."  This is particularly useful
 * in signing/verification use cases where gskkyman was used to
 * import certificates or cryptographic identities to the PKCS#11
 * token, resulting in multiple token objects with the same label
 * but different classes.  Expected in_objectClass values include:
 *      CKO_DATA
 *      CKO_CERTIFICATE
 *      CKO_PUBLIC_KEY
 *      CKO_PRIVATE_KEY
 *      CKO_SECRET_KEY
 * These values are defined in csnpdefs.h
 */
int rs_icsfp11_findObjectsByLabel(
                    const ICSFP11_HANDLE_T *in_token_handle,
                    const char *in_keylabel,
                    const char *in_appname,
                    ICSFP11_HANDLE_T **out_handles,
                    int *out_numfound,
                    int *out_rc, int *out_reason);

/* If you don't know the desired objectClass, use rs_icsfp11_findObjectsByLabel. */
int rs_icsfp11_findObjectsByLabelAndClass(
                    const ICSFP11_HANDLE_T *in_token_handle,
                    const char *in_label,
                    unsigned int in_objectClass,
                    const char *in_appname,
                    ICSFP11_HANDLE_T **out_handles,
                    int *out_numfound,
                    int *out_rc, int *out_reason);

/**
 * Delete an object using its unique handle.
 */
int rs_icsfp11_deleteObject(const ICSFP11_HANDLE_T *in_object_handle,
                            int *out_rc, int *out_reason);

/**
 * Set or retrieve data from an object's CKA_APPLICATION attribute.
 * In practice, set only works on CKO_DATA objects because other objects only allow
 * the CKA_APPLICATION attribute to be set at create/generate time.
 */
int rs_icsfp11_setObjectApplicationAttribute(const ICSFP11_HANDLE_T *in_object_handle,
                                             const unsigned char* in_attribute_value,
                                             int in_value_len,
                                             int *out_rc, int *out_reason);

int rs_icsfp11_getObjectApplicationAttribute(const ICSFP11_HANDLE_T *in_object_handle,
                                             unsigned char* out_attribute_value,
                                             int* out_value_len,
                                             int *out_rc, int *out_reason);

int rs_icsfp11_getObjectValue(const ICSFP11_HANDLE_T *in_object_handle,
                              unsigned char* out_value, int* out_value_len,
                              int *out_rc, int *out_reason);

/**
 * Create a token by name (max 32 chars).
 */
int rs_icsfp11_createToken(const char *in_name,
                           const ICSFP11_TOKENATTRS_T *in_tokenattrs,
                           ICSFP11_HANDLE_T **out_handle,
                           int *out_rc, int *out_reason);

/**
 * Delete a token by handle, or by name.
 */
int rs_icsfp11_deleteTokenByHandle(const ICSFP11_HANDLE_T *in_handle,
                                   int *out_rc, int* out_reason);

int rs_icsfp11_deleteTokenByName(const char *in_name,
                                 int *out_rc, int *out_reason);

/**
 * Compute HMACs
 */
int rs_icsfp11_hmacSHA1(const ICSFP11_HANDLE_T *in_key_handle,
                        const unsigned char* in_cleartext, int in_cleartext_len,
                        unsigned char* out_hmac, int *out_hmac_len,
                        int *out_rc, int *out_reason);

int rs_icsfp11_hmacSHA256(const ICSFP11_HANDLE_T *in_key_handle,
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_hmac, int *out_hmac_len,
                          int *out_rc, int *out_reason);

int rs_icsfp11_hmacSHA384(const ICSFP11_HANDLE_T *in_key_handle,
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_hmac, int *out_hmac_len,
                          int *out_rc, int *out_reason);

int rs_icsfp11_hmacSHA512(const ICSFP11_HANDLE_T *in_key_handle,
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_hmac, int *out_hmac_len,
                          int *out_rc, int *out_reason);

/**
 * RS256 signatures and verification for JWT support
 */
int rs_icsfp11_RS256_sign(const ICSFP11_HANDLE_T *in_key_handle, /* RSA privkey handle */
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_sig, int *out_sig_len,
                          int *out_rc, int *out_reason);

int rs_icsfp11_RS256_verify(const ICSFP11_HANDLE_T *in_key_handle, /* RSA pubkey handle */
                            const unsigned char* in_cleartext, int in_cleartext_len,
                            const unsigned char* in_sig, int in_sig_len,
                            int *out_rc, int *out_reason);

#endif /* RS_ICSFP11_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
