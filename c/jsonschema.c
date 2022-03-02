

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
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> 
#include <stdint.h> 
#include <stdbool.h> 
#include <errno.h>
#include <setjmp.h>

#endif


#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "json.h"
#include "charsets.h"
#include "collections.h"

#ifdef __ZOWE_OS_WINDOWS
#include "winregex.h"
#else
#include "psxregex.h"  /* works on ZOS and Linux */
#endif

#include "jsonschema.h"

#ifdef __ZOWE_OS_WINDOWS
typedef int64_t ssize_t;
#endif

#define JSTYPE_NULL 0
#define JSTYPE_BOOLEAN 1
#define JSTYPE_OBJECT 2
#define JSTYPE_ARRAY 3
#define JSTYPE_STRING 4
#define JSTYPE_NUMBER 5
#define JSTYPE_INTEGER 6
#define JSTYPE_MAX 6

#define JS_VALIDATOR_MULTOF       0x00000001
#define JS_VALIDATOR_MAX          0x00000002
#define JS_VALIDATOR_MIN          0x00000004
#define JS_VALIDATOR_MAX_CONTAINS 0x00000008
#define JS_VALIDATOR_MIN_CONTAINS 0x00000010
#define JS_VALIDATOR_MAX_ITEMS    0x00000020
#define JS_VALIDATOR_MIN_ITEMS    0x00000040
#define JS_VALIDATOR_MAX_PROPS    0x00000080
#define JS_VALIDATOR_MIN_PROPS    0x00000100
#define JS_VALIDATOR_MAX_LENGTH   0x00000200
#define JS_VALIDATOR_MIN_LENGTH   0x00000400

typedef struct JSValueSpec_tag {
  int      typeMask;
  Json   **enumeratedValues;
  Json    *constValue;
  char    *id;
  char    *description;
  char    *title;
  char    *ref;
  bool     deprecated;
  bool     nullable;
  /* shared sub-schema definitions go here */
  hashtable *definitions;
  /* Compositing */
  struct JSValueSpec_tag **allOf;
  struct JSValueSpec_tag **anyOf;
  struct JSValueSpec_tag **oneOf;
  struct JSValueSpec_tag *not;

  /* optional validators have flag for presence */
  uint64_t validatorFlags;

  double minimum;
  double maximum;
  int64_t minContains;
  int64_t maxContains;
  int64_t minItems;
  int64_t maxItems;
  int64_t minProperties;
  int64_t maxProperties;
  int      requiredCount;
  char   **required;
  hashtable *properties;
  hashtable *dependentRequired;

  /* for numbers */
  bool exclusiveMaximum;
  bool exclusiveMinimum;
  double multipleOf;
  bool isInteger;

  /* for arrays */
  struct JSValueSpec_tag *itemSpec;
  bool uniqueItems;

  /* for strings */
  int64_t minLength;
  int64_t maxLength;
  char *pattern;
  regex_t *compiledPattern;
  int      regexCompilationError;
} JSValueSpec;

typedef struct AccessPathUnion_tag {
  int dummy;
} AccessPathUnion;



static void printAccessPath(FILE *out, AccessPath *path){
  for (int i=0; i<path->currentSize; i++){
    if (path->elements[i].isName){
      fprintf(out,"/%s",path->elements[i].key.name);
    } else {
      fprintf(out,"/%d",path->elements[i].key.index);
    }
  }
  fprintf(out,"\n");
}

static char *makeAccessPathString(char *buffer, int bufferLen, AccessPath *path){
  int pos = 0;
  for (int i=0; i<path->currentSize; i++){
    if (pos + 100 >= bufferLen){
      sprintf(buffer+pos,"...");
      break;
    }
    if (path->elements[i].isName){
      pos += sprintf(buffer+pos,"/%s",path->elements[i].key.name);
    } else {
      pos += sprintf(buffer+pos,"/%d",path->elements[i].key.index);
    }
  }
  return buffer;
}


static void accessPathPushIndex(AccessPath *accessPath, int index){
  if (accessPath->currentSize < MAX_SCHEMA_DEPTH){
    accessPath->elements[accessPath->currentSize].isName = false;
    accessPath->elements[accessPath->currentSize++].key.index = index;
  }
}

static void accessPathPushName(AccessPath *accessPath, char *name){
  if (accessPath->currentSize < MAX_SCHEMA_DEPTH){
    accessPath->elements[accessPath->currentSize].isName = true;
    accessPath->elements[accessPath->currentSize++].key.name = name;
  }
}

static void accessPathPop(AccessPath *accessPath){
  if (accessPath->currentSize > 0){
    accessPath->currentSize--;
  }
}



#define ERROR_MAX 1024

static void validationThrow(JsonValidator *validator, int errorCode, char *formatString, ...){
  va_list argPointer;
  char *text = safeMalloc(ERROR_MAX,"ErrorBuffer");
  va_start(argPointer,formatString);
  vsnprintf(text,ERROR_MAX,formatString,argPointer);
  va_end(argPointer);
  validator->errorCode = errorCode;
  validator->errorMessage = text;
  validator->errorMessageLength = ERROR_MAX;
  longjmp(validator->recoveryData,1);
}

