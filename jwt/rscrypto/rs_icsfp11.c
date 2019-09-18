/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include "rs_rsclibc.h"

#include "rs_icsfp11.h"
#include "rs_crypto_errors.h"

#ifdef _LP64
#pragma linkage(CSFPHMG6,OS) /* hmac generate */
#pragma linkage(CSFPGSK6,OS) /* gen secret key */
#pragma linkage(CSFPPRF6,OS) /* random */
#pragma linkage(CSFPTRC6,OS) /* token record create */
#pragma linkage(CSFPTRD6,OS) /* token record delete */
#pragma linkage(CSFPTRL6,OS) /* token record list */
#pragma linkage(CSFPSKE6,OS) /* secret key encrypt */
#pragma linkage(CSFPSKD6,OS) /* secret key decrypt */
#pragma linkage(CSFPSAV6,OS) /* set attribute value */
#pragma linkage(CSFPGAV6,OS) /* get attribute value */
#pragma linkage(CSFPOWH6,OS) /* one-way hash, sign, or verify */
#else
#pragma linkage(CSFPHMG,OS) /* hmac generate */
#pragma linkage(CSFPGSK,OS) /* gen secret key */
#pragma linkage(CSFPPRF,OS) /* random */
#pragma linkage(CSFPTRC,OS) /* token record create */
#pragma linkage(CSFPTRD,OS) /* token record delete */
#pragma linkage(CSFPTRL,OS) /* token record list */
#pragma linkage(CSFPSKE,OS) /* secret key encrypt */
#pragma linkage(CSFPSKD,OS) /* secret key decrypt */
#pragma linkage(CSFPSAV,OS) /* set attribute value */
#pragma linkage(CSFPGAV,OS) /* get attribute value */
#pragma linkage(CSFPOWH,OS) /* one-way hash, sign, or verify */
#endif

/**
 * Utilities for internal use
 */
static int rs_icsfp11_hmacSHA_internal (
                    const ICSFP11_HANDLE_T *in_key_handle,
                    ICSFP11_DIGEST_ALG in_alg,
                    const unsigned char* in_cleartext, int in_cleartext_len,
                    unsigned char* out_hmac, int *out_hmac_len,
                    int *out_rc, int *out_reason);

static int rs_icsfp11_findObjects_internal (
                    const ICSFP11_HANDLE_T *in_token_handle,
                    int in_numattrs, const CK_ATTRIBUTE *in_attrs,
                    ICSFP11_HANDLE_T **out_handles,
                    int *out_numfound,
                    int *out_rc, int *out_reason);

static int rs_icsfp11_createObject_internal(const ICSFP11_HANDLE_T *in_token_handle,
                                            int in_numattrs, const CK_ATTRIBUTE *in_attrs,
                                            ICSFP11_HANDLE_T **out_obj_handle,
                                            int *out_rc, int *out_reason);


static int serialize_attribute_list(int num_attrs,
                                    const CK_ATTRIBUTE *attr_array,
                                    unsigned char **out_attrlist_buf,
                                    int *out_attrlist_len);

/* parse_attribute_list allocates new CK_ATTRIBUTE structures, but those structures
 * point into the supplied in_serialized_attrlist buffer */
static int parse_attribute_list(const unsigned char* in_serialized_attrlist,
                                int in_serialized_attrlist_len,
                                CK_ATTRIBUTE** out_attr_array, int* out_num_attrs);

static const char RS_TOKEN_KEYWORD[8] =  {'T','O','K','E','N',' ',' ',' '};
static const char RS_OBJECT_KEYWORD[8] = {'O','B','J','E','C','T',' ',' '};
static const char RS_PRNG_KEYWORD[8] =   {'P','R','N','G',' ',' ',' ',' '};
static const char RS_KEY_KEYWORD[8] =    {'K','E','Y',' ',' ',' ',' ',' '};
static const char RS_SHA1_KEYWORD[8] =   {'S','H','A','-','1',' ',' ',' '};
static const char RS_SHA256_KEYWORD[8] = {'S','H','A','-','2','5','6',' '};
static const char RS_SHA384_KEYWORD[8] = {'S','H','A','-','3','8','4',' '};
static const char RS_SHA512_KEYWORD[8] = {'S','H','A','-','5','1','2',' '};
static const char RS_ONLY_KEYWORD[8] =   {'O','N','L','Y',' ',' ',' ',' '};

typedef struct TOKEN_OUTPUT_RECORD_tag {
  /* first part is just like a CK_TOKEN_INFO */
  CK_UTF8CHAR   label[32];           /* blank padded */
  CK_UTF8CHAR   manufacturerID[32];  /* blank padded */
  CK_UTF8CHAR   model[16];           /* blank padded */
  CK_CHAR       serialNumber[16];    /* blank padded */
  /* then it changes */
  CK_DATE       dateLastUpdated;     /* yyyymmdd */
  CK_CHAR       timeLastUpdated[8];  /* hhmmssth */
  unsigned int  flags;               /* 32 bits, bit 0 on - token is write protected */
} ICSFP11_TOKEN_RECORD_T;

