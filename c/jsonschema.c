

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

static char *getJSTypeName(int type){
  switch (type){
  case JSTYPE_NULL: return "null";
  case JSTYPE_BOOLEAN: return "boolean";
  case JSTYPE_OBJECT: return "objct";
  case JSTYPE_ARRAY: return "array";
  case JSTYPE_STRING: return "string";
  case JSTYPE_NUMBER: return "number";
  case JSTYPE_INTEGER: return "integer";
  default: return "unknown";
  }
}

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

typedef struct PatternProperty_tag {
  char    *pattern;
  regex_t *compiledPattern;
  int      compilationError;
  struct JSValueSpec_tag *valueSpec;
  struct PatternProperty_tag *next;
} PatternProperty;

typedef struct JSValueSpec_tag {
  int      typeMask;
  int      enumeratedValuesCount;
  Json   **enumeratedValues;
  Json    *constValue;
  char    *id;
  char    *hostName; /* from ID if set */
  char    *fileName; /* from ID if set */
  char    *description;
  char    *title;
  char    *anchor;
  char    *ref;
  bool     deprecated;
  bool     nullable;
  /* shared sub-schema definitions go here */
  hashtable *definitions;
  /* Quantifying/Compositing */
  int allOfCount;
  struct JSValueSpec_tag **allOf;
  int anyOfCount;
  struct JSValueSpec_tag **anyOf;
  int oneOfCount;
  struct JSValueSpec_tag **oneOf;
  struct JSValueSpec_tag *not;

  /* optional validators have flag for presence */
  uint64_t validatorFlags;
  /* only filled for top level JSValueSpecs, ie no parent, or $id is present */
  hashtable *anchorTable;
  /* everything with an ID under the top level */
  hashtable *idTable; 

  double minimum;
  double maximum;
  int64_t minContains;
  int64_t maxContains;
  int64_t minItems;
  int64_t maxItems;

  /* for Objects */
  int64_t minProperties;
  int64_t maxProperties;
  int      requiredCount;
  char   **required;
  hashtable *properties;
  hashtable *dependentRequired;
  bool       additionalProperties;
  PatternProperty *firstPatternProperty;
  PatternProperty *lastPatternProperty;

  /* for numbers */
  bool exclusiveMaximum;
  bool exclusiveMinimum;
  double multipleOf;
  /* bool isInteger; */

  /* for arrays */
  struct JSValueSpec_tag *itemSpec;
  bool uniqueItems;

  /* for strings */
  int64_t minLength;
  int64_t maxLength;
  char *pattern;
  regex_t *compiledPattern;
  int      regexCompilationError;

  /* back/up pointer */
  struct JSValueSpec_tag *parent;
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

static ValidityException *noteValidityException(JsonValidator *validator,
                                                bool isSpeculating,
                                                int invalidityCode,
                                                char *formatString, ...){
  if (isSpeculating){
    return NULL;
  }
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

JsonValidator *makeJsonValidator(void){
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

static JSValueSpec *getTopLevelAncestor(JSValueSpec *valueSpec){
  JSValueSpec *ancestor = valueSpec;
  while (ancestor->parent){
    if (ancestor->id != NULL){
      return ancestor;
    }
    ancestor = ancestor->parent;
  }
  return ancestor;
}

static JSValueSpec *getOutermostAncestor(JSValueSpec *valueSpec){
  JSValueSpec *ancestor = valueSpec;
  while (ancestor->parent){
    ancestor = ancestor->parent;
  }
  return ancestor;
}


/* The return types of these validators is bool to allow validator to signal whether to continue 
   to gather more validation exceptions in this part of the JSON tree/graph.  Returning false says
   this part is invalid enough such that further evaluation would probably confuse the user with 
   contradictory information. */

typedef enum VResult_tag {
  InvalidStop = 0,
  ValidStop = 1,
  InvalidContinue = 2,
  ValidContinue = 3
} VResult;

static bool vResultValid(VResult r){
  return (r == ValidStop) || (r == ValidContinue);
}

static VResult invertValidity(VResult r){
  return (VResult)(((int)r)^0x1);
}

static bool validateType(JsonValidator *validator,
                         bool isSpeculating,
                         int typeCode,
                         JSValueSpec *valueSpec){
  if (validator->traceLevel >= 1){
    printf("typeCode=%d shifted=0x%x mask=0x%x\n",typeCode,(1 << typeCode),valueSpec->typeMask);
  }
  if (((1 << typeCode) & valueSpec->typeMask) == 0){
    noteValidityException(validator,isSpeculating,12,"type '%s' not permitted at %s",
                          getJSTypeName(typeCode),validatorAccessPath(validator));
    return false;
  } else {
    return true;
  }
}

static regex_t *compileRegex(char *pattern, int *regexStatus){
  regex_t *rx = regexAlloc();
  int compStatus = regexComp(rx,pattern,REG_EXTENDED);
  if (compStatus){
    regexFree(rx);
    *regexStatus = compStatus;
    printf("*** WARNING *** pattern '%s' failed to compile with status %d\n",
           pattern,compStatus);
    return NULL;
  } else {
    *regexStatus = 0;
    return rx;
  }
}

static JSValueSpec *getPropertyValueSpec(JSValueSpec *valueSpec, char *propertyName){
  JSValueSpec *propertyValueSpec = NULL;
  if (valueSpec->properties){
    propertyValueSpec = (JSValueSpec*)htGet(valueSpec->properties,propertyName);
  }
  if (propertyValueSpec != NULL){
    return propertyValueSpec;
  }
  PatternProperty *patternProperty = valueSpec->firstPatternProperty;
  while (patternProperty){
    if (patternProperty->compilationError == 0){
      if (patternProperty->compiledPattern == NULL){
        patternProperty->compiledPattern = compileRegex(patternProperty->pattern,&patternProperty->compilationError);
      }
      if (patternProperty->compiledPattern){
        int regexStatus = regexExec(patternProperty->compiledPattern,propertyName,0,NULL,0);
        if (regexStatus == 0){
          return patternProperty->valueSpec;
        }
      }
    }
    patternProperty = patternProperty->next;
  }
  return NULL;
}

/* for mutual recursion */
static VResult validateJSON(JsonValidator *validator, bool isSpeculating, Json *value, JSValueSpec *valueSpec);

static VResult validateJSONObject(JsonValidator *validator,
                                  bool isSpeculating,
                                  JsonObject *object,
                                  JSValueSpec *valueSpec){
  int invalidCount = 0;
  AccessPath *accessPath = validator->accessPath;
  if (validator->traceLevel >= 1){
    printf("validateJSONObject required=0x%p\n",valueSpec->required);
    printAccessPath(stdout,accessPath);
    fflush(stdout);
  }
  if (valueSpec->required){
    for (int r=0; r<valueSpec->requiredCount; r++){
      char *requiredProperty = valueSpec->required[r];
      if (jsonObjectGetPropertyValue(object,requiredProperty) == NULL){
        noteValidityException(validator,isSpeculating,12,"missing required property '%s' at '%s'",
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
    if (validator->traceLevel >= 1){
      printf("validate object pname=%s\n",propertyName);
      fflush(stdout);
    }
    Json *propertyValue = jsonPropertyGetValue(property);
    JSValueSpec *propertySpec = getPropertyValueSpec(valueSpec,propertyName);
    accessPathPushName(accessPath,propertyName);
        
    if (propertySpec != NULL){
      VResult propResult = validateJSON(validator,isSpeculating,propertyValue,propertySpec);
      if (!vResultValid(propResult)){
        invalidCount++;
      }
    } else {
      if (valueSpec->additionalProperties == false){
        noteValidityException(validator,isSpeculating,12,"unspecified additional property not allowed: '%s' at '%s'",
                              propertyName,validatorAccessPath(validator));
      } else if (validator->flags && VALIDATOR_WARN_ON_UNDEFINED_PROPERTIES){
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
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"too many properties, %d > MAX=%d at %s",
                      propertyCount,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_PROPS){
    int64_t lim = valueSpec->minProperties;
    if (propertyCount < lim){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"too few properties, %d < MIN=%d at %s",
                            propertyCount,lim,validatorAccessPath(validator));
    }
  }
  return (invalidCount > 0 ? InvalidContinue : ValidContinue);
}

static VResult validateJSONArray(JsonValidator *validator,
                                 bool isSpeculating,
                                 JsonArray *array,
                                 JSValueSpec *valueSpec){
  AccessPath *accessPath = validator->accessPath;
  int invalidCount = 0;
  //boolean uniqueItems;
  // Long maxContains;
  // Long minContains;
  int elementCount = jsonArrayGetCount(array);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_ITEMS){
    int64_t lim = valueSpec->maxItems;
    if (elementCount > lim){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,
                            "too many array items, %d > MAX=%d at %s",
                            elementCount,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_ITEMS){
    int64_t lim = valueSpec->minItems;
    if (elementCount < lim){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,
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
      VResult elementStatus = validateJSON(validator,isSpeculating,itemValue,valueSpec->itemSpec);
      if (!vResultValid(elementStatus)){
        invalidCount++;
      }

    }
    if (valueSpec->uniqueItems){
      long longHash = jsonLongHash(itemValue);
      if (lhtGet(uniquenessSet,longHash) != NULL){
        invalidCount++;
        noteValidityException(validator,isSpeculating,12,"array uniqueItems violation %s is duplicate at %s",
                              itemValue,i,validatorAccessPath(validator));
      }
    }
    accessPathPop(accessPath);
  }
  return (invalidCount > 0 ? InvalidContinue : ValidContinue);
}


static VResult validateJSONString(JsonValidator *validator,
                                  bool isSpeculating,
                                  Json *string,
                                  JSValueSpec *valueSpec){
  int invalidCount = 0;
  char *s = jsonAsString(string);
  int len = strlen(s);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_LENGTH){
    long lim = valueSpec->maxLength;
    if (len > lim){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"string too long, %d > MAX=%d at %s",
                            len,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_LENGTH){
    long lim = valueSpec->minLength;
    if (len < lim){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"string too short, %d < MAX=%d at %s",
                            len,lim,validatorAccessPath(validator));
    }
  }
  if (valueSpec->pattern && (valueSpec->regexCompilationError == 0)){
    if (valueSpec->compiledPattern == NULL){
      valueSpec->compiledPattern = compileRegex(valueSpec->pattern,&valueSpec->regexCompilationError);
    }
    if (valueSpec->compiledPattern){
      int regexStatus = regexExec(valueSpec->compiledPattern,s,0,NULL,0);
      if (regexStatus != 0){
        invalidCount++;
        noteValidityException(validator,isSpeculating,12,"string pattern match fail s='%s', pat='%s', at %s",
                              s,valueSpec->pattern,validatorAccessPath(validator));
      }
    }
  }
  return (invalidCount > 0 ? InvalidContinue : ValidContinue);
}

static VResult validateJSONNumber(JsonValidator *validator,
                                  bool isSpeculating,
                                  Json *doubleOrLegacyInt,
                                  JSValueSpec *valueSpec){
  // Number multipleOf; // can be a filled, and must be > 0
  // Number exclusiveMaximum;
  // Number exclusiveMinimum;
  int invalidCount = 0;
  double d = (doubleOrLegacyInt->type == JSON_TYPE_NUMBER ?
              (double)jsonAsNumber(doubleOrLegacyInt) :
              jsonAsDouble(doubleOrLegacyInt));
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX){
    double lim = valueSpec->maximum;
    if (valueSpec->exclusiveMaximum ? (d >= lim) : (d > lim)){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"value too large, %f %s MAX=%f at %s",
                            d,
                            (valueSpec->exclusiveMaximum ? ">=" : ">"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN){
    double lim = valueSpec->minimum;
    if (valueSpec->exclusiveMinimum ? (d <= lim) : (d < lim)){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"value too small, %f %s MAX=%f at %s",
                            d,
                            (valueSpec->exclusiveMinimum ? "<=" : "<"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  return (invalidCount > 0 ? InvalidContinue : ValidContinue);
}

static VResult validateJSONInteger(JsonValidator *validator,
                                   bool isSpeculating,
                                   Json *value64,
                                   JSValueSpec *valueSpec){
  // Number multipleOf; // can be a filled, and must be > 0
  // Number exclusiveMaximum;
  // Number exclusiveMinimum;
  int invalidCount = 0;
  int64_t i = jsonAsInt64(value64);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX){
    int64_t lim = (int64_t)valueSpec->maximum;
    if (valueSpec->exclusiveMaximum ? (i >= lim) : (i > lim)){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"value too large, %lld %s MAX=%lld at %s",
                            i,
                            (valueSpec->exclusiveMaximum ? ">=" : ">"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN){
    int64_t lim = (int64_t)valueSpec->minimum;
    if (valueSpec->exclusiveMinimum ? (i <= lim) : (i < lim)){
      invalidCount++;
      noteValidityException(validator,isSpeculating,12,"value too small, %lld %s MAX=%lld at %s",
                            i,
                            (valueSpec->exclusiveMinimum ? "<=" : "<"),
                            lim,
                            validatorAccessPath(validator));
    }
  }
  return (invalidCount > 0 ? InvalidContinue : ValidContinue);
}

static char *httpPattern = "^(https?://[^/]+)/([^#]*)(#.*)?$";
static char *filePattern = "^/([^#]*)(#.*)?$";

static int httpMatch(JsonValidator *v, char *s){
  if (v->httpRegexError){
    return v->httpRegexError;
  } else if (v->httpRegex == NULL){
    v->httpRegex = compileRegex(httpPattern,&v->httpRegexError);
  }
  if (v->httpRegex){
    return regexExec(v->httpRegex,s,MAX_VALIDATOR_MATCHES,v->matches,0);
  } else {
    return v->httpRegexError;
  }
}

static int fileMatch(JsonValidator *v, char *s){
  if (v->fileRegexError){
    return v->fileRegexError;
  } else if (v->fileRegex == NULL){
    v->fileRegex = compileRegex(filePattern,&v->fileRegexError);
  }
  if (v->fileRegex){
    return regexExec(v->fileRegex,s,MAX_VALIDATOR_MATCHES,v->matches,0);
  } else {
    return v->fileRegexError;
  }
}

static JSValueSpec *getTopSchemaByName(JsonValidator *validator,
                                       char *hostName, int hostNameLength,
                                       char *fileName, int fileNameLength){
  int len = hostNameLength+fileNameLength+8;
  char *key = safeMalloc(len,"topSchemaKey");
  snprintf(key,len,"%*.*s/%*.*s",
           hostNameLength,hostNameLength,hostName,
           fileNameLength,fileNameLength,fileName);
  hashtable *definitionTable = validator->topSchema->topValueSpec->definitions;
  JSValueSpec *schema = (definitionTable ? htGet(definitionTable,key) : NULL);
  if (validator->traceLevel >= 1){
    printf("getTopSchema for key='%s' yields 0x%p (from HT)\n",key,schema);
  }
  if (schema == NULL){
    for (int i=0; i<validator->otherSchemaCount; i++){
      JsonSchema *otherSchema = validator->otherSchemas[i];
      JSValueSpec *otherTopSpec = otherSchema->topValueSpec;
      
      if (otherTopSpec->id && !strcmp(otherTopSpec->id,key)){
        schema = otherTopSpec;
        if (validator->traceLevel >= 1){
          printf("getTopSchema for key='%s' yields 0x%p (from Other Schemas)\n",key,schema);
        }
        break;
      }
    }
           
  }
  safeFree(key,len);
  return schema;
}

static int matchLen(regmatch_t *match){
  return (int)(match->rm_eo - match->rm_so);
}

static JSValueSpec *resolveCrossSchemaRef(JsonValidator *validator, bool isSpeculating,
                                          JSValueSpec *referredTopSchema, char *ref, regmatch_t *anchorMatch){
  if (anchorMatch->rm_so == -1){
    return referredTopSchema;
  } else if (referredTopSchema->anchorTable == NULL){
    noteValidityException(validator,isSpeculating,12,"schema '%s' does not define any anchors for ref '%s'",
                          (referredTopSchema->id ? referredTopSchema->id : "<anonymous>"),
                          ref);
    return NULL;
  } else {
    char *anchor = ref+anchorMatch->rm_so+1; /* +1 to skip the '#' char */
    JSValueSpec *spec = (JSValueSpec*)htGet(referredTopSchema->anchorTable,anchor);
    if (validator->traceLevel >= 1){
      printf("anchor '%s' resolves to 0x%p\n",anchor,spec);
    }
    if (spec == NULL){
      noteValidityException(validator,isSpeculating,12,"anchor schema ref '%s' does not resolve in referred schema",ref);
    }
    return spec;
  }
}


static JSValueSpec *resolveRef(JsonValidator *validator, bool isSpeculating, JSValueSpec *valueSpec){
  char *ref = valueSpec->ref;
  int refLen = strlen(ref);
  int matchStatus = 0;
  if (validator->traceLevel >= 1){
    printf("resolveRef ref=%s\n",valueSpec->ref);
  }
  JSValueSpec *containingSpec = getTopLevelAncestor(valueSpec);
  if (containingSpec == NULL){ /* this code is wrong if topLevel (parent chain) is no good */
    printf("*** PANIC *** no top level\n");
    return NULL;
  }
  
  /* Case 1 local reference w/o anchor */
  if (refLen >= 8 && !memcmp(ref,"#/$defs/",8)){   /* local resolution */
    int refLen = strlen(ref);
    int pos = 2;
    char *key = ref+8;
    JSValueSpec *topSchema = containingSpec;
    if (topSchema->definitions == NULL){
      noteValidityException(validator,isSpeculating,12,"schema '%s' does not define shared '$defs'",
                            (topSchema->id ? topSchema->id : "<anonymous>"));
      return NULL; /* for the compiler, bless its heart */
    }
    JSValueSpec *resolvedSchema = htGet(topSchema->definitions,key);
    if (resolvedSchema == NULL){
      noteValidityException(validator,isSpeculating,12,"path schema ref '%s' does not resolve against '$defs'",ref);
      return NULL;
    } else {
      return resolvedSchema;
    }
    /* Case 2, global URL reference */
  } else if (refLen >= 10 && ((matchStatus = httpMatch(validator,ref)) == 0)){
    regmatch_t domainMatch = validator->matches[1];
    regmatch_t fileMatch = validator->matches[2];
    regmatch_t anchorMatch = validator->matches[3];
    JSValueSpec *referredTopSchema = getTopSchemaByName(validator,
                                                        ref+domainMatch.rm_so,matchLen(&domainMatch),
                                                        ref+fileMatch.rm_so,matchLen(&fileMatch));
    if (referredTopSchema == NULL){
      noteValidityException(validator,isSpeculating,12,"cross-domain schema not found for ref '%s'",ref);
      return NULL;
    } else {
      return resolveCrossSchemaRef(validator,isSpeculating,referredTopSchema,ref,&anchorMatch);
    }
    /* Case 3, same-domain URL fragment (ie file) reference */
  } else if (refLen >= 1 && ((matchStatus = fileMatch(validator,ref)) == 0)){
    regmatch_t fileMatch = validator->matches[1];
    regmatch_t anchorMatch = validator->matches[2];
    JSValueSpec *referredTopSchema = getTopSchemaByName(validator,
                                                        containingSpec->hostName,strlen(containingSpec->hostName),
                                                        ref+fileMatch.rm_so,matchLen(&fileMatch));
    if (referredTopSchema == NULL){
      noteValidityException(validator,isSpeculating,12,"same-domain schema not found for ref '%s'",ref);
      return NULL;
    } else {
      return resolveCrossSchemaRef(validator,isSpeculating,referredTopSchema,ref,&anchorMatch);
    }
    /* Case 4, just an anchor in same schema */
  } else if (refLen >= 1 && ref[0] == '#'){
    if (containingSpec->anchorTable == NULL){
      noteValidityException(validator,isSpeculating,12,"schema '%s' does not define any anchors for ref '%s'",
                            (containingSpec->id ? containingSpec->id : "<anonymous>"),
                            ref);
      return NULL;
    }
    JSValueSpec *resolvedSchema = (JSValueSpec*)htGet(containingSpec->anchorTable,ref+1);
    if (resolvedSchema == NULL){
      noteValidityException(validator,isSpeculating,12,"anchor schema ref '%s' does not resolve in containing schema",ref);
      return NULL;
    } else {
      return resolvedSchema;
    }
    /* Other cases not known or supported */
  } else {
    /* notes, 
       need to merge URL according W3 rules and pass to pluggable resolver that
       we hope this validator has.
     */
    noteValidityException(validator,isSpeculating,12,"bad or unimplemented ref at '%s' at %s",
                          valueSpec->ref,validatorAccessPath(validator));
    return NULL;
  }
}

static VResult validateJSONSimple(JsonValidator *validator, bool isSpeculating, Json *value, JSValueSpec *valueSpec){
  if (jsonIsNull(value)){
    return (validateType(validator,isSpeculating,JSTYPE_NULL,valueSpec) ?
            ValidContinue : InvalidStop);
  } else if (jsonIsBoolean(value)){
    return (validateType(validator,isSpeculating,JSTYPE_BOOLEAN,valueSpec) ?
            ValidContinue : InvalidStop);
  } else if (jsonIsObject(value)){
    return (validateType(validator,isSpeculating,JSTYPE_OBJECT,valueSpec) ?
            validateJSONObject(validator,isSpeculating,jsonAsObject(value),valueSpec) :
            InvalidStop);
  } else if (jsonIsArray(value)){
    return (validateType(validator,isSpeculating,JSTYPE_ARRAY,valueSpec) ?
            validateJSONArray(validator,isSpeculating,jsonAsArray(value),valueSpec) :
            InvalidStop);
  } else if (jsonIsString(value)){
    return (validateType(validator,isSpeculating,JSTYPE_STRING,valueSpec) ?
            validateJSONString(validator,isSpeculating,value,valueSpec) :
            InvalidStop);
  } else if (jsonIsNumber(value)){
    if (!jsonIsInt64(value)){
      return (validateType(validator,isSpeculating,JSTYPE_NUMBER,valueSpec) ?
              validateJSONNumber(validator,isSpeculating,value,valueSpec) : /* general, comparisons done as doubles */
              InvalidStop);
    } else {
      return (validateType(validator,isSpeculating,JSTYPE_INTEGER,valueSpec) ?
              validateJSONInteger(validator,isSpeculating,value,valueSpec) :
              InvalidStop);
    }
    
  } else {
    printf("*** PANIC *** unhandled JS Value with type = %d\n",value->type);
    return InvalidStop;
  }
}

#define QUANT_ALL 1
#define QUANT_ANY 2
#define QUANT_ONE 3

static VResult validateQuantifiedSpecs(JsonValidator *validator,
                                       bool isSpeculating,
                                       Json *value, JSValueSpec **valueSpecs,
                                       int specCount, int quantType){
  int validCount = 0;
  
  for (int i=0; i<specCount; i++){
    JSValueSpec *valueSpec = valueSpecs[i];
    VResult eltValid = validateJSON(validator,
                                    isSpeculating | (quantType != QUANT_ALL),
                                    value,valueSpec);
    validCount += (vResultValid(eltValid) ? 1 : 0);
    if (validator->traceLevel >= 1){
      printf("vQS i=%d type=%d valid=%d vCount=%d\n",i,quantType,eltValid,validCount);
    }
    switch (quantType){
    case QUANT_ALL:
      if (!eltValid){
        noteValidityException(validator,isSpeculating,12,"not allOf schemas at '%s' are valid",
                              validatorAccessPath(validator));
        return InvalidStop;
      }
      break;
    case QUANT_ANY:
      if (eltValid){
        return ValidStop;
      }
    case QUANT_ONE:
      if (validCount > 1){
        noteValidityException(validator,isSpeculating,12,"more than oneOf schemas at '%s' are valid",
                              validatorAccessPath(validator));
        return InvalidStop;
      } 
      break;
    }
  }
  switch (quantType){
  case QUANT_ALL:
    return ValidContinue;
  case QUANT_ANY:
    noteValidityException(validator,isSpeculating,12,"not anyOf schemas at '%s' are valid",
                          validatorAccessPath(validator));

    return ValidContinue;
  case QUANT_ONE:
    /* printf("quantOne vCount=%d\n",validCount); */
    return (validCount == 1) ? ValidContinue : InvalidContinue;
  default:
    /* can't really get here */
    return InvalidStop;
  }
}

static bool jsonEquals(Json *a, Json *b){
  if (jsonIsArray(a) || jsonIsArray(b)){
    return false;
  } else if (jsonIsObject(a) || jsonIsObject(b)){
    return false;
  } else if (jsonIsString(a)){
    if (jsonIsString(b)){
      char *aStr = jsonAsString(a);
      char *bStr = jsonAsString(b);
      return !strcmp(aStr,bStr);
    } else {
      return false;
    }
  } else {
    return !memcmp(a,b,sizeof(Json));
  }
}

static VResult validateJSON(JsonValidator *validator, bool isSpeculating,Json *value, JSValueSpec *valueSpec){
  while (valueSpec->ref){
    valueSpec = resolveRef(validator,isSpeculating,valueSpec);
    if (valueSpec == NULL){
      return InvalidStop;
    }
  }
  if (validator->traceLevel >= 1){
    printf("validate JSON value->type=%d specTypeMask=0x%x\n",value->type,valueSpec->typeMask);
    fflush(stdout);
  }
  if (valueSpec->constValue){
    bool eq = jsonEquals(value,valueSpec->constValue);
    if (!eq){
      noteValidityException(validator,isSpeculating,12,"unequal constant value at %s",
                            validatorAccessPath(validator));
      return InvalidStop;
    } else {
      return ValidContinue;
    }
  } else if (valueSpec->enumeratedValues){
    bool matched = false;
    for (int ee=0; ee<valueSpec->enumeratedValuesCount; ee++){
      Json *enumValue = valueSpec->enumeratedValues[ee];
      if (jsonEquals(value,enumValue)){
        matched = true;
        break;
      }
    }
    if (!matched){
      noteValidityException(validator,isSpeculating,12,"no matching enum value at %s",
                            validatorAccessPath(validator));
      return InvalidStop;
    } else {
      return ValidContinue;
    }
  } else {
    /* type is string or array, does this mean to collapse ValueSpecTypes
       should we flag errors on out-of-bounds validation keywords for type keyword */
    VResult validity = validateJSONSimple(validator,isSpeculating,value,valueSpec);
    /* here, mix in the quantified and compound */
    if (!vResultValid(validity)) return validity;
    if (valueSpec->allOf){
      validity = validateQuantifiedSpecs(validator,isSpeculating,value,valueSpec->allOf,valueSpec->allOfCount,QUANT_ALL);
    }
    if (!vResultValid(validity)) return InvalidStop;
    if (valueSpec->anyOf){
      validity = validateQuantifiedSpecs(validator,isSpeculating,value,valueSpec->anyOf,valueSpec->anyOfCount,QUANT_ANY);
    }
    if (!vResultValid(validity)) return InvalidStop;
    if (valueSpec->oneOf){
      validity = validateQuantifiedSpecs(validator,isSpeculating,value,valueSpec->oneOf,valueSpec->oneOfCount,QUANT_ONE);
    }
    if (!vResultValid(validity)) return InvalidStop;
    if (valueSpec->not){
      validity = validateJSON(validator,true,value,valueSpec->not);
      if (vResultValid(validity)){
        noteValidityException(validator,isSpeculating,12,"negated schema at %s is valid",
                              validatorAccessPath(validator));
      }
      validity = invertValidity(validity);
    }
    return validity;
  }
}

int jsonValidateSchema(JsonValidator *validator, Json *value, JsonSchema *topSchema,
                       JsonSchema **otherSchemas, int otherSchemaCount){
  if (setjmp(validator->recoveryData) == 0) {  /* normal execution */
    validator->topSchema = topSchema;
    validator->otherSchemas = otherSchemas;
    validator->otherSchemaCount = otherSchemaCount;
    VResult validity = validateJSON(validator,false,value,topSchema->topValueSpec);
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
    schemaThrow(builder,12,"property '%s' must contain a boolean, not %s",propertyName,propertyValue);
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
static JSValueSpec *build(JsonSchemaBuilder *builder, JSValueSpec *parent, Json *jsValue, bool isTopLevel);

static void indexByAnchor(JSValueSpec *valueSpec){
  JSValueSpec *topLevelAncestor = getTopLevelAncestor(valueSpec);
  if (topLevelAncestor->anchorTable == NULL){
    topLevelAncestor->anchorTable = htCreate(257,stringHash,stringCompare,NULL,NULL);
  }
  htPut(topLevelAncestor->anchorTable,valueSpec->anchor,valueSpec);
}

static void indexByID(JSValueSpec *valueSpec){
  JSValueSpec *outermostAncestor = getOutermostAncestor(valueSpec);
  if (outermostAncestor->idTable == NULL){
    outermostAncestor->idTable = htCreate(257,stringHash,stringCompare,NULL,NULL);
  }
  htPut(outermostAncestor->idTable,valueSpec->id,valueSpec);
}

static char *extractString(JsonSchemaBuilder *b, char *s, regmatch_t *match){
  if (match->rm_so != -1){
    int len = (int)(match->rm_eo - match->rm_so);
    char *copy = SLHAlloc(b->slh,len+1);
    memcpy(copy,s+match->rm_so,len);
    copy[len] = 0;
    return copy;
  } else {
    return NULL;
  }
}


static JSValueSpec *makeValueSpec(JsonSchemaBuilder *builder,
                                  JSValueSpec *parent,
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
  spec->parent = parent;
  /* parent must be set */
  if (id){
    indexByID(spec);
    regmatch_t matches[10];

    int regexStatus = 0;
    regex_t *regex = compileRegex(httpPattern,&regexStatus);
    if (regex){
      int matchStatus = regexExec(regex,id,10,matches,0);
      if (matchStatus){
        fprintf(stderr,"id does not look like a URL: '%s', regexMatchStatus=%d\n",id,matchStatus);
      } else {
        spec->hostName = extractString(builder,id,&matches[1]);
        spec->fileName = extractString(builder,id,&matches[2]);
      }
      regexFree(regex);
    } 
    
  }
  for (int i=0; i<typeNameArrayLength; i++){
    int typeCode = getJSTypeForName(builder,typeNameArray[i]);
    spec->typeMask |= (1 << typeCode);
    if (typeCode == JSTYPE_NUMBER){
      /* numbers allow integers, but not the converse */
      spec->typeMask |= (1 << JSTYPE_INTEGER);
    }
  }
  return spec;
}
                                  

/* Map<String,JSValueSpec>  */
static hashtable *getDefinitions(JsonSchemaBuilder *builder, JSValueSpec *parent, JsonObject *object){
  AccessPath *accessPath = builder->accessPath;
  JsonObject *definitionsObject = getObjectValue(builder,object,"$defs");
  if (definitionsObject != NULL){
    hashtable *definitionMap = htCreate(101,stringHash,stringCompare,NULL,NULL);
    accessPathPushName(accessPath,"$defs");
    JsonProperty *property = jsonObjectGetFirstProperty(definitionsObject);
    while (property){
      char *propertyName = jsonPropertyGetKey(property);
      Json *propertyValue = jsonPropertyGetValue(property);
      if (propertyValue == NULL){
        schemaThrow(builder,12,"sub definition %s is NULL, which is not allowed",propertyName);
        return NULL;
      }
      accessPathPushName(accessPath,propertyName);
      htPut(definitionMap,propertyName,build(builder,parent,propertyValue,false));
      accessPathPop(accessPath);
      property = jsonObjectGetNextProperty(property);
    }
    accessPathPop(accessPath);
    return definitionMap;
  } else {
    return NULL;
  } 
}

static void addPatternProperties(JsonSchemaBuilder *builder, JSValueSpec *valueSpec, JsonObject *object){
  JsonObject *patternObject = getObjectValue(builder,object,"patternProperties");
  if (patternObject != NULL){
    AccessPath *accessPath = builder->accessPath;
    accessPathPushName(accessPath,"patternProperties");
    JsonProperty *property = jsonObjectGetFirstProperty(patternObject);
    while (property){
      char *propertyName = jsonPropertyGetKey(property);
      Json *propertyValue = jsonPropertyGetValue(property);
      if (propertyValue == NULL){
        schemaThrow(builder,12,"patternProperty %s is NULL, which is not allowed",propertyName);
        return;
      }
      accessPathPushName(accessPath,propertyName);
      PatternProperty *patternProperty = (PatternProperty*)SLHAlloc(builder->slh,sizeof(PatternProperty));
      if (valueSpec->firstPatternProperty == NULL){
        valueSpec->firstPatternProperty = patternProperty;
        valueSpec->lastPatternProperty = patternProperty;
      } else {
        valueSpec->lastPatternProperty->next = patternProperty;
        valueSpec->lastPatternProperty = patternProperty;
      }
      memset(patternProperty,0,sizeof(PatternProperty));
      patternProperty->pattern = propertyName;
      patternProperty->valueSpec = build(builder,valueSpec,propertyValue,false);
      accessPathPop(accessPath);
      property = jsonObjectGetNextProperty(property);
    }
    accessPathPop(accessPath);
  }
}

static JSValueSpec **getComposite(JsonSchemaBuilder *builder, JSValueSpec *parent,
                                  JsonObject *object, char *key, int *countPtr){
  AccessPath *accessPath = builder->accessPath;
  JsonArray *array = jsonObjectGetArray(object,key);
  if (array != NULL){
    int len = jsonArrayGetCount(array);
    *countPtr = len;
    JSValueSpec **schemata = (JSValueSpec**)SLHAlloc(builder->slh,len*sizeof(JSValueSpec*));
    accessPathPushName(accessPath,key);
    for (int i=0; i<len; i++){
      accessPathPushIndex(accessPath,i);
      schemata[i] = build(builder,parent,jsonArrayGetItem(array,i),false);
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
static JSValueSpec *build(JsonSchemaBuilder *builder, JSValueSpec *parent, Json *jsValue, bool isTopLevel){
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
    char *id = getID(builder,object);
    JSValueSpec *valueSpec = makeValueSpec(builder,parent,id,description,title,typeArray,typeArrayCount);
    valueSpec->ref = getString(builder,object,"$ref",NULL);
    valueSpec->anchor = getString(builder,object,"$anchor",NULL);
    if (valueSpec->anchor){
      indexByAnchor(valueSpec);
    }
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
    valueSpec->definitions = getDefinitions(builder,valueSpec,object);
    valueSpec->additionalProperties = getBooleanValue(builder,object,"additionalProperties",true);
    addPatternProperties(builder,valueSpec,object);
    valueSpec->allOf = getComposite(builder,valueSpec,object,"allOf",&valueSpec->allOfCount);
    valueSpec->anyOf = getComposite(builder,valueSpec,object,"anyOf",&valueSpec->anyOfCount);
    valueSpec->oneOf = getComposite(builder,valueSpec,object,"oneOf",&valueSpec->oneOfCount);
    Json *notSpec = jsonObjectGetPropertyValue(object,"not");
    if (notSpec != NULL){
      accessPathPushName(accessPath,"not");
      valueSpec->not = build(builder,valueSpec,notSpec,false);
      accessPathPop(accessPath);
    }
    Json *constValue = jsonObjectGetPropertyValue(object,"const");
    JsonArray *enumValues = getArrayValue(builder,object,"enum");
    if (constValue && enumValues){
      schemaThrow(builder,12,"cannot specify both 'const' and 'enum' for the same schema");
    }
    if (constValue){
      valueSpec->constValue = jsonCopy(builder->slh,constValue);
    }
    if (enumValues){
      valueSpec->enumeratedValuesCount = jsonArrayGetCount(enumValues);
      valueSpec->enumeratedValues = (Json**)SLHAlloc(builder->slh,valueSpec->enumeratedValuesCount*sizeof(Json*));
      for (int ee=0; ee<valueSpec->enumeratedValuesCount; ee++){
        valueSpec->enumeratedValues[ee] = jsonCopy(builder->slh,jsonArrayGetItem(enumValues,ee));
      }
    }
    /// valueSpec.not = getComposite(object,accessPath);
    bool handledNumeric = false;
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
                  htPut(valueSpec->properties,propertyName,build(builder,valueSpec,propertyValue,false));
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
              arraySpec->itemSpec = build(builder,valueSpec,items,false);
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
            if (!handledNumeric){
              handledNumeric = true;
              JSValueSpec *numericSpec = valueSpec;
              // new JSNumericSpec(description,title,(jsType == JSType.INTEGER));
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
          }
          break;
        default:
          printf("*** PANIC *** unknown JSType code=%d\n",typeCode);
        }
        
      }
    }/*  for (jsType... */
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

JsonSchema *jsonBuildSchema(JsonSchemaBuilder *builder,
                            Json  *schemaJson){
  if (setjmp(builder->recoveryData) == 0) {  /* normal execution */
    if (builder->traceLevel >= 2){
      printf("after setjmp normal\n");
      fflush(stdout);
    }
    JSValueSpec *topValueSpec = build(builder,NULL,schemaJson,true);
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

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