static ValidityException *noteValidityException(JsonValidator *validator, int invalidityCode, char *formatString, ...){
  va_list argPointer;
  ValidityException *exception = (ValidityException*)safeMalloc(sizeof(ValidityException),"ValidityException");
  exception->code = invalidityCode;
  exception->next = NULL;
  va_start(argPointer,formatString);
  vsnprintf(exception->message,MAX_VALIDITY_EXCEPTION_MSG,formatString,argPointer);
  va_end(argPointer);
  if (validator->firstValidityException == NULL){
    validator->firstValidityException = exception;
    validator->lastValidityException = exception;
  } else {
    validator->lastValidityException->next = exception;
    validator->lastValidityException = exception;
  }
  return exception;
}

JsonValidator *makeJsonValidator(){
  JsonValidator *validator = (JsonValidator*)safeMalloc(sizeof(JsonValidator),"JsonValidator");
  memset(validator,0,sizeof(JsonValidator));
  validator->accessPath = (AccessPath*)safeMalloc(sizeof(AccessPath),"JsonValidatorAccessPath");
  validator->accessPath->currentSize = 0;
  return validator;
}

void freeJsonValidator(JsonValidator *validator){
  safeFree((char*)validator->accessPath,sizeof(AccessPath));
  safeFree((char*)validator,sizeof(JsonValidator));
}

static char *validatorAccessPath(JsonValidator *validator){
  return makeAccessPathString(validator->accessPathBuffer,MAX_ACCESS_PATH,validator->accessPath);
}

/* The return types of these validators is bool to allow validator to signal whether to continue 
   to gather more validation exceptions in this part of the JSON tree/graph.  Returning false says
   this part is invalid enough such that further evaluation would probably confuse the user with 
   contradictory information. */

static bool validateType(JsonValidator *validator,
                         int typeCode,
                         JSValueSpec *valueSpec){
  if (((1 << typeCode) & valueSpec->typeMask) == 0){
    noteValidityException(validator,12,"type %d not permitted at %s",typeCode,validatorAccessPath(validator));
    return false;
  } else {
    return true;
  }
}

/* for mutual recursion */
static bool validateJSON(JsonValidator *validator, Json *value, JSValueSpec *valueSpec);

