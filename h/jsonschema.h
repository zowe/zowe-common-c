/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_JSONSCHEMA__
#define __ZOWE_JSONSCHEMA__ 1

#ifdef METTLE

#else
#include <setjmp.h>
#endif

#ifdef __ZOWE_OS_WINDOWS
#include "winregex.h"
#else
#include "psxregex.h"
#endif

#include "json.h"

typedef struct JsonSchema_tag {
  ShortLivedHeap *slh;
  int             version;
  struct JSValueSpec_tag *topValueSpec;
} JsonSchema;

#define MAX_SCHEMA_DEPTH 100

typedef struct AccessPath_tag {
  struct {
    union {
      char *name;
      int   index;
    } key;
    bool isName;
  } elements[MAX_SCHEMA_DEPTH];
  int currentSize;
} AccessPath;

typedef struct JsonSchemaBuilder_tag {
  JsonSchema *schema;
  ShortLivedHeap *slh;
  int         version;
  int         traceLevel;
  AccessPath *accessPath;
  int         errorCode;
  char       *errorMessage;
  int         errorMessageLength;
#ifdef METTLE
#error "need setjmp"
#else
  jmp_buf     recoveryData;
#endif
} JsonSchemaBuilder;


/* The return types of these validators is bool to allow validator to signal whether to continue 
   to gather more validation exceptions in this part of the JSON tree/graph.  Returning false says
   this part is invalid enough such that further evaluation would probably confuse the user with 
   contradictory information. */
typedef enum VStatus_tag {
  InvalidStop = 0,
  ValidStop = 1,
  InvalidContinue = 2,
  ValidContinue = 3
} VStatus;

typedef struct VResult_tag {
  VStatus status;
  struct ValidityException_tag *exception;
} VResult;


#define MAX_ACCESS_PATH 1024

#define MAX_VALIDITY_EXCEPTION_MSG 1024

typedef struct ValidityException_tag {
  int code;
  VResult result;
  char *message;
  struct ValidityException_tag *firstChild;
  struct ValidityException_tag *nextSibling;
} ValidityException;

#define VALIDATOR_WARN_ON_UNDEFINED_PROPERTIES 0x0001

#define MAX_VALIDATOR_MATCHES 8

typedef struct JsonValidator_tag {
  JsonSchema  *topSchema;
  /* Other schemas that are referred to by the top and each other can be loaded into
     one validator */
  JsonSchema **otherSchemas;
  int          otherSchemaCount;
  int          errorCode;
  char        *errorMessage;
  int          errorMessageLength;
  char         accessPathBuffer[MAX_ACCESS_PATH];
  ValidityException *topValidityException;
  int         flags;
  int         traceLevel;
  FILE       *traceOut;
  AccessPath *accessPath;
  /* we use these pattern many times and manage the resources from here */
  regmatch_t  matches[MAX_VALIDATOR_MATCHES];
  regex_t    *httpRegex;
  int         httpRegexError;
  regex_t    *fileRegex;
  int         fileRegexError;
  jmp_buf     recoveryData;
  ShortLivedHeap *evalHeap;
} JsonValidator;

#define JSON_SCHEMA_DRAFT_4 400
#define JSON_SCHEMA_DRAFT_7 700
#define JSON_SCHEMA_DRAFT_2019 201909
#define DEFAULT_JSON_SCHEMA_VERSION JSON_SCHEMA_DRAFT_2019


JsonSchemaBuilder *makeJsonSchemaBuilder(int version);
void freeJsonSchemaBuilder(JsonSchemaBuilder *builder);
JsonSchema *jsonBuildSchema(JsonSchemaBuilder *builder, Json *schemaJson);

JsonValidator *makeJsonValidator();
void freeJsonValidator(JsonValidator *validator);

#define JSON_VALIDATOR_NO_EXCEPTIONS 0
#define JSON_VALIDATOR_HAS_EXCEPTIONS 4
#define JSON_VALIDATOR_INTERNAL_FAILURE 8

int jsonValidateSchema(JsonValidator *validator, Json *value, JsonSchema *topSchema,
                       JsonSchema **otherSchemas, int otherSchemaCount);


#endif 

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