int rs_icsfp11_getEnvironment(ICSFP11_ENV_T **out_env,
                              int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;
  ICSFP11_ENV_T *env = NULL;

  int exit_data_len = 0;
  char last_handle[44] = {0};
  int rule_array_count = 1;
  int search_template_len = 0;
  char* output_list = NULL;
  int output_list_length;
  int handle_count;

  int i;

  char* readhead = NULL;

  do {
    if ((NULL == out_env) ||
        (NULL == out_rc) ||
        (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }
    env = (ICSFP11_ENV_T *)rscAlloc(sizeof(ICSFP11_ENV_T), "icsfp11 env");

#define ICSFP11_MAX_TOKENS_SUPPORTED  16
    output_list_length = ICSFP11_MAX_TOKENS_SUPPORTED * sizeof(ICSFP11_TOKEN_RECORD_T);
    output_list = (char*)rscAlloc(output_list_length, "token records output");
    handle_count = ICSFP11_MAX_TOKENS_SUPPORTED;

    memset(last_handle, 0x40, sizeof(last_handle)); /*set handle to all blanks*/
    /* use the Token Record List service to get the accessible token handles */
#ifdef _LP64
    CSFPTRL6 (
#else
    CSFPTRL (
#endif
        out_rc,
        out_reason,
        &exit_data_len,
        NULL, /* no exit data */
        (unsigned char*)last_handle,
        &rule_array_count,
        (unsigned char*)RS_TOKEN_KEYWORD,
        &search_template_len,
        NULL, /* no search template */
        &output_list_length,
        &handle_count,
        (unsigned char*)output_list);

    if (8 <= *out_rc) {
      /* we weren't able to extract token information for some reason */
      rscFree(env, sizeof(ICSFP11_ENV_T));
      *out_env = NULL;
      status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
      break;
    }

#ifdef DEBUG
    printf("after token record list, output_list contains:\n");
    dumpbuffer(output_list, output_list_length);
#endif

    env->numTokensAccessible = handle_count;
    env->tokensHandlesArray = (ICSFP11_HANDLE_T*)rscAlloc(handle_count * sizeof(ICSFP11_HANDLE_T), "token handles");
    readhead = output_list;

    /* copy the token names into the token handles array */
    for (i=0; i < handle_count; i++) {
      memset((&(env->tokensHandlesArray[i])), 0x40, sizeof(ICSFP11_HANDLE_T));
      memcpy(env->tokensHandlesArray[i].tokenName, readhead, 32);
      readhead += sizeof(ICSFP11_TOKEN_RECORD_T);
    }
    /*TODO: call query facilities to fill in rest of environment data */

    *out_env = env;

  } while(0);

  if (output_list) {
    rscFree(output_list, ICSFP11_MAX_TOKENS_SUPPORTED * sizeof(ICSFP11_TOKEN_RECORD_T));
  }

  return status;
}

void rs_icsfp11_envDescribe(const ICSFP11_ENV_T *in_env)
{
  char safeString[45] = {0};
  int i;
  if (in_env) {
    printf("ICSFP11 Environment Description for env at 0x%p:\n", in_env);
    printf("numTokensAccessible: %d\n", in_env->numTokensAccessible);
    for (i=0; i < in_env->numTokensAccessible; i++)
    {
      memcpy(safeString, &(in_env->tokensHandlesArray[i]), 44);
      printf("tokenHandlesArray[%d] = <%s>\n", i, safeString);
      memset(safeString, 0x00, sizeof(safeString));
    }
    printf("- - -\n");
  }
}

void rs_icsfp11_envDestroy(ICSFP11_ENV_T *in_env)
{
  if (in_env) {
    if ((0 < in_env->numTokensAccessible) && (NULL != in_env->tokensHandlesArray)) {
      rscFree(in_env->tokensHandlesArray, in_env->numTokensAccessible * sizeof(ICSFP11_HANDLE_T));
    }
    memset(in_env, 0x00, sizeof(ICSFP11_ENV_T));
    rscFree(in_env, sizeof(ICSFP11_ENV_T));
  }
}

int rs_icsfp11_getRandomBytes(const ICSFP11_HANDLE_T *token_handle,
                              unsigned char *randbuf, int *randbuf_len,
                              int *out_rc, int *out_reason)
{
  int status = 0;

  int exit_data_len = 0;
  int rule_array_count = 1;
  int parms_list_len = 0;

  ICSFP11_HANDLE_T cleanHandle = ICSFP11_HANDLE_INITIALIZER;

  do {
    if ((NULL == token_handle) ||
        (NULL == randbuf) || (0 == randbuf_len) ||
        (NULL == out_rc) || (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }
    memcpy(&(cleanHandle.tokenName[0]), &(token_handle->tokenName[0]), sizeof(cleanHandle.tokenName));

#ifdef _LP64
    CSFPPRF6(
#else
    CSFPPRF(
#endif
        out_rc,
        out_reason,
        &exit_data_len,
        NULL, /* no exit data */
        &rule_array_count,
        (unsigned char*) RS_PRNG_KEYWORD,
        (unsigned char*)(void *) &cleanHandle,
        &parms_list_len,
        NULL, /* no parms list */
        randbuf_len,
        randbuf);

  } while(0);

  return status;
}

int rs_icsfp11_hmacTokenKeyCreateFromRaw(
                    ICSFP11_HANDLE_T *in_token_handle,
                    char* in_keylabel,
                    char* in_appname,
                    const unsigned char* in_keydata, int in_keydata_len,
                    ICSFP11_HANDLE_T **out_key_handle,
                    int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  unsigned int objectclass = CKO_SECRET_KEY;
  unsigned int keytype = CKK_GENERIC_SECRET;
  CK_BBOOL ontoken = CK_TRUE;
  CK_BBOOL cansign = CK_TRUE;
  CK_ATTRIBUTE hmac_key_attrs[7] = {
      { CKA_CLASS, &objectclass, sizeof(unsigned int) },
      { CKA_KEY_TYPE, &keytype, sizeof(unsigned int) },
      { CKA_TOKEN, &ontoken, sizeof(CK_BBOOL) },
      { CKA_SIGN, &cansign, sizeof(CK_BBOOL) },
      { CKA_VALUE, (char *)in_keydata, in_keydata_len },
      { CKA_LABEL, NULL, 0 },
      { CKA_APPLICATION, NULL, 0 }
  };
  int num_key_attrs=0;

  do {
    if ((NULL == in_token_handle) ||
        (NULL == in_keylabel) ||
        (NULL == out_key_handle) ||
        (NULL == out_rc) ||
        (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    hmac_key_attrs[5].pValue = in_keylabel;
    hmac_key_attrs[5].ulValueLen = strlen(in_keylabel);
    if (32 < hmac_key_attrs[5].ulValueLen) {
      status = RSCRYPTO_INVALID_IDENTIFIER_LENGTH;
      break;
    }

    if (in_appname) {
      hmac_key_attrs[6].pValue = in_appname;
      hmac_key_attrs[6].ulValueLen = strlen(in_appname);
      num_key_attrs = 7;
    } else {
      num_key_attrs = 6;
    }

    /* we use the Create Token Record service when we want
     * to pass our own key data */
    status = rs_icsfp11_createObject_internal(in_token_handle,
                                              num_key_attrs, (CK_ATTRIBUTE*)hmac_key_attrs,
                                              out_key_handle,
                                              out_rc, out_reason);

  } while(0);

  return status;
}

int rs_icsfp11_findObjectsByLabelAndClass(
                    ICSFP11_HANDLE_T *in_token_handle,
                    char* in_label,
                    CK_ULONG in_objectClass,
                    char* in_appname,
                    ICSFP11_HANDLE_T **out_handles,
                    int *out_numfound,
                    int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  CK_ATTRIBUTE object_attrs[3] = {
      { CKA_CLASS, &in_objectClass, sizeof(CK_OBJECT_CLASS) },
      { CKA_LABEL, NULL, 0 },
      { CKA_APPLICATION, NULL, 0 }
  };
  int num_attrs = 0;

  do {
    if (NULL == in_label) {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    object_attrs[1].pValue = in_label;
    object_attrs[1].ulValueLen = strlen(in_label);
    if (32 < object_attrs[1].ulValueLen) {
      status = RSCRYPTO_INVALID_IDENTIFIER_LENGTH;
      break;
    }

    if (in_appname) {
      object_attrs[2].pValue = in_appname;
      object_attrs[2].ulValueLen = strlen(in_appname);
      num_attrs = 3;
    } else {
      num_attrs = 2;
    }

    status = rs_icsfp11_findObjects_internal(in_token_handle,
                                             num_attrs, (CK_ATTRIBUTE*)object_attrs,
                                             out_handles,
                                             out_numfound,
                                             out_rc, out_reason);

  } while(0);

  return status;
}

int rs_icsfp11_findObjectsByLabel(
                    ICSFP11_HANDLE_T *in_token_handle,
                    char* in_label,
                    char* in_appname,
                    ICSFP11_HANDLE_T **out_handles,
                    int *out_numfound,
                    int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  CK_ATTRIBUTE object_attrs[2] = {
      { CKA_LABEL, NULL, 0 },
      { CKA_APPLICATION, NULL, 0 }
  };
  int num_attrs = 0;

  do {
    if (NULL == in_label) {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    object_attrs[0].pValue = in_label;
    object_attrs[0].ulValueLen = strlen(in_label);
    if (32 < object_attrs[0].ulValueLen) {
      status = RSCRYPTO_INVALID_IDENTIFIER_LENGTH;
      break;
    }

    if (in_appname) {
      object_attrs[1].pValue = in_appname;
      object_attrs[1].ulValueLen = strlen(in_appname);
      num_attrs = 2;
    } else {
      num_attrs = 1;
    }

    status = rs_icsfp11_findObjects_internal(in_token_handle,
                                             num_attrs, (CK_ATTRIBUTE*)object_attrs,
                                             out_handles,
                                             out_numfound,
                                             out_rc, out_reason);

  } while(0);

  return status;
}

int rs_icsfp11_deleteObject(ICSFP11_HANDLE_T *in_object_handle,
                            int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 1;

#ifdef _LP64
  CSFPTRD6(
#else
  CSFPTRD(
#endif
      out_rc,
      out_reason,
      &exit_data_len,
      NULL, /* no exit data */
      (unsigned char*)(void *)in_object_handle,
      &rule_array_count,
      (unsigned char*)RS_OBJECT_KEYWORD);

  if (8 <= *out_rc) {
    status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
  }

  return status;
}

/**
 * Set or retrieve data from an object's CKA_APPLICATION attribute.
 */
int rs_icsfp11_setObjectApplicationAttribute(ICSFP11_HANDLE_T *in_object_handle,
                                             const unsigned char* in_attribute_value, int in_value_len,
                                             int *out_rc, int *out_reason)
{
  int sts = 0;
  int num_attrs = 1;
  CK_ATTRIBUTE object_attrs[1] = {
    { CKA_APPLICATION, NULL, 0 }
  };
  char* serialized_attrlist = NULL;
  int serialized_attrlist_len = 0;

  int exit_data_len = 0;
  int rule_array_count = 0;

  object_attrs[0].pValue = (char *)in_attribute_value;
  object_attrs[0].ulValueLen = (CK_ULONG) in_value_len;

  do {
    sts = serialize_attribute_list(num_attrs, object_attrs, &serialized_attrlist, &serialized_attrlist_len);
    if (0 != sts) {
      break;
    }

  #ifdef _LP64
    CSFPSAV6(
  #else
    CSFPSAV(
  #endif
        out_rc, out_reason,
        &exit_data_len, NULL, /* exit data length (should be zero), exit data (ignored) */
        (unsigned char*)(void *) in_object_handle,
        &rule_array_count, NULL, /* rule array count, rule array (ignored) */
        &serialized_attrlist_len,
        serialized_attrlist);

    if (8 <= *out_rc) {
      sts = RSCRYPTO_CALLABLE_SERVICE_ERROR;
    }

  } while(0);

  return sts;
}

int rs_icsfp11_getObjectApplicationAttribute(ICSFP11_HANDLE_T *in_object_handle,
                                             unsigned char* out_attribute_value, int* out_value_len,
                                             int *out_rc, int *out_reason)
{
  int sts = 0;

  CK_ATTRIBUTE *object_attrs = NULL;
  int num_attrs = 0;
  int didFindApplicationAttribute = 0;

#define ICSFP11_ATTRIBUTE_LIST_MAXLEN   32752
  char serialized_attrlist[ICSFP11_ATTRIBUTE_LIST_MAXLEN] = {0};
  int serialized_attrlist_len = ICSFP11_ATTRIBUTE_LIST_MAXLEN;

  int exit_data_len = 0;
  int rule_array_count = 0;

  do {

#ifdef _LP64
    CSFPGAV6(
#else
    CSFPGAV(
#endif
        out_rc, out_reason,
        &exit_data_len, NULL, /* exit data length (should be zero), exit data (ignored) */
        (unsigned char*)(void *) in_object_handle,
        &rule_array_count, NULL, /* rule array count, rule array (ignored) */
        &serialized_attrlist_len,
        serialized_attrlist);

    if (8 <= *out_rc) {
      sts = RSCRYPTO_CALLABLE_SERVICE_ERROR;
      break;
    }

    sts = parse_attribute_list(serialized_attrlist, serialized_attrlist_len, &object_attrs, &num_attrs);
    if (0 != sts) {
      break;
    }

    for (int i=0; i < num_attrs; i++) {
      if (CKA_APPLICATION == object_attrs[i].type) {
        didFindApplicationAttribute = 1;
        if (*out_value_len < object_attrs[i].ulValueLen) {
          sts = RSCRYPTO_INSUFFICIENT_OUTPUT_SPACE;
          break;
        }
        memcpy(out_attribute_value, object_attrs[i].pValue, object_attrs[i].ulValueLen);
        *out_value_len = object_attrs[i].ulValueLen;
        break;
      }
    }
    if (0 == didFindApplicationAttribute) {
      sts = RSCRYPTO_APPATTR_NOT_FOUND;
      break;
    }

  } while(0);

  if (NULL != object_attrs) {
    memset(object_attrs, 0x00, num_attrs * sizeof(CK_ATTRIBUTE));
    rscFree(object_attrs, num_attrs * sizeof(CK_ATTRIBUTE));
  }

  memset(serialized_attrlist, 0x00, ICSFP11_ATTRIBUTE_LIST_MAXLEN);
  return sts;
}

int rs_icsfp11_getObjectValue(const ICSFP11_HANDLE_T *in_object_handle,
                              unsigned char* out_value, int* out_value_len,
                              int *out_rc, int *out_reason)
{
  int sts = 0;

  CK_ATTRIBUTE *object_attrs = NULL;
  int num_attrs = 0;
  int didFindValueAttribute = 0;

  char serialized_attrlist[ICSFP11_ATTRIBUTE_LIST_MAXLEN] = {0};
  int serialized_attrlist_len = ICSFP11_ATTRIBUTE_LIST_MAXLEN;

  int exit_data_len = 0;
  int rule_array_count = 0;

  do {

#ifdef _LP64
    CSFPGAV6(
#else
    CSFPGAV(
#endif
        out_rc, out_reason,
        &exit_data_len, NULL, /* exit data length (should be zero), exit data (ignored) */
        (unsigned char*) in_object_handle,
        &rule_array_count, NULL, /* rule array count, rule array (ignored) */
        &serialized_attrlist_len,
        serialized_attrlist);

    if (8 <= *out_rc) {
      sts = RSCRYPTO_CALLABLE_SERVICE_ERROR;
      break;
    }

    sts = parse_attribute_list(serialized_attrlist, serialized_attrlist_len, &object_attrs, &num_attrs);
    if (0 != sts) {
      break;
    }

    for (int i=0; i < num_attrs; i++) {
      if (CKA_VALUE == object_attrs[i].type) {
        didFindValueAttribute = 1;
        if (*out_value_len < object_attrs[i].ulValueLen) {
          sts = RSCRYPTO_INSUFFICIENT_OUTPUT_SPACE;
          break;
        }
        memcpy(out_value, object_attrs[i].pValue, object_attrs[i].ulValueLen);
        *out_value_len = object_attrs[i].ulValueLen;
        break;
      }
    }
    if (0 == didFindValueAttribute) {
      sts = RSCRYPTO_APPATTR_NOT_FOUND;
      break;
    }

  } while(0);

  if (NULL != object_attrs) {
    memset(object_attrs, 0x00, num_attrs * sizeof(CK_ATTRIBUTE));
    rscFree(object_attrs, num_attrs * sizeof(CK_ATTRIBUTE));
  }

  memset(serialized_attrlist, 0x00, ICSFP11_ATTRIBUTE_LIST_MAXLEN);
  return sts;
}

int rs_icsfp11_deleteTokenByHandle(const ICSFP11_HANDLE_T *in_handle,
                                   int *out_rc, int* out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 1;

#ifdef _LP64
  CSFPTRD6(
#else
  CSFPTRD(
#endif
      out_rc,
      out_reason,
      &exit_data_len,
      NULL, /* no exit data */
      (unsigned char*)in_handle,
      &rule_array_count,
      (unsigned char*)RS_TOKEN_KEYWORD);

  if (8 <= *out_rc) {
    status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
  }

  return status;
}

int rs_icsfp11_deleteTokenByName(const char *in_name,
                                 int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;
  ICSFP11_HANDLE_T token_handle = ICSFP11_HANDLE_INITIALIZER;
  int namelen = 0;

  do {
    if (NULL == in_name) {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    namelen = strlen(in_name);
    if (32 < namelen) {
      status = RSCRYPTO_INVALID_IDENTIFIER_LENGTH;
      break;
    }
    memcpy(&(token_handle.tokenName[0]), in_name, namelen);
    status = rs_icsfp11_deleteTokenByHandle(&token_handle, out_rc, out_reason);

  } while(0);

  return status;
}

int rs_icsfp11_findToken(const char *in_token_name,
                         ICSFP11_HANDLE_T **out_handle,
                         int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int attr_list_len = 0;
  int rule_array_count = 1;
  int output_list_length;
  char* output_list = NULL;
  char* readhead = NULL;

  int handle_count;

  ICSFP11_HANDLE_T lasthandle = ICSFP11_HANDLE_INITIALIZER;
  int foundToken = 0;

  do {
    if ((NULL == in_token_name) ||
        (NULL == out_handle) ||
        (NULL == out_rc) ||
        (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    int namelen = strlen(in_token_name);
    if (32 < namelen) {
      status = RSCRYPTO_INVALID_IDENTIFIER_LENGTH;
      break;
    }

#define ICSFP11_FINDTOKEN_RECORDS_PER_ITERATION   16
    output_list_length = ICSFP11_FINDTOKEN_RECORDS_PER_ITERATION * sizeof(ICSFP11_TOKEN_RECORD_T);
    output_list = (char*)rscAlloc(output_list_length, "token handle output list");

    do {
#ifdef DEBUG
      printf("findToken iteration\n");
#endif

      handle_count = ICSFP11_FINDTOKEN_RECORDS_PER_ITERATION;

#ifdef DEBUG
      printf("lasthandle:\n");
      dumpbuffer((char*)&lasthandle, sizeof(ICSFP11_HANDLE_T));
#endif

      /* use the Token Record List service to get the next batch of token infos */
#ifdef _LP64
      CSFPTRL6 (
#else
      CSFPTRL (
#endif
        out_rc,
        out_reason,
        &exit_data_len,
        NULL, /* no exit data */
        (unsigned char*)&lasthandle,
        &rule_array_count,
        (unsigned char*)RS_TOKEN_KEYWORD,
        &attr_list_len, /* zero search template length */
        NULL, /* no search template */
        &output_list_length,
        &handle_count,
        (unsigned char*)output_list);
  #ifdef DEBUG
      printf("CSFPTRL rc=%d, rsn=0x%x, hc=%d, oll=%d, ol:\n",
             *out_rc, *out_reason, handle_count, output_list_length);
      dumpbuffer(output_list, output_list_length);
  #endif

      if (8 <= *out_rc) {
        *out_handle = NULL;
        status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
        break;
      }

      readhead = output_list;
      ICSFP11_TOKEN_RECORD_T *tmpRecord = NULL;

      for (int i=0; i < handle_count; i++) {
        tmpRecord = (ICSFP11_TOKEN_RECORD_T *)(void *) readhead;
#ifdef DEBUG
        printf("tmpRecord:\n");
        dumpbuffer((char*)tmpRecord, sizeof(ICSFP11_TOKEN_RECORD_T));
#endif
        if (0 == strncmp(&(tmpRecord->label[0]), in_token_name, namelen)) {
          foundToken = 1;
          *out_handle = (ICSFP11_HANDLE_T*) rscAlloc(sizeof(ICSFP11_HANDLE_T), "output handle");
          memcpy(&((*out_handle)->tokenName[0]), &(tmpRecord->label[0]), sizeof(tmpRecord->label));
          break;
        }
        readhead += sizeof(ICSFP11_TOKEN_RECORD_T);
      }

      if (0 == foundToken) { /* prepare to iterate */
        memcpy(&(lasthandle.tokenName[0]), &(tmpRecord->label[0]), sizeof(tmpRecord->label));
        output_list_length = ICSFP11_FINDTOKEN_RECORDS_PER_ITERATION * sizeof(ICSFP11_TOKEN_RECORD_T);
        memset(output_list, 0x00, output_list_length);
      }

    } while ((0 == foundToken) &&
             (4 == *out_rc) && (0xBC9 == *out_reason)); /* there are more results available */

  } while(0);

  if (output_list) {
    rscFree(output_list, ICSFP11_FINDTOKEN_RECORDS_PER_ITERATION * sizeof(ICSFP11_TOKEN_RECORD_T));
  }

  if (foundToken) { /* we might have been in 'more data available' state, of no interest to caller */
    *out_rc = 0;
    *out_reason = 0;
  } else {
    status = RS_PKCS11_TOKEN_NOTFOUND;
  }

  return status;
}

int rs_icsfp11_createToken(const char *in_name,
                           ICSFP11_TOKENATTRS_T *in_tokenattrs,
                           ICSFP11_HANDLE_T **out_handle,
                           int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 1;

  int attr_list_len = sizeof(ICSFP11_TOKENATTRS_T);

  ICSFP11_HANDLE_T inout_handle = ICSFP11_HANDLE_INITIALIZER;
  ICSFP11_HANDLE_T* newhandle = NULL;
  int namelen = 0;

  do {
    if ((NULL == in_name) ||
        (NULL == out_handle) ||
        (NULL == out_rc) || (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    namelen = strlen(in_name);
    if (32 < namelen) {
      status = RSCRYPTO_INVALID_IDENTIFIER_LENGTH;
      break;
    }
    memcpy(&(inout_handle.tokenName[0]), in_name, namelen);

#ifdef _LP64
    CSFPTRC6(
#else
    CSFPTRC(
#endif
        out_rc,
        out_reason,
        &exit_data_len,
        NULL, /* no exit data */
        (unsigned char*)&inout_handle,
        &rule_array_count,
        (unsigned char*)RS_TOKEN_KEYWORD,
        &attr_list_len,
        (unsigned char*)in_tokenattrs);

    if (8 <= *out_rc) {
      *out_handle = NULL;
      status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
      break;
    }

    newhandle = (ICSFP11_HANDLE_T*)rscAlloc(sizeof(ICSFP11_HANDLE_T), "token handle");
    memcpy(newhandle, &inout_handle, sizeof(ICSFP11_HANDLE_T));
    *out_handle = newhandle;

  } while(0);

  return status;
}

int rs_icsfp11_hmacSHA1(const ICSFP11_HANDLE_T *in_key_handle,
                        const unsigned char* in_cleartext, int in_cleartext_len,
                        unsigned char* out_hmac, int *out_hmac_len,
                        int *out_rc, int *out_reason)
{
  return rs_icsfp11_hmacSHA_internal(in_key_handle,
                                     ICSFP11_SHA1,
                                     in_cleartext, in_cleartext_len,
                                     out_hmac, out_hmac_len,
                                     out_rc, out_reason);
}

int rs_icsfp11_hmacSHA256(const ICSFP11_HANDLE_T *in_key_handle,
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_hmac, int *out_hmac_len,
                          int *out_rc, int *out_reason)
{
  return rs_icsfp11_hmacSHA_internal(in_key_handle,
                                     ICSFP11_SHA256,
                                     in_cleartext, in_cleartext_len,
                                     out_hmac, out_hmac_len,
                                     out_rc, out_reason);
}

int rs_icsfp11_hmacSHA384(const ICSFP11_HANDLE_T *in_key_handle,
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_hmac, int *out_hmac_len,
                          int *out_rc, int *out_reason)
{
  return rs_icsfp11_hmacSHA_internal(in_key_handle,
                                     ICSFP11_SHA384,
                                     in_cleartext, in_cleartext_len,
                                     out_hmac, out_hmac_len,
                                     out_rc, out_reason);
}

int rs_icsfp11_hmacSHA512(const ICSFP11_HANDLE_T *in_key_handle,
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_hmac, int *out_hmac_len,
                          int *out_rc, int *out_reason)
{
  return rs_icsfp11_hmacSHA_internal(in_key_handle,
                                     ICSFP11_SHA512,
                                     in_cleartext, in_cleartext_len,
                                     out_hmac, out_hmac_len,
                                     out_rc, out_reason);
}

int rs_icsfp11_RS256_sign(const ICSFP11_HANDLE_T *in_key_handle, /* RSA privkey handle */
                          const unsigned char* in_cleartext, int in_cleartext_len,
                          unsigned char* out_sig, int *out_sig_len,
                          int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 2;
  char rule_array[16] = {'S','H','A','-','2','5','6',' ',
                         'S','I','G','N','-','R','S','A'};
  int alet = 0;
  unsigned char chain_data[128] = {0};
  int chain_data_len = sizeof(chain_data);

#ifdef _LP64
  CSFPOWH6(
#else
  CSFPOWH(
#endif
           out_rc,
           out_reason,
           &exit_data_len,
           NULL, /* no exit data */
           &rule_array_count,
           (unsigned char*)rule_array,
           &in_cleartext_len,
           (unsigned char *)in_cleartext,
           &alet,
           &chain_data_len,
           chain_data,
           (unsigned char*)in_key_handle,
           out_sig_len,
           out_sig);
  if (8 <= *out_rc) {
    status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
  }

  return status;
}

int rs_icsfp11_RS256_verify(const ICSFP11_HANDLE_T *in_key_handle, /* RSA pubkey handle */
                            const unsigned char* in_cleartext, int in_cleartext_len,
                            const unsigned char* in_sig, int in_sig_len,
                            int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 2;
  char rule_array[16] = {'S','H','A','-','2','5','6',' ',
                         'V','E','R','-','R','S','A',' '};
  int alet = 0;
  unsigned char chain_data[128] = {0};
  int chain_data_len = sizeof(chain_data);

#ifdef _LP64
  CSFPOWH6(
#else
  CSFPOWH(
#endif
           out_rc,
           out_reason,
           &exit_data_len,
           NULL, /* no exit data */
           &rule_array_count,
           (unsigned char*)rule_array,
           &in_cleartext_len,
           (unsigned char *)in_cleartext,
           &alet,
           &chain_data_len,
           chain_data,
           (unsigned char*)in_key_handle,
           &in_sig_len,
           (unsigned char *)in_sig);

  if (8 <= *out_rc) {
    status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
  }

  return status;
}

static int rs_icsfp11_hmacSHA_internal(
                    const ICSFP11_HANDLE_T *in_key_handle,
                    ICSFP11_DIGEST_ALG in_alg,
                    const unsigned char* in_cleartext, int in_cleartext_len,
                    unsigned char* out_hmac, int *out_hmac_len,
                    int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 2;
  char rule_array[16] = {0};
  unsigned char* writehead = NULL;
  int alet = 0;
  unsigned char chain_data[128] = {0};
  int chain_data_len = sizeof(chain_data);
  int required_outsize = 0;

  do {
    if ((NULL == in_key_handle) ||
        (NULL == in_cleartext) ||
        (NULL == out_hmac) || (NULL == out_hmac_len) ||
        (NULL == out_rc) || (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    writehead = (unsigned char*) &(rule_array[0]);
    switch(in_alg) {
      case ICSFP11_SHA1:
        required_outsize = ICSFP11_SHA1_HASHLEN;
        memcpy(writehead, &(RS_SHA1_KEYWORD[0]), 8);
        writehead += 8;
        break;
      case ICSFP11_SHA256:
        required_outsize = ICSFP11_SHA256_HASHLEN;
        memcpy(writehead, &(RS_SHA256_KEYWORD[0]), 8);
        writehead += 8;
        break;
      case ICSFP11_SHA384:
        required_outsize = ICSFP11_SHA384_HASHLEN;
        memcpy(writehead, &(RS_SHA384_KEYWORD[0]), 8);
        writehead += 8;
        break;
      case ICSFP11_SHA512:
        required_outsize = ICSFP11_SHA512_HASHLEN;
        memcpy(writehead, &(RS_SHA512_KEYWORD[0]), 8);
        writehead += 8;
        break;
      default:
        status = RSCRYPTO_INVALID_ARGUMENT;
        break;
    }
    if (RS_SUCCESS != status)
      break;

    if (required_outsize > *out_hmac_len) {
      status = RSCRYPTO_INSUFFICIENT_OUTPUT_SPACE;
      break;
    }

    memcpy(writehead, &(RS_ONLY_KEYWORD[0]), 8);
    writehead += 8;

#ifdef DEBUG
    printf("rs_icsfp11_hmacSHA_internal: Before HMAC Generate rc=%d, reason=%d, out_hmac_len=%d\n",
           *out_rc, *out_reason, *out_hmac_len);
    printf("in_cleartext:\n");
    dumpbuffer(in_cleartext, in_cleartext_len);
#endif

#ifdef _LP64
    CSFPHMG6(
#else
    CSFPHMG(
#endif
        out_rc,
        out_reason,
        &exit_data_len,
        NULL, /* no exit data */
        &rule_array_count,
        (unsigned char*) rule_array,
        &in_cleartext_len,
        (unsigned char *)in_cleartext,
        &alet,
        &chain_data_len,
        chain_data,
        (unsigned char*)in_key_handle,
        out_hmac_len,
        out_hmac);

#ifdef DEBUG
    printf("rs_icsfp11_hmacSHA_internal: After HMAC Generate service: rc=%d, reason=0x%x, out_hmac_len=%d\n",
           *out_rc, *out_reason, *out_hmac_len);
    printf("out_hmac:\n");
    dumpbuffer(out_hmac, *out_hmac_len);
#endif

    if (8 <= *out_rc) {
      status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
      break;
    }

  } while(0);

  return status;
}

static int rs_icsfp11_findObjects_internal (
                    const ICSFP11_HANDLE_T *in_token_handle,
                    int in_numattrs, const CK_ATTRIBUTE *in_attrs,
                    ICSFP11_HANDLE_T **out_handles,
                    int *out_numfound,
                    int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 1;
  unsigned char* output_list = NULL;
  int output_list_length;
  int handle_count;

  unsigned char* attrlist = NULL;
  int attrlistlen = 0;

  ICSFP11_HANDLE_T inout_handle = ICSFP11_HANDLE_INITIALIZER;

  do {
    if ((NULL == in_token_handle) ||
        (NULL == in_attrs) ||
        (NULL == out_handles) ||
        (NULL == out_rc) ||
        (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    memcpy(&(inout_handle.tokenName[0]), &(in_token_handle->tokenName[0]), 32);

    status = serialize_attribute_list(in_numattrs, in_attrs,
                                      &attrlist, &attrlistlen);
    if (RS_SUCCESS != status)
      break;

#ifdef DEBUG
    printf("Serialized attribute list:\n");
    dumpbuffer(attrlist, attrlistlen);
#endif

#define ICSFP11_MAX_FOUND_OBJECTS   10
    output_list_length = ICSFP11_MAX_FOUND_OBJECTS * sizeof(ICSFP11_HANDLE_T);
    output_list = (unsigned char*)rscAlloc(output_list_length, "object handles output");
    handle_count = ICSFP11_MAX_FOUND_OBJECTS;

    /* use the Token Record List service to get the accessible token handles */
#ifdef _LP64
    CSFPTRL6 (
#else
    CSFPTRL (
#endif
        out_rc,
        out_reason,
        &exit_data_len,
        NULL, /* no exit data */
        (unsigned char*) &inout_handle,
        &rule_array_count,
        (unsigned char*)RS_OBJECT_KEYWORD,
        &attrlistlen, /* search template length */
        attrlist, /* search template */
        &output_list_length,
        &handle_count,
        output_list);

    if (8 <= *out_rc) {
      *out_handles = NULL;
      status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
      break;
    }

    if (0 < handle_count) {
      *out_handles = (ICSFP11_HANDLE_T*)rscAlloc(handle_count * sizeof(ICSFP11_HANDLE_T), "returned object handles");
      memcpy(*out_handles, output_list, handle_count * sizeof(ICSFP11_HANDLE_T));
    }
    *out_numfound = handle_count;

  } while(0);

  if (attrlist) {
    rscFree(attrlist, attrlistlen);
  }

  if (output_list) {
    rscFree(output_list, ICSFP11_MAX_FOUND_OBJECTS * sizeof(ICSFP11_HANDLE_T));
  }

  return status;
}

static int rs_icsfp11_createObject_internal(const ICSFP11_HANDLE_T *in_token_handle,
                                            int in_numattrs, const CK_ATTRIBUTE *in_attrs,
                                            ICSFP11_HANDLE_T **out_obj_handle,
                                            int *out_rc, int *out_reason)
{
  int status = RS_SUCCESS;

  int exit_data_len = 0;
  int rule_array_count = 1;

  unsigned char* attrlist = NULL;
  int attrlistlen = 0;

  ICSFP11_HANDLE_T inout_handle = ICSFP11_HANDLE_INITIALIZER;
  ICSFP11_HANDLE_T *key_handle = NULL;

  do {
    if ((NULL == in_token_handle) ||
        (NULL == in_attrs) ||
        (NULL == out_obj_handle) ||
        (NULL == out_rc) ||
        (NULL == out_reason))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    memcpy(&(inout_handle.tokenName[0]), &(in_token_handle->tokenName[0]), 32);

    status = serialize_attribute_list(in_numattrs, in_attrs,
                                      &attrlist, &attrlistlen);
    if (RS_SUCCESS != status)
      break;

#ifdef DEBUG
    printf("Serialized attribute list:\n");
    dumpbuffer(attrlist, attrlistlen);
#endif

#ifdef _LP64
    CSFPTRC6(
#else
    CSFPTRC(
#endif
        out_rc,
        out_reason,
        &exit_data_len,
        NULL, /* no exit data */
        (unsigned char*) &inout_handle,
        &rule_array_count,
        (unsigned char*) RS_OBJECT_KEYWORD,
        &attrlistlen,
        attrlist);

    if (8 <= *out_rc) {
      *out_obj_handle = NULL;
      status = RSCRYPTO_CALLABLE_SERVICE_ERROR;
      break;
    }

    key_handle = (ICSFP11_HANDLE_T *)rscAlloc(sizeof(ICSFP11_HANDLE_T), "key handle");
    memcpy(key_handle, &inout_handle, sizeof(ICSFP11_HANDLE_T));
    *out_obj_handle = key_handle;

  } while(0);

  if (attrlist) {
    rscFree(attrlist, attrlistlen);
  }

  return status;
}
/**
 * Allocate and return a serialized Attribute List based on the
 * supplied array of CK_ATTRIBUTEs.
 */
static int serialize_attribute_list(int num_attrs,
                                    const CK_ATTRIBUTE *attr_array,
                                    unsigned char **out_attrlist_buf,
                                    int *out_attrlist_len)
{
  int status = RS_SUCCESS;

  unsigned char *listbuf = NULL;
  int listlen = 0;

  unsigned char* writehead = NULL;
  unsigned short tmpshort = 0;
  unsigned int tmpint = 0;

  int i;

  do {
    /* try to support zero-attribute Attribute List */
    if ((NULL == out_attrlist_buf) ||
        (NULL == out_attrlist_len))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    if ((0 < num_attrs) && (NULL == attr_array))
    {
      status = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }

    listlen += 2; /* number of attributes in list */
    /* calculate the attrs required length */
    for (i=0; i < num_attrs; i++) {
      listlen += (4 + 2 + attr_array[i].ulValueLen);
    }

    listbuf = (unsigned char*) rscAlloc(listlen, "attribute list");
    if (NULL == listbuf) {
      status = RSCRYPTO_ALLOC_FAILED;
      break;
    }
    writehead = listbuf;

    tmpshort = (unsigned short) num_attrs;
    memcpy(writehead, &tmpshort, sizeof(unsigned short));
    writehead += sizeof(unsigned short);

    /* each attribute is structure consisting of:
     * attr_name (4 bytes), value_len (2 bytes), value (len bytes) */
    for (i=0; i < num_attrs; i++)
    {
      tmpint = (unsigned int) attr_array[i].type;
      memcpy(writehead, &tmpint, sizeof(unsigned int));
      writehead += sizeof(unsigned int);

      tmpshort = (unsigned short) attr_array[i].ulValueLen;
      memcpy(writehead, &tmpshort, sizeof(unsigned short));
      writehead += sizeof(unsigned short);

      memcpy(writehead, attr_array[i].pValue, attr_array[i].ulValueLen);
      writehead += attr_array[i].ulValueLen;

      tmpint = 0;
      tmpshort = 0;
    }

    *out_attrlist_buf = listbuf;
    *out_attrlist_len = listlen;

  } while(0);

  return status;
}

/* allocates new CK_ATTRIBUTE structures, but those structures point into the supplied serialized_attrlist buffer! */
static int parse_attribute_list(const unsigned char* in_serialized_attrlist,
                                int in_serialized_attrlist_len,
                                CK_ATTRIBUTE** out_attrs, int* out_num_attrs)
{
  int sts = 0;
  const unsigned char* readhead = in_serialized_attrlist;
  int bytes_remaining = in_serialized_attrlist_len;

  CK_ATTRIBUTE* attrs = NULL;
  unsigned short num_attrs = 0;

  do {
    if ((NULL == in_serialized_attrlist) || (0 == in_serialized_attrlist_len) ||
        (NULL == out_attrs) || (NULL == out_num_attrs))
    {
      sts = RSCRYPTO_INVALID_ARGUMENT;
      break;
    }
#ifdef DEBUG
    printf("starting read at address 0x%p, remaining: %d\n", readhead, bytes_remaining);
#endif
    if (sizeof(unsigned short) <= bytes_remaining) {
#ifdef DEBUG
      printf("reading %d bytes from address 0x%p, remaining: %d\n", sizeof(unsigned short), readhead, bytes_remaining);
#endif
      num_attrs = *((unsigned short *)readhead);
      readhead += sizeof(unsigned short);
      bytes_remaining -= sizeof(unsigned short);
    } else {
      sts = RSCRYPTO_ATTRLIST_PARSE_ERROR;
      break;
    }

    if (0 < num_attrs) {
      attrs = (CK_ATTRIBUTE*) rscAlloc(num_attrs * sizeof(CK_ATTRIBUTE), "attrs");
      for (int i=0;
           (i < num_attrs) && (0 < bytes_remaining);
           i++)
      {
        uint32 tmpType = 0;
        unsigned short tmpAttrLen = 0;

        if (sizeof(uint32) < bytes_remaining) {
#ifdef DEBUG
          printf("reading %d bytes from address 0x%p, remaining: %d\n", sizeof(uint32), readhead, bytes_remaining);
#endif
          tmpType = *((uint32 *)readhead);
          readhead += sizeof(uint32);
          bytes_remaining -= sizeof(uint32);
        } else {
          sts = RSCRYPTO_ATTRLIST_PARSE_ERROR;
          break;
        }

        if (sizeof(unsigned short) <= bytes_remaining) {
#ifdef DEBUG
          printf("reading %d bytes from address 0x%p, remaining: %d\n", sizeof(unsigned short), readhead, bytes_remaining);
#endif
          tmpAttrLen = *((unsigned short *)readhead);
          readhead += sizeof(unsigned short);
          bytes_remaining -= sizeof(unsigned short);
        } else {
          sts = RSCRYPTO_ATTRLIST_PARSE_ERROR;
          break;
        }

        if (tmpAttrLen <= bytes_remaining) {
#ifdef DEBUG
          printf("setting attr[%d] with type 0x%x and len %d\n", i, tmpType, (int)tmpAttrLen);
#endif
          attrs[i].type = tmpType;
          attrs[i].pValue = (CK_VOID_PTR) readhead;
          attrs[i].ulValueLen = (CK_ULONG) tmpAttrLen;
#ifdef DEBUG
          printf("seeking ahead %d bytes from address 0x%p, remaining: %d\n", tmpAttrLen, readhead, bytes_remaining);
#endif
          readhead += tmpAttrLen;
          bytes_remaining -= tmpAttrLen;
        } else {
          sts = RSCRYPTO_ATTRLIST_PARSE_ERROR;
          break;
        }
      }
    }
#ifdef DEBUG
    printf("after parsing serialized attribute list, bytes_remaining = %d\n", bytes_remaining);
    printf("num_attrs: %d\n", num_attrs);
    printf("attrs contents follow:\n");
    dumpbuffer((char*)attrs, num_attrs * sizeof(CK_ATTRIBUTE));
#endif

    if (0 != sts) {
      if (attrs) {
        memset(attrs, 0x00, num_attrs * sizeof(CK_ATTRIBUTE));
        rscFree(attrs, num_attrs * sizeof(CK_ATTRIBUTE));
      }
      break;
    }

    *out_attrs = attrs;
    *out_num_attrs = (int) num_attrs;

  } while(0);

  return sts;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