static bool validateJSONObject(JsonValidator *validator,
                               JsonObject *object,
                               JSValueSpec *valueSpec){
  AccessPath *accessPath = validator->accessPath;
  printf("validateJSONObject required=0x%p\n",valueSpec->required);
  printAccessPath(stdout,accessPath);
  fflush(stdout);
  if (valueSpec->required){
    for (int r=0; r<valueSpec->requiredCount; r++){
      char *requiredProperty = valueSpec->required[r];
      if (jsonObjectGetPropertyValue(object,requiredProperty) == NULL){
        noteValidityException(validator,12,"missing required property '%s' at '%s'",
                              requiredProperty,validatorAccessPath(validator));
      }
    }
  }
  // dependent required should go here
  int propertyCount = 0;
  JsonProperty *property = jsonObjectGetFirstProperty(object);
  while (property){
    propertyCount++;
    char *propertyName = jsonPropertyGetKey(property);
    printf("validate object pname=%s\n",propertyName);
    fflush(stdout);
    Json *propertyValue = jsonPropertyGetValue(property);
    JSValueSpec *propertySpec = (valueSpec->properties ?
                                 (JSValueSpec*)htGet(valueSpec->properties,propertyName) :
                                 NULL);
    accessPathPushName(accessPath,propertyName);
        
    if (propertySpec != NULL){
      validateJSON(validator,propertyValue,propertySpec);
    } else {
      if (validator->flags && VALIDATOR_WARN_ON_UNDEFINED_PROPERTIES){
        printf("*WARNING* unspecified property seen, '%s', and checking code is not complete, vspec->props=0x%p\n",
               propertyName,valueSpec->properties);
        fflush(stdout);
      }
      if (valueSpec->properties){ 
        /* htDump(valueSpec->properties); */
      }
    }
    accessPathPop(accessPath);
    property = jsonObjectGetNextProperty(property);
  }
  
  // Interdependencies
  if (valueSpec->dependentRequired != NULL){
    printf("*WARNING* depenentRequired not yet implemented\n");
  }

  
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_PROPS){
    int64_t lim = valueSpec->minProperties;
    if (propertyCount > lim){
      noteValidityException(validator,12,"too many properties, %d > MAX=%d at %s",
                      propertyCount,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_PROPS){
    int64_t lim = valueSpec->minProperties;
    if (propertyCount < lim){
      noteValidityException(validator,12,"too few properties, %d < MIN=%d at %s",
                            propertyCount,lim,validatorAccessPath(validator));
    }
  }
  return true;
}

bool validateJSONArray(JsonValidator *validator,
                       JsonArray *array,
                       JSValueSpec *valueSpec){
  AccessPath *accessPath = validator->accessPath;
  //boolean uniqueItems;
  // Long maxContains;
  // Long minContains;
  int elementCount = jsonArrayGetCount(array);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_ITEMS){
    int64_t lim = valueSpec->maxItems;
    if (elementCount > lim){
      noteValidityException(validator,12,
                            "too many array items, %d > MAX=%d at %s",
                            elementCount,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_ITEMS){
    int64_t lim = valueSpec->minItems;
    if (elementCount < lim){
      noteValidityException(validator,12,
                            "too few array ites, %d < MIN=%d at %s",
                            elementCount,lim,validatorAccessPath(validator));
    }
  }
  /* 'uniqueItems' is poorly specified regarding equality predicates.
     for now it will use native equals() from JSValue.
     It's kinda-crazy to do a true equal on printed representation on things that are
     unlimited in size and complexity.
  */
  LongHashtable *uniquenessSet = NULL;
  if (valueSpec->uniqueItems){ 
    uniquenessSet = lhtCreate(257,NULL);
  }
  
  for (int i=0; i<elementCount; i++){
    Json *itemValue = jsonArrayGetItem(array,i);
    accessPathPushIndex(accessPath,i);
    if (valueSpec->itemSpec != NULL){
      validateJSON(validator,itemValue,valueSpec->itemSpec);
    }
    if (valueSpec->uniqueItems){
      long longHash = jsonLongHash(itemValue);
      if (lhtGet(uniquenessSet,longHash) != NULL){
        noteValidityException(validator,12,"array uniqueItems violation %s is duplicate at %s",
                              itemValue,i,validatorAccessPath(validator));
      }
    }
    accessPathPop(accessPath);
  }
  return true;
}

static bool validateJSONString(JsonValidator *validator,
                               Json *string,
                               JSValueSpec *valueSpec){
  char *s = jsonAsString(string);
  int len = strlen(s);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_LENGTH){
    long lim = valueSpec->maxLength;
    if (len > lim){
      noteValidityException(validator,12,"string too long, %d > MAX=%d at %s",
                            len,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_LENGTH){
    long lim = valueSpec->minLength;
    if (len < lim){
      noteValidityException(validator,12,"string too short, %d < MAX=%d at %s",
                            len,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->pattern && (valueSpec->regexCompilationError == 0)){
    if (valueSpec->compiledPattern == NULL){
      regex_t *rx = regexAlloc();
      int compStatus = regexComp(rx,valueSpec->pattern,REG_EXTENDED);
      if (compStatus){
        regexFree(rx);
        valueSpec->regexCompilationError = compStatus;
        printf("*** WARNING *** pattern '%s' failed to compile with status %d\n",
               valueSpec->pattern,compStatus);
      } else {
        valueSpec->compiledPattern = rx;
      }
    }
    if (valueSpec->compiledPattern){
      int regexStatus = regexExec(valueSpec->compiledPattern,s,0,NULL,0);
      if (regexStatus != 0){
        noteValidityException(validator,12,"string pattern match fail s='%s', pat='%s', at %s",
                              s,valueSpec->pattern,validatorAccessPath(validator));
      }
    }
  }
  // String pattern; // ECMA-262 regex
  // prefix items
  // additionalProperties
  return true;
}

static bool validateJSONNumber(JsonValidator *validator,
                               Json *doubleOrLegacyInt,
                               JSValueSpec *valueSpec){
  // Number multipleOf; // can be a filled, and must be > 0
  // Number exclusiveMaximum;
  // Number exclusiveMinimum;
  double d = (doubleOrLegacyInt->type == JSON_TYPE_NUMBER ?
              (double)jsonAsNumber(doubleOrLegacyInt) :
              jsonAsDouble(doubleOrLegacyInt));
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX){
    double lim = valueSpec->maximum;
    if (valueSpec->exclusiveMaximum ? (d >= lim) : (d > lim)){
      noteValidityException(validator,12,"value too large, %f %s MAX=%f at %s",
                            d,
                            (valueSpec->exclusiveMaximum ? ">=" : ">"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN){
    double lim = valueSpec->minimum;
    if (valueSpec->exclusiveMinimum ? (d <= lim) : (d < lim)){
      noteValidityException(validator,12,"value too small, %f %s MAX=%f at %s",
                            d,
                            (valueSpec->exclusiveMinimum ? "<=" : "<"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  return true;
}

static bool validateJSONInteger(JsonValidator *validator,
                                Json *value64,
                                JSValueSpec *valueSpec){
  // Number multipleOf; // can be a filled, and must be > 0
  // Number exclusiveMaximum;
  // Number exclusiveMinimum;
  int64_t i = jsonAsInt64(value64);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX){
    int64_t lim = (int64_t)valueSpec->maximum;
    if (valueSpec->exclusiveMaximum ? (i >= lim) : (i > lim)){
      noteValidityException(validator,12,"value too large, %lld %s MAX=%lld at %s",
                            i,
                            (valueSpec->exclusiveMaximum ? ">=" : ">"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN){
    int64_t lim = (int64_t)valueSpec->minimum;
    if (valueSpec->exclusiveMinimum ? (i <= lim) : (i < lim)){
      noteValidityException(validator,12,"value too small, %lld %s MAX=%lld at %s",
                            i,
                            (valueSpec->exclusiveMinimum ? "<=" : "<"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  return true;
}

static JSValueSpec *resolveRef(JsonValidator *validator, JSValueSpec *valueSpec){
  char *ref = valueSpec->ref;
  printf("resolveRef ref=%s\n",valueSpec->ref);
  if (!memcmp(ref,"#/$defs/",8)){   /* local resolution */
    int refLen = strlen(ref);
    int pos = 2;
    char *key = ref+8;
    JSValueSpec *topSchema = validator->schema->topValueSpec;
    if (topSchema->definitions == NULL){
      noteValidityException(validator,12,"schema '%s' does not define shared '$defs'",
                            (topSchema->id ? topSchema->id : "<anonymous>"));
      return NULL; /* for the compiler, bless its heart */
    }
    JSValueSpec *resolvedSchema = htGet(topSchema->definitions,key);
    if (resolvedSchema == NULL){
      noteValidityException(validator,12,"schema ref '%s' does not resolve against '$defs'",ref);
      return NULL;
    }
    return resolvedSchema;
  } else {
    /* notes, 
       need to merge URL according W3 rules and pass to pluggable resolver that
       we hope this validator has.
     */
    noteValidityException(validator,12,"external refs not yet implemented '%s' at %s",
                          valueSpec->ref,validatorAccessPath(validator));
    return NULL;
  }
}

static bool validateJSON(JsonValidator *validator, Json *value, JSValueSpec *valueSpec){
  while (valueSpec->ref){
    valueSpec = resolveRef(validator,valueSpec);
    if (valueSpec == NULL){
      return false;
    }
  }
  printf("validate JSON value->type=%d\n",value->type);
  fflush(stdout);
  // type is string or array, does this mean to collapse ValueSpecTypes
  // should we flag errors on out-of-bounds validation keywords for type keyword
  if (jsonIsNull(value)){
    return validateType(validator,JSTYPE_NULL,valueSpec);
  } else if (jsonIsBoolean(value)){
    return validateType(validator,JSTYPE_BOOLEAN,valueSpec);
  } else if (jsonIsObject(value)){
    return (validateType(validator,JSTYPE_OBJECT,valueSpec) &&
            validateJSONObject(validator,jsonAsObject(value),valueSpec));
  } else if (jsonIsArray(value)){
    return (validateType(validator,JSTYPE_ARRAY,valueSpec) &&
            validateJSONArray(validator,jsonAsArray(value),valueSpec));
  } else if (jsonIsString(value)){
    return (validateType(validator,JSTYPE_STRING,valueSpec) &&
            validateJSONString(validator,value,valueSpec));
  } else if (jsonIsNumber(value)){
    if (!jsonIsInt64(value)){
      return (validateType(validator,JSTYPE_NUMBER,valueSpec) &&
              validateJSONNumber(validator,value,valueSpec)); /* general, comparisons done as doubles */
    } else {
      return (validateType(validator,JSTYPE_INTEGER,valueSpec) &&
              validateJSONInteger(validator,value,valueSpec));
    }
    
  } else {
    printf("*** PANIC *** unhandled JS Value with type = %d\n",value->type);
    return false;
  }
}

int jsonValidateSchema(JsonValidator *validator, Json *value, JsonSchema *schema){
  if (setjmp(validator->recoveryData) == 0) {  /* normal execution */
    validator->schema = schema;
    validateJSON(validator,value,schema->topValueSpec);
    printf("after validate without throw, should show validation exceptions\n");
    fflush(stdout);
    if (validator->firstValidityException == NULL){
      return JSON_VALIDATOR_NO_EXCEPTIONS;
    } else {
      return JSON_VALIDATOR_HAS_EXCEPTIONS;
    }
  } else {
    printf("validation failed '%s'\n",validator->errorMessage);
    fflush(stdout);
    return JSON_VALIDATOR_INTERNAL_FAILURE;
  }
}


static void schemaThrow(JsonSchemaBuilder *builder, int errorCode, char *formatString, ...){
  va_list argPointer;
  char *text = safeMalloc(ERROR_MAX,"ErrorBuffer");
  va_start(argPointer,formatString);
  vsnprintf(text,ERROR_MAX,formatString,argPointer);
  va_end(argPointer);

  builder->errorCode = errorCode;
  builder->errorMessage = text;
  builder->errorMessageLength = ERROR_MAX;

  longjmp(builder->recoveryData,1);
}

static char *getStringOrFail(JsonSchemaBuilder *builder, JsonObject *object, char *propertyName) {
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    schemaThrow(builder,12,"object must have property: %s",propertyName);
    return NULL; /* unreachable, but compiler doesn't know */
  } else if (jsonIsString(propertyValue)){
    return jsonAsString(propertyValue);
  } else {
    schemaThrow(builder,12,"property '%s' must contain a string, not %s",propertyName,propertyValue);
    return NULL; /* unreachable, but compiler doesn't know */
  }
}

static char *getString(JsonSchemaBuilder *builder, JsonObject *object, char *propertyName, char *defaultValue) {
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    return defaultValue;
  } else if (jsonIsString(propertyValue)){
    return jsonAsString(propertyValue);
  } else {
    schemaThrow(builder,12,"property '%s' must contain a string, not %s",propertyName,propertyValue);
    return NULL; /* unreachable, but compiler doesn't know */
  }
}

static bool getBooleanValue(JsonSchemaBuilder *builder, JsonObject *object, char *propertyName, bool defaultValue){
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    return defaultValue;
  } else if (jsonIsBoolean(propertyValue)){
    return (bool)jsonAsBoolean(propertyValue);
  } else {
    schemaThrow(builder,12,"property '%s' must contain a number, not %s",propertyName,propertyValue);
    return false; /* unreachable, but compiler doesn't know */
  }
}

static double getNumber(JsonSchemaBuilder *builder, JsonObject *object, char *propertyName, double defaultValue){
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    return defaultValue;
  } else if (jsonIsNumber(propertyValue)){
    return jsonAsDouble(propertyValue);
  } else {
    schemaThrow(builder,12,"property '%s' must contain a number, not %s",propertyName,propertyValue);
    return 0.0; /* unreachable, but compiler doesn't know */
  }
}

static int64_t getIntegralValue(JsonSchemaBuilder *builder, JsonObject *object, char *propertyName, int64_t defaultValue){
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    return defaultValue;
  } else if (jsonIsNumber(propertyValue)){
    if (jsonIsInt64(propertyValue)){
      return jsonAsInt64(propertyValue);
    } else {
      schemaThrow(builder,12,"property '%s' must contain an integer, not %s",propertyName,propertyValue);
      return 0; /* unreachable, but compiler doesn't know */
    }
  } else {
    schemaThrow(builder,12,"property '%s' must contain a number, not %s",propertyName,propertyValue);
    return 0; /* unreachable, but compiler doesn't know */
  }
}

static JsonArray *getArrayValue(JsonSchemaBuilder *builder, JsonObject *object, char *propertyName) {
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    return NULL;
  } else if (jsonIsArray(propertyValue)){
    return jsonAsArray(propertyValue);
  } else {
    schemaThrow(builder,12,"property '%s' must contain an array, not %s",propertyName,propertyValue);
    return NULL; /* unreachable, but compiler doesn't know */
  }
}

static JsonObject *getObjectValue(JsonSchemaBuilder *builder, JsonObject *object, char *propertyName) {
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    return NULL;
  } else if (jsonIsObject(propertyValue)){
    return jsonAsObject(propertyValue);
  } else {
    schemaThrow(builder,12,"property '%s' must contain an object, not %s",propertyName,propertyValue);
    return NULL; /* unreachable, but compiler doesn't know */
  }
}

static int getJSTypeForName(JsonSchemaBuilder *builder, char *name){
  if (!strcmp(name,"null")){
    return JSTYPE_NULL;
  } else if (!strcmp(name,"boolean")){
    return JSTYPE_BOOLEAN;
  } else if (!strcmp(name,"object")){
    return JSTYPE_OBJECT;
  } else if (!strcmp(name,"array")){
    return JSTYPE_ARRAY;
  } else if (!strcmp(name,"number")){
    return JSTYPE_NUMBER;
  } else if (!strcmp(name,"string")){
    return JSTYPE_STRING;
  } else if (!strcmp(name,"integer")){
    return JSTYPE_INTEGER;
  } else {
    schemaThrow(builder,12,"'%s' is not valid JSON Schema type",name);
    return 0;
  }
}

static void checkNonNegative(JsonSchemaBuilder *builder, int64 value, char *name) {
  if (value < 0){
    schemaThrow(builder,12,"%s must be non-negative",name);
  }
}

static void checkPositiveDouble(JsonSchemaBuilder *builder, double value, char *name) {
  if (value <= 0){
    schemaThrow(builder,12,"%s must be positive",name);
  }
}

static char** builderStringArray(JsonSchemaBuilder *builder, int count){
  char **result = (char**)SLHAlloc(builder->slh,count*sizeof(char*));
  return result;
}

static char **getStringArrayOrFail(JsonSchemaBuilder *builder, JsonArray *a, int *count){
  ShortLivedHeap *slh = builder->slh;
  int aLen = jsonArrayGetCount(a);
  char **result = builderStringArray(builder,aLen);
  for (int i=0; i<aLen; i++){
    Json *element = jsonArrayGetItem(a,i);
    if (jsonIsString(element)){
      result[i] = jsonAsString(element);
    } else {
      schemaThrow(builder,12,"required string array, but saw element=%s",element);
    }
  }
  *count = aLen;
  return result;
}

static hashtable *getStringToStringsMapOrFail(JsonSchemaBuilder *builder, JsonObject *o) {
  hashtable *map = htCreate(31,stringHash,stringCompare,NULL,NULL);
  JsonProperty *property = jsonObjectGetFirstProperty(o);
  while (property){
    char *propertyName = jsonPropertyGetKey(property);
    Json *propertyValue = jsonPropertyGetValue(property);
    if (jsonIsArray(propertyValue)){
      int count; /* hmm */
      htPut(map,propertyName,getStringArrayOrFail(builder,jsonAsArray(propertyValue),&count));
    } else {
      schemaThrow(builder,12,"required string array, but saw 0x%p, with type=%d",propertyValue,propertyValue->type);
      return NULL;
    }
    property = jsonObjectGetNextProperty(property);
  }
  return map;
}

static bool ensureUniqueKeys(JsonSchemaBuilder *builder, JsonObject *o){
  /*
    The Java JSON Reader we use guarantees unique property keys, but
  */
  return true;
}

// Tolerate both "id" and "$id"
static char *getID(JsonSchemaBuilder *builder, JsonObject *o) {
  char *id = getString(builder,o,"$id",NULL);
  if (id == NULL){
    id = getString(builder,o,"id",NULL);
  }
  return id;
}

/* forwarding for complex recursion */
static JSValueSpec *build(JsonSchemaBuilder *builder, Json *jsValue, bool isTopLevel);

static JSValueSpec *makeValueSpec(JsonSchemaBuilder *builder,
                                  char *id,
                                  char *description,
                                  char *title,
                                  char **typeNameArray,
                                  int  typeNameArrayLength){
  JSValueSpec *spec = (JSValueSpec*)SLHAlloc(builder->slh,sizeof(JSValueSpec));
  memset(spec,0,sizeof(JSValueSpec));
  spec->id = id;
  spec->description = description;
  spec->title = title;
  for (int i=0; i<typeNameArrayLength; i++){
    int typeCode = getJSTypeForName(builder,typeNameArray[i]);
    spec->typeMask |= (1 << typeCode);    
  }
  return spec;
}
                                  

/* Map<String,JSValueSpec>  */
static hashtable *getDefinitions(JsonSchemaBuilder *builder, JsonObject *object){
  AccessPath *accessPath = builder->accessPath;
  JsonObject *definitionsObject = getObjectValue(builder,object,"$defs");
  if (definitionsObject != NULL){
    hashtable *definitionMap = htCreate(101,stringHash,stringCompare,NULL,NULL);
    accessPathPushName(accessPath,"definitions");
    JsonProperty *property = jsonObjectGetFirstProperty(definitionsObject);
    while (property){
      char *propertyName = jsonPropertyGetKey(property);
      Json *propertyValue = jsonPropertyGetValue(property);
      if (propertyValue == NULL){
        schemaThrow(builder,12,"sub definition %s is NULL, which is not allowed",propertyName);
        return NULL;
      }
      accessPathPushName(accessPath,propertyName);
      htPut(definitionMap,propertyName,build(builder,propertyValue,false));
      accessPathPop(accessPath);
      property = jsonObjectGetNextProperty(property);
    }
    accessPathPop(accessPath);
    return definitionMap;
  } else {
    return NULL;
  } 
}

static JSValueSpec **getComposite(JsonSchemaBuilder *builder, JsonObject *object, char *key){
  AccessPath *accessPath = builder->accessPath;
  JsonArray *array = jsonObjectGetArray(object,key);
  if (array != NULL){
    int len = jsonArrayGetCount(array);
    JSValueSpec **schemata = (JSValueSpec**)SLHAlloc(builder->slh,len*sizeof(JSValueSpec*));
    accessPathPushName(accessPath,key);
    for (int i=0; i<len; i++){
      accessPathPushIndex(accessPath,i);
      schemata[i] = build(builder,jsonArrayGetItem(array,i),false);
      accessPathPop(accessPath);
    }
    accessPathPop(accessPath);
    return schemata;
  } else {
    return NULL;
  }
}

#define ALL_TYPES_COUNT 7
static char *allJSTypeNames[ALL_TYPES_COUNT] = { "null", "boolean", "object", "array", "number", "string", "integer"};

#define MISSING_VALIDATOR -7815551212l
#define MISSING_FLOAT_VALIDATOR -644212.639183

/* This function is recursive for all places that can be filled by a sub schema within a schema */
static JSValueSpec *build(JsonSchemaBuilder *builder, Json *jsValue, bool isTopLevel){
  AccessPath *accessPath = builder->accessPath;
  if (builder->traceLevel >= 2){
    printf("JSONSchema build\n");
    printAccessPath(stdout,accessPath);
    fflush(stdout);
  }
  if (jsonIsObject(jsValue)){
    JsonObject *object = jsonAsObject(jsValue);
    ensureUniqueKeys(builder,object);
    Json *typeValue = jsonObjectGetPropertyValue(object,"type");
    char **typeArray = NULL;
    int    typeArrayCount = 0;
    if (typeValue == NULL){
      if (builder->traceLevel >= 1){
        printf("*** INFO *** untyped value at path:\n");
        printAccessPath(stdout,accessPath);
      }
      typeArray = allJSTypeNames;
      typeArrayCount = ALL_TYPES_COUNT;
    } else {
      if (jsonIsString(typeValue)){
        typeArray = builderStringArray(builder,1);
        typeArray[0] = getStringOrFail(builder,object,"type");
        typeArrayCount = 1;
      } else if (jsonIsArray(typeValue)){
        typeArray = getStringArrayOrFail(builder,jsonAsArray(typeValue),&typeArrayCount);
      } else {
        schemaThrow(builder,12,"'type' must be either a single string or an array of string, not type=%d",typeValue->type);
      }
    }
        
    char *description = getString(builder,object,"description",NULL);
    char *title = getString(builder,object,"title",NULL);
    
    JSValueSpec *valueSpec = makeValueSpec(builder,getID(builder,object),description,title,typeArray,typeArrayCount);
    valueSpec->ref = getString(builder,object,"$ref",NULL);
    if (valueSpec->ref){
      /* THe JSON Schema spec says that if a $ref is defined it will be the only thing looked at, so
         we short-cicuit here. 

      "In Draft 4-7, $ref behaves a little differently. When an object
      contains a $ref property, the object is considered a reference,
      not a schema. Therefore, any other properties you put in that
      object will not be treated as JSON Schema keywords and will be
      ignored by the validator. $ref can only be used where a schema
      is expected."

      */
      return valueSpec;
    }
    valueSpec->definitions = getDefinitions(builder,object);
    valueSpec->allOf = getComposite(builder,object,"allOf");
    valueSpec->anyOf = getComposite(builder,object,"anyOf");
    valueSpec->oneOf = getComposite(builder,object,"oneOf");
    Json *notSpec = jsonObjectGetPropertyValue(object,"not");
    if (notSpec != NULL){
      accessPathPushName(accessPath,"not");
      valueSpec->not = build(builder,notSpec,false);
      accessPathPop(accessPath);
    }
    /// valueSpec.not = getComposite(object,accessPath);
    for (int typeCode=0; typeCode<=JSTYPE_MAX; typeCode++){
      if ((1 << typeCode) & valueSpec->typeMask){
        switch (typeCode){
        case JSTYPE_OBJECT:
          {
            JSValueSpec *objectSpec = valueSpec;
            Json *properties = jsonObjectGetPropertyValue(object,"properties");
            objectSpec->maxProperties = getIntegralValue(builder,object,"maxProperties",MISSING_VALIDATOR);
            objectSpec->minProperties = getIntegralValue(builder,object,"minProperties",MISSING_VALIDATOR);
            if (objectSpec->maxProperties != MISSING_VALIDATOR){
              checkNonNegative(builder,objectSpec->maxProperties,"maxProperties");
              objectSpec->validatorFlags |= JS_VALIDATOR_MAX_PROPS;
            }
            if (objectSpec->maxProperties != MISSING_VALIDATOR){
              checkNonNegative(builder,objectSpec->minProperties,"minProperties");
              objectSpec->validatorFlags |= JS_VALIDATOR_MIN_PROPS;
            }
            JsonArray *required = getArrayValue(builder,object,"required");
            if (required != NULL){
              int count = 0;
              objectSpec->required = getStringArrayOrFail(builder,required,&count);
              objectSpec->requiredCount = count;
            }
            if (properties != NULL){
              if (jsonIsObject(properties)){
                JsonObject *propertiesObject = jsonAsObject(properties);
                JsonProperty *property = jsonObjectGetFirstProperty(propertiesObject);
                while (property){
                  char *propertyName = jsonPropertyGetKey(property);
                  Json *propertyValue = jsonPropertyGetValue(property);
                  accessPathPushName(accessPath,propertyName);
                  if (valueSpec->properties == NULL){
                    valueSpec->properties = htCreate(101,stringHash,stringCompare,NULL,NULL);
                  }
                  htPut(valueSpec->properties,propertyName,build(builder,propertyValue,false));
                  accessPathPop(accessPath);
                  property = jsonObjectGetNextProperty(property);
                }
              } else {
                schemaThrow(builder,12,"properties is filled by JSON value that is not object with type=%d\n",properties->type);
              }
            } else {
              // properties is not required
            }
            JsonObject *dependentRequiredSpec = getObjectValue(builder,object,"dependentRequired");
            if (dependentRequiredSpec != NULL){
              objectSpec->dependentRequired = getStringToStringsMapOrFail(builder,dependentRequiredSpec);
            } 
          }
          break;
        case JSTYPE_ARRAY:
          {
            JSValueSpec *arraySpec = valueSpec;
            
            arraySpec->maxItems = getIntegralValue(builder,object,"maxItems",MISSING_VALIDATOR);
            arraySpec->minItems = getIntegralValue(builder,object,"minItems",MISSING_VALIDATOR);
            arraySpec->uniqueItems = getBooleanValue(builder,object,"uniqueItems",false);
            arraySpec->maxContains = getIntegralValue(builder,object,"maxContains",MISSING_VALIDATOR);
            arraySpec->minContains = getIntegralValue(builder,object,"minContains",MISSING_VALIDATOR);
            if (arraySpec->maxItems != MISSING_VALIDATOR){
              checkNonNegative(builder,arraySpec->maxItems,"maxItems");
              arraySpec->validatorFlags |= JS_VALIDATOR_MAX_ITEMS;
            }
            if (arraySpec->minItems != MISSING_VALIDATOR){
              checkNonNegative(builder,arraySpec->minItems,"minItems");
              arraySpec->validatorFlags |= JS_VALIDATOR_MIN_ITEMS;              
            }
            if (arraySpec->maxContains != MISSING_VALIDATOR){
              checkNonNegative(builder,arraySpec->maxContains,"maxContains");
              arraySpec->validatorFlags |= JS_VALIDATOR_MAX_CONTAINS;
            }
            if (arraySpec->minContains != MISSING_VALIDATOR){
              checkNonNegative(builder,arraySpec->minContains,"minContains");
              arraySpec->validatorFlags |= JS_VALIDATOR_MIN_CONTAINS;
            }
            // recursion on items
            Json *items = jsonObjectGetPropertyValue(object,"items");
            if (items != NULL){
              accessPathPushName(accessPath,"items");
              arraySpec->itemSpec = build(builder,items,false);
              accessPathPop(accessPath);
            } else {
              // what does this default to or error
            }
          }
          break;
        case JSTYPE_STRING:
          {
            JSValueSpec *stringSpec = valueSpec;
            stringSpec->maxLength = getIntegralValue(builder,object,"maxLength",MISSING_VALIDATOR);
            stringSpec->minLength = getIntegralValue(builder,object,"minLength",MISSING_VALIDATOR);
            if (stringSpec->maxLength != MISSING_VALIDATOR){
              checkNonNegative(builder,stringSpec->maxLength,"maxLength");
              stringSpec->validatorFlags |= JS_VALIDATOR_MAX_LENGTH;
            }
            if (stringSpec->minLength != MISSING_VALIDATOR){
              checkNonNegative(builder,stringSpec->minLength,"maxLength");
              stringSpec->validatorFlags |= JS_VALIDATOR_MIN_LENGTH;
            }
            stringSpec->pattern = getString(builder,object,"pattern",NULL); // hard to support on ANSI C
          }
          break;
        case JSTYPE_NULL:
          {
            // no customizations ?!
          }
          break;
        case JSTYPE_BOOLEAN:
          {
            // no customizations ?!
          }
          break;
        case JSTYPE_NUMBER:
        case JSTYPE_INTEGER:
          {
            JSValueSpec *numericSpec = valueSpec;
            // new JSNumericSpec(description,title,(jsType == JSType.INTEGER));
            numericSpec->isInteger = (typeCode == JSTYPE_INTEGER);
            numericSpec->multipleOf = getNumber(builder,object,"multipleOf",MISSING_FLOAT_VALIDATOR);
            numericSpec->maximum = getNumber(builder,object,"maximum",MISSING_FLOAT_VALIDATOR);
            numericSpec->exclusiveMaximum = getBooleanValue(builder,object,"exclusiveMaximum",false);
            numericSpec->minimum = getNumber(builder,object,"minimum",MISSING_FLOAT_VALIDATOR);
            numericSpec->exclusiveMinimum = getBooleanValue(builder,object,"exclusiveMinimum",false);
            if (numericSpec->maximum != MISSING_FLOAT_VALIDATOR){
              numericSpec->validatorFlags |= JS_VALIDATOR_MAX;
            }
            if (numericSpec->minimum != MISSING_FLOAT_VALIDATOR){
              numericSpec->validatorFlags |= JS_VALIDATOR_MIN;
            }
            if (numericSpec->multipleOf != MISSING_FLOAT_VALIDATOR){
              checkPositiveDouble(builder,numericSpec->multipleOf,"multipleOf");
              numericSpec->validatorFlags |= JS_VALIDATOR_MULTOF;
            }
          }
          break;
        default:
          printf("*** PANIC *** unknown JSType code=%d\n",typeCode);
        }
        
      }
    }// for (jsType...
    return valueSpec;
  } else {
    schemaThrow(builder,12,"top level schema must be an object");
    return NULL;
  }
}


JsonSchemaBuilder *makeJsonSchemaBuilder(int version){
  JsonSchemaBuilder *builder = (JsonSchemaBuilder*)safeMalloc(sizeof(JsonSchemaBuilder),"JsonBuilder");
  memset(builder,0,sizeof(JsonSchemaBuilder));
  builder->slh = makeShortLivedHeap(0x10000, 100);
  builder->version = version;
  builder->accessPath = (AccessPath*)SLHAlloc(builder->slh,sizeof(AccessPath));
  builder->accessPath->currentSize = 0;
  return builder;
}

void freeJsonSchemaBuilder(JsonSchemaBuilder *builder){
  if (builder->slh){
    SLHFree(builder->slh);
  }
  safeFree((char*)builder,sizeof(JsonSchemaBuilder));
}

JsonSchema *jsonBuildSchema(JsonSchemaBuilder *builder, Json *jsValue){
  if (setjmp(builder->recoveryData) == 0) {  /* normal execution */
    if (builder->traceLevel >= 2){
      printf("after setjmp normal\n");
      fflush(stdout);
    }
    JSValueSpec *topValueSpec = build(builder,jsValue,true);
    JsonSchema *schema = (JsonSchema*)SLHAlloc(builder->slh,sizeof(JsonSchema));
    schema->topValueSpec = topValueSpec;
    schema->version = builder->version;
    /* Take ownership of the SLH if build is successful */
    schema->slh = builder->slh;
    builder->slh = NULL;
    return schema;
  } else {                                   /* throw handling */
    if (builder->traceLevel >= 2){
      printf("schema build fail %s\n",builder->errorMessage);
      fflush(stdout);
    }
    return NULL;
  }
}