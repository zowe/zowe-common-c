

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
  case JSTYPE_OBJECT: return "object";
  case JSTYPE_ARRAY: return "array";
  case JSTYPE_STRING: return "string";
  case JSTYPE_NUMBER: return "number";
  case JSTYPE_INTEGER: return "integer";
  default: return "unknown";
  }
}

static void trace(JsonValidator *validator, int depth, char *formatString, ...){
  for (int i=0; i<depth; i++){
    fprintf(validator->traceOut,"  ");
  }
  va_list argPointer; 
  va_start(argPointer,formatString);
  vfprintf(validator->traceOut,formatString,argPointer);
  va_end(argPointer);
  fflush(validator->traceOut);
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
  int      type;
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
  bool       unevaluatedProperties;
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

static char *fillAccessPathString(char *buffer, int bufferLen, AccessPath *path){
  memset(buffer,0,bufferLen);
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

static ValidityException *makeValidityException(JsonValidator *validator, char *message){
  ValidityException *exception = (ValidityException*)SLHAlloc(validator->evalHeap,sizeof(ValidityException));
  memset(exception,0,sizeof(ValidityException));
  exception->message = message;
  return exception;
}

static void addValidityChild(ValidityException *parent, ValidityException *exception){
  if (parent->firstChild == NULL){
    parent->firstChild = exception;
  } else {
    ValidityException *child = parent->firstChild;
    while (child->nextSibling){
      child = child->nextSibling;
    }
    child->nextSibling = exception;
  }
}

static bool hasChildren(ValidityException *e){
  return e->firstChild ? true : false;
}

static char *validityMessage(JsonValidator *validator,
                             char *formatString, ...){
  va_list argPointer;
  char *message = SLHAlloc(validator->evalHeap,MAX_VALIDITY_EXCEPTION_MSG);

  va_start(argPointer,formatString);
  vsnprintf(message,MAX_VALIDITY_EXCEPTION_MSG,formatString,argPointer);
  va_end(argPointer);

  return message;
}

static VResult simpleFailure(JsonValidator *validator,
                             char *formatString, ...){
  va_list argPointer;
  char *message = SLHAlloc(validator->evalHeap,MAX_VALIDITY_EXCEPTION_MSG);

  va_start(argPointer,formatString);
  vsnprintf(message,MAX_VALIDITY_EXCEPTION_MSG,formatString,argPointer);
  va_end(argPointer);

  VResult result;
  result.status = InvalidStop;
  result.exception = makeValidityException(validator,message);
  return result;
}

static VResult failureWithException(ValidityException *exception){
  VResult result;
  result.status = InvalidStop;
  result.exception = exception;
  return result;
}

static VResult simpleSuccess(){
  VResult result;
  result.status = ValidContinue;
  result.exception = NULL;
  return result;
}

JsonValidator *makeJsonValidator(void){
  JsonValidator *validator = (JsonValidator*)safeMalloc(sizeof(JsonValidator),"JsonValidator");
  memset(validator,0,sizeof(JsonValidator));
  validator->accessPath = (AccessPath*)safeMalloc(sizeof(AccessPath),"JsonValidatorAccessPath");
  validator->accessPath->currentSize = 0;
  validator->traceOut = stderr;
  return validator;
}

void freeJsonValidator(JsonValidator *validator){
  if (validator->evalHeap){
    SLHFree(validator->evalHeap);
  }
  safeFree((char*)validator->accessPath,sizeof(AccessPath));
  safeFree((char*)validator,sizeof(JsonValidator));
}

static char *validatorAccessPath(JsonValidator *validator){
  return fillAccessPathString(validator->accessPathBuffer,MAX_ACCESS_PATH,validator->accessPath);
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

static bool vStatusValid(VStatus r){
  return (r == ValidStop) || (r == ValidContinue);
}

static VStatus invertValidity(VStatus r){
  return (VStatus)(((int)r)^0x1);
}

static char *vStatusName(VStatus s){
  switch (s){
  case InvalidStop: return "IS";
  case ValidStop: return "VS";
  case InvalidContinue: return "IC";
  case ValidContinue: return "VC";
  default: return "??";
  }
}

static VStatus vStatusMin(VStatus a, VStatus b){
  int aNum = (int)a;
  int bNum = (int)b;
  int minValid = min((aNum&1),(bNum&1));
  int minContinue = min((aNum&2),(bNum&2));
  return (VStatus)(minContinue|minValid);
}

static VResult validateType(JsonValidator *validator,
                            int typeCode,
                            JSValueSpec *valueSpec,
                            int depth){
  if (validator->traceLevel >= 1){
    trace(validator,depth,"typeCode=%d shifted=0x%x mask=0x%x\n",typeCode,(1 << typeCode),valueSpec->typeMask);
  }
  if (((1 << typeCode) & valueSpec->typeMask) == 0){
    return simpleFailure(validator,"type '%s' not permitted at %s expecting type '%s'",
                         getJSTypeName(typeCode),validatorAccessPath(validator),
                         getJSTypeName(valueSpec->type));
  } else {
    return simpleSuccess();
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

/*
  each JsonObject in the tree has a set of evaluated properties
  that grows and shrinks as validation rules run.

  for each value object only things seen in this traversal (lateness, deepness) are considered "evaluated"

  try to dump the evaluated set for each validateOBject

  each JSValueSpec evaluation has a set of (obj,pname) pairs that have been evaluated in its recursive descent
    this a large-ish but well-defined construction
    this is also true for (array,index)

  Sunday

  the speculation feature is wrong
    invalidations should be in a tree and everything that is checked should be placed under a parent
    so, pass a subInvalidations bucket into each recursive call, and if failing the current level, add those children
  
*/

typedef struct PropertyEvalRecord_tag {
  char *name;
  struct PropertyEvalRecord_tag *next;
} PropertyEvalRecord;

typedef struct ObjectEvalRecord_tag {
  JsonObject *object;
  PropertyEvalRecord *properties;
  struct ObjectEvalRecord_tag *next;
} ObjectEvalRecord;

typedef struct EvalSet_tag {
  ObjectEvalRecord *objectRecords;
  struct EvalSet_tag *next;
} EvalSet;

static PropertyEvalRecord *getPropertyEvalRecord(ObjectEvalRecord *objectRecord, char *propertyName){
  PropertyEvalRecord *record = objectRecord->properties;
  while (record){
    if (!strcmp(record->name,propertyName)){
      return record;
    }
    record = record->next;
  }
  return NULL;
}

static ObjectEvalRecord *getObjectEvalRecord(EvalSet *evalSet, JsonObject *object){
  ObjectEvalRecord *record = evalSet->objectRecords;
  while (record){
    if (record->object == object){
      return record;
    }
    record = record->next;
  }
  return NULL;
}

static void addPropertyToObjectRecord(JsonValidator *validator,
                                      ObjectEvalRecord *objectEvalRecord,
                                      char *propertyName){
  if (!getPropertyEvalRecord(objectEvalRecord,propertyName)){
    PropertyEvalRecord *record = (PropertyEvalRecord*)SLHAlloc(validator->evalHeap,sizeof(PropertyEvalRecord));
    record->name = propertyName;
    record->next = objectEvalRecord->properties;
    objectEvalRecord->properties = record;
  }
}

static EvalSet *extendEvalSets(JsonValidator *validator, EvalSet *evalSets){
  EvalSet *newSet = (EvalSet*)SLHAlloc(validator->evalHeap,sizeof(EvalSet));
  newSet->objectRecords = NULL;
  newSet->next = evalSets;
  return newSet;
}

static void notePropertyEvaluation(JsonValidator *validator, EvalSet *evalSetList, JsonObject *object, char *propertyName){
  EvalSet *evalSet = evalSetList;
  /* add to *ALL* eval sets */
  while (evalSet){
    ObjectEvalRecord *objectEvalRecord = getObjectEvalRecord(evalSet,object);
    if (objectEvalRecord == NULL){
      objectEvalRecord = (ObjectEvalRecord*)SLHAlloc(validator->evalHeap,sizeof(ObjectEvalRecord));
      objectEvalRecord->object = object;
      objectEvalRecord->properties = NULL;
      objectEvalRecord->next = evalSet->objectRecords;
      evalSet->objectRecords = objectEvalRecord;
    }
    addPropertyToObjectRecord(validator,objectEvalRecord,propertyName);
    evalSet = evalSet->next;
  }
}

static void traceEvalSets(JsonValidator *validator, int depth, EvalSet *evalSetList){
  EvalSet *evalSet = evalSetList;
  while (evalSet){
    trace(validator,depth,"EvalSet 0x%p\n",evalSet);
    ObjectEvalRecord *objectRecord = evalSet->objectRecords;
    while (objectRecord){
      trace(validator,depth+1,"For JsonObj=0x%p:",objectRecord->object);
      PropertyEvalRecord *propertyRecord = objectRecord->properties;
      while (propertyRecord){
        trace(validator,0," '%s'",propertyRecord->name);
        propertyRecord = propertyRecord->next;
      }
      trace(validator,0,"\n");
      objectRecord = objectRecord->next;
    }
    evalSet = evalSet->next;
  }
}

static bool hasBeenEvaluated(EvalSet *evalSet, JsonObject *object, char *propertyName){
  ObjectEvalRecord *objectRecord = evalSet->objectRecords;
  while (objectRecord){
    if (objectRecord->object == object){
      PropertyEvalRecord *propertyRecord = objectRecord->properties;
      while (propertyRecord){
        if (!strcmp(propertyRecord->name,propertyName)){
          return true;
        }
        propertyRecord = propertyRecord->next;
      }
    }
    objectRecord = objectRecord->next;
  }
  return false;
}

/* for mutual recursion */
static VResult validateJSON(JsonValidator *validator, Json *value, JSValueSpec *valueSpec, int depth,
                            EvalSet *evalSetList);

static VResult validateJSONObject(JsonValidator *validator,
                                  JsonObject *object,
                                  JSValueSpec *valueSpec,
                                  int depth,
                                  EvalSet *evalSetList){
  ValidityException *pendingException = makeValidityException(validator,NULL);
  int invalidCount = 0;
  AccessPath *accessPath = validator->accessPath;
  if (validator->traceLevel >= 1){
    trace(validator,depth,"validateJSONObject required=[");
    for (int r=0; r<valueSpec->requiredCount; r++){
      trace(validator,0,"\"%s\", ",valueSpec->required[r]);
    }
    trace(validator,0,"]\n"); /* 0 because is line fragment */
    trace(validator,depth,"accessPath (top is blank):\n");
    printAccessPath(stdout,accessPath);
    fflush(validator->traceOut);
  }
  if (valueSpec->required){
    for (int r=0; r<valueSpec->requiredCount; r++){
      char *requiredProperty = valueSpec->required[r];
      Json *propertyValue = jsonObjectGetPropertyValue(object,requiredProperty);
      if (validator->traceLevel >= 1){
        trace(validator,depth,"  req prop check on '%s': 0x%p\n",requiredProperty,propertyValue);
      }
      if (propertyValue == NULL){
        addValidityChild(pendingException,
                         makeValidityException(validator,
                                               validityMessage(validator,
                                                               "missing required property '%s' at '%s'",
                                                               requiredProperty,validatorAccessPath(validator))));
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
      trace(validator,depth,"validate object pname=%s\n",propertyName);
    }
    Json *propertyValue = jsonPropertyGetValue(property);
    JSValueSpec *propertySpec = getPropertyValueSpec(valueSpec,propertyName);
    accessPathPushName(accessPath,propertyName);
    /* fprintf(validator->traceOut,"PUSH %s, pthsz=%d\n",propertyName,validator->accessPath->currentSize); */
    if (propertySpec != NULL){
      VResult propResult = validateJSON(validator,propertyValue,propertySpec,depth+1,evalSetList);
      notePropertyEvaluation(validator,evalSetList,object,propertyName);
      if (!vStatusValid(propResult.status)){
        addValidityChild(pendingException,propResult.exception);
      }
    } else {
      if (valueSpec->additionalProperties == false){
        addValidityChild(pendingException,
                         makeValidityException(validator,
                                               validityMessage(validator,
                                                               "unspecified additional property not allowed: '%s' at '%s'",
                                                               propertyName,validatorAccessPath(validator))));
      } else if (valueSpec->unevaluatedProperties == false){
        if (validator->traceLevel >= 1){
          trace(validator,depth,"Is '%s' in the following eval sets for obj=0x%p myEvalSet=0x%p\n",propertyName,object,evalSetList);
          traceEvalSets(validator,depth+1,evalSetList);
        }
        EvalSet *ourEvalSet = evalSetList; /* The first evalSet in the list must contain the evaluation of any property 
                                              at this "level" in the schema evalution.  It's kinda complex.
                                           */
        if (!hasBeenEvaluated(ourEvalSet,object,propertyName)){
          if (validator->traceLevel >= 1){
            trace(validator,depth,"Invalid object on unevaluated property '%s'\n",propertyName);
          }
          addValidityChild(pendingException,
                           makeValidityException(validator,
                                                 validityMessage(validator,
                                                                 "unevaluated property not allowed: '%s' at '%s'",
                                                                 propertyName,validatorAccessPath(validator))));
        }
      } else if (validator->flags && VALIDATOR_WARN_ON_UNDEFINED_PROPERTIES){
        trace(validator,depth,"*WARNING* unspecified property seen, '%s', and checking code is not complete, vspec->props=0x%p\n",
               propertyName,valueSpec->properties);
      }
      if (valueSpec->properties){ 
        /* htDump(valueSpec->properties); */
      }
    }
    accessPathPop(accessPath);
    /* fprintf(validator->traceOut,"POP %s, pthsz=%d\n",propertyName,validator->accessPath->currentSize); */
    property = jsonObjectGetNextProperty(property);
  }
  
  // Interdependencies
  if (valueSpec->dependentRequired != NULL){
    trace(validator,depth,"*WARNING* depenentRequired not yet implemented\n");
  }

  
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_PROPS){
    int64_t lim = valueSpec->minProperties;
    if (propertyCount > lim){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,"too many properties, %d > MAX=%d at %s",
                                                             propertyCount,lim,validatorAccessPath(validator))));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_PROPS){
    int64_t lim = valueSpec->minProperties;
    if (propertyCount < lim){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,"too few properties, %d < MIN=%d at %s",
                                                             propertyCount,lim,validatorAccessPath(validator))));
    }
  }
  if (hasChildren(pendingException)){
    pendingException->message = validityMessage(validator,"Validity Exceptions(s) with object at %s",
                                                validatorAccessPath(validator));
    return failureWithException(pendingException);
  } else {
    return simpleSuccess();
  }
}

static VResult validateJSONArray(JsonValidator *validator,
                                 JsonArray *array,
                                 JSValueSpec *valueSpec,
                                 int depth,
                                 EvalSet *evalSetList){
  AccessPath *accessPath = validator->accessPath;
  ValidityException *pendingException = makeValidityException(validator,NULL);

  //boolean uniqueItems;
  // Long maxContains;
  // Long minContains;
  int elementCount = jsonArrayGetCount(array);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_ITEMS){
    int64_t lim = valueSpec->maxItems;
    if (elementCount > lim){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "too many array items, %d > MAX=%d at %s",
                                                             elementCount,lim,validatorAccessPath(validator))));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_ITEMS){
    int64_t lim = valueSpec->minItems;
    if (elementCount < lim){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "too few array items, %d < MIN=%d at %s",
                                                             elementCount,lim,validatorAccessPath(validator))));
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
      VResult elementResult = validateJSON(validator,itemValue,valueSpec->itemSpec,depth+1,evalSetList);
      if (!vStatusValid(elementResult.status)){
        addValidityChild(pendingException,elementResult.exception);
      }
    }
    
    if (valueSpec->uniqueItems){
      int64_t longHash = jsonLongHash(itemValue);
      
      if (lhtGet(uniquenessSet,longHash) != NULL){
        addValidityChild(pendingException,
                         makeValidityException(validator,
                                               validityMessage(validator,
                                                               "array uniqueItems violation, duplicate at %s",
                                                               validatorAccessPath(validator))));
      }
      lhtPut(uniquenessSet,longHash,itemValue);
    }
    accessPathPop(accessPath);
  }
  if (hasChildren(pendingException)){
    pendingException->message = validityMessage(validator,"Validity Exceptions(s) with array at %s",
                                                validatorAccessPath(validator));
    return failureWithException(pendingException);
  } else {
    return simpleSuccess();
  }
}


static VResult validateJSONString(JsonValidator *validator,
                                  Json *string,
                                  JSValueSpec *valueSpec,
                                  int depth){
  ValidityException *pendingException = makeValidityException(validator,NULL);
  
  char *s = jsonAsString(string);
  int len = strlen(s);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX_LENGTH){
    long lim = valueSpec->maxLength;
    if (len > lim){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "string too long (len=%d) '%s' %d > MAX=%d at %s",
                                                             len,s,len,lim,validatorAccessPath(validator))));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN_LENGTH){
    long lim = valueSpec->minLength;
    if (len < lim){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "string too short (len=%d) '%s' %d < MIN=%d at %s",
                                                             len,s,len,lim,validatorAccessPath(validator))));
    }
  }
  if (valueSpec->pattern && (valueSpec->regexCompilationError == 0)){
    if (valueSpec->compiledPattern == NULL){
      valueSpec->compiledPattern = compileRegex(valueSpec->pattern,&valueSpec->regexCompilationError);
    }
    if (valueSpec->compiledPattern){
      int regexStatus = regexExec(valueSpec->compiledPattern,s,0,NULL,0);
      if (regexStatus != 0){
        addValidityChild(pendingException,
                         makeValidityException(validator,
                                               validityMessage(validator,
                                                               "string pattern match fail s='%s', pat='%s', at %s",
                                                               s,valueSpec->pattern,validatorAccessPath(validator))));
      }
    }
  }
  if (hasChildren(pendingException)){
    pendingException->message = validityMessage(validator,"Validity Exceptions(s) with string at %s",
                                                validatorAccessPath(validator));
    return failureWithException(pendingException);
  } else {
    return simpleSuccess();
  }
}

static VResult validateJSONNumber(JsonValidator *validator,
                                  Json *doubleOrLegacyInt,
                                  JSValueSpec *valueSpec,
                                  int depth){
  // Number multipleOf; // can be a filled, and must be > 0
  // Number exclusiveMaximum;
  // Number exclusiveMinimum;
  ValidityException *pendingException = makeValidityException(validator,NULL);
  double d = (doubleOrLegacyInt->type == JSON_TYPE_NUMBER ?
              (double)jsonAsNumber(doubleOrLegacyInt) :
              jsonAsDouble(doubleOrLegacyInt));
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX){
    double lim = valueSpec->maximum;
    if (valueSpec->exclusiveMaximum ? (d >= lim) : (d > lim)){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "value too large, %f %s MAX=%f at %s",
                                                             d,
                                                             (valueSpec->exclusiveMaximum ? ">=" : ">"),
                                                             lim,
                                                             validatorAccessPath(validator))));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN){
    double lim = valueSpec->minimum;
    if (valueSpec->exclusiveMinimum ? (d <= lim) : (d < lim)){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "value too small, %f %s MIN=%f at %s",
                                                             d,
                                                             (valueSpec->exclusiveMinimum ? "<=" : "<"),
                                                             lim,
                                                             validatorAccessPath(validator))));
    }
  }
  if (hasChildren(pendingException)){
    pendingException->message = validityMessage(validator,"Validity Exceptions(s) with number at %s",
                                                validatorAccessPath(validator));
    return failureWithException(pendingException);
  } else {
    return simpleSuccess();
  }
}

static VResult validateJSONInteger(JsonValidator *validator,
                                   Json *value64,
                                   JSValueSpec *valueSpec,
                                   int depth){
  // Number multipleOf; // can be a filled, and must be > 0
  // Number exclusiveMaximum;
  // Number exclusiveMinimum;
  ValidityException *pendingException = makeValidityException(validator,NULL);
  int64_t i = jsonAsInt64(value64);
  if (valueSpec->validatorFlags & JS_VALIDATOR_MAX){
    int64_t lim = (int64_t)valueSpec->maximum;
    if (valueSpec->exclusiveMaximum ? (i >= lim) : (i > lim)){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "value too large, %lld %s MAX=%lld at %s",
                                                             i,
                                                             (valueSpec->exclusiveMaximum ? ">=" : ">"),
                                                             lim,
                                                             validatorAccessPath(validator))));
    }
  }
  if (valueSpec->validatorFlags & JS_VALIDATOR_MIN){
    int64_t lim = (int64_t)valueSpec->minimum;
    if (valueSpec->exclusiveMinimum ? (i <= lim) : (i < lim)){
      addValidityChild(pendingException,
                       makeValidityException(validator,
                                             validityMessage(validator,
                                                             "value too small, %lld %s MIN=%lld at %s",
                                                             i,
                                                             (valueSpec->exclusiveMinimum ? "<=" : "<"),
                                                             lim,
                                                             validatorAccessPath(validator))));
    }
  }
  if (hasChildren(pendingException)){
    pendingException->message = validityMessage(validator,"Validity Exceptions(s) with integer at %s",
                                                validatorAccessPath(validator));
    return failureWithException(pendingException);
  } else {
    return simpleSuccess();
  }  
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
                                       char *fileName, int fileNameLength,
                                       int depth){
  int len = hostNameLength+fileNameLength+8;
  char *key = safeMalloc(len,"topSchemaKey");
  snprintf(key,len,"%*.*s/%*.*s",
           hostNameLength,hostNameLength,hostName,
           fileNameLength,fileNameLength,fileName);
  hashtable *definitionTable = validator->topSchema->topValueSpec->definitions;
  JSValueSpec *schema = (definitionTable ? htGet(definitionTable,key) : NULL);
  if (validator->traceLevel >= 1){
    trace(validator,depth,"getTopSchema for key='%s' yields 0x%p (from HT)\n",key,schema);
  }
  if (schema == NULL){
    for (int i=0; i<validator->otherSchemaCount; i++){
      JsonSchema *otherSchema = validator->otherSchemas[i];
      JSValueSpec *otherTopSpec = otherSchema->topValueSpec;
      
      if (validator->traceLevel >= 1){
	trace(validator,depth,"getTopSchema compare key='%s' otherTopID='%s' \n",key,otherTopSpec->id);
      }
      if (otherTopSpec->id && !strcmp(otherTopSpec->id,key)){
        schema = otherTopSpec;
        if (validator->traceLevel >= 1){
          trace(validator,depth,"getTopSchema for key='%s' yields 0x%p (from Other Schemas)\n",key,schema);
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

static JSValueSpec *resolveCrossSchemaRef(JsonValidator *validator, ValidityException *pendingException,
                                          JSValueSpec *referredTopSchema, char *ref, regmatch_t *anchorMatch,
                                          int depth){
  if (anchorMatch->rm_so == -1){
    return referredTopSchema;
  } else if (referredTopSchema->anchorTable == NULL){
    pendingException->message = validityMessage(validator,"schema '%s' does not define any anchors for ref '%s'",
                                                (referredTopSchema->id ? referredTopSchema->id : "<anonymous>"),
                                                ref);
    return NULL;
  } else {
    char *anchor = ref+anchorMatch->rm_so+1; /* +1 to skip the '#' char */
    JSValueSpec *spec = (JSValueSpec*)htGet(referredTopSchema->anchorTable,anchor);
    if (validator->traceLevel >= 1){
      trace(validator,depth,"anchor '%s' resolves to 0x%p\n",anchor,spec);
    }
    if (spec == NULL){
      pendingException->message = validityMessage(validator,"anchor schema ref '%s' does not resolve in referred schema",ref);
    }
    return spec;
  }
}


static JSValueSpec *resolveRef(JsonValidator *validator, ValidityException *pendingException, JSValueSpec *valueSpec, int depth){
  char *ref = valueSpec->ref;
  int refLen = strlen(ref);
  int matchStatus = 0;
  if (validator->traceLevel >= 1){
    trace(validator,depth,"resolveRef ref=%s\n",valueSpec->ref);
  }
  JSValueSpec *containingSpec = getTopLevelAncestor(valueSpec);
  if (containingSpec == NULL){ /* this code is wrong if topLevel (parent chain) is no good */
    trace(validator,depth,"*** PANIC *** no top level\n");
    return NULL;
  }
  
  /* Case 1 local reference w/o anchor */
  if (refLen >= 8 && !memcmp(ref,"#/$defs/",8)){   /* local resolution */
    int refLen = strlen(ref);
    int pos = 2;
    char *key = ref+8;
    JSValueSpec *topSchema = containingSpec;
    if (topSchema->definitions == NULL){
      pendingException->message = validityMessage(validator,"schema '%s' does not define shared '$defs'",
                                                  (topSchema->id ? topSchema->id : "<anonymous>"));
      return NULL; /* for the compiler, bless its heart */
    }
    JSValueSpec *resolvedSchema = htGet(topSchema->definitions,key);
    if (resolvedSchema == NULL){
      pendingException->message = validityMessage(validator,"path schema ref '%s' does not resolve against '$defs'",ref);
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
                                                        ref+fileMatch.rm_so,matchLen(&fileMatch),
                                                        depth);
    if (referredTopSchema == NULL){
      pendingException->message = validityMessage(validator,"cross-domain schema not found for ref '%s'",ref);
      return NULL;
    } else {
      return resolveCrossSchemaRef(validator,pendingException,referredTopSchema,ref,&anchorMatch,depth);
    }
    /* Case 3, same-domain URL fragment (ie file) reference */
  } else if (refLen >= 1 && ((matchStatus = fileMatch(validator,ref)) == 0)){
    regmatch_t fileMatch = validator->matches[1];
    regmatch_t anchorMatch = validator->matches[2];
    JSValueSpec *referredTopSchema = getTopSchemaByName(validator,
                                                        containingSpec->hostName,strlen(containingSpec->hostName),
                                                        ref+fileMatch.rm_so,matchLen(&fileMatch),depth);
    if (referredTopSchema == NULL){
      pendingException->message = validityMessage(validator,"same-domain schema not found for ref '%s'",ref);
      return NULL;
    } else {
      return resolveCrossSchemaRef(validator,pendingException,referredTopSchema,ref,&anchorMatch,depth);
    }
    /* Case 4, just an anchor in same schema */
  } else if (refLen >= 1 && ref[0] == '#'){
    if (containingSpec->anchorTable == NULL){
      pendingException->message = validityMessage(validator,"schema '%s' does not define any anchors for ref '%s'",
                                                  (containingSpec->id ? containingSpec->id : "<anonymous>"),
                                                  ref);
      return NULL;
    }
    JSValueSpec *resolvedSchema = (JSValueSpec*)htGet(containingSpec->anchorTable,ref+1);
    if (resolvedSchema == NULL){
      pendingException->message = validityMessage(validator,"anchor schema ref '%s' does not resolve in containing schema",ref);
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
    pendingException->message = validityMessage(validator,"bad or unimplemented ref at '%s' at %s",
                                                valueSpec->ref,validatorAccessPath(validator));
    return NULL;
  }
}

static VResult validateJSONSimple(JsonValidator *validator,
                                  Json *value, JSValueSpec *valueSpec, int depth, EvalSet *evalSetList){
  if (jsonIsNull(value)){
    return validateType(validator,JSTYPE_NULL,valueSpec,depth+1);
  } else if (jsonIsBoolean(value)){
    return validateType(validator,JSTYPE_BOOLEAN,valueSpec,depth+1);
  } else if (jsonIsObject(value)){
    VResult typeResult = validateType(validator,JSTYPE_OBJECT,valueSpec,depth+1);
    if (vStatusValid(typeResult.status)){
      return validateJSONObject(validator,jsonAsObject(value),valueSpec,depth+1,evalSetList);
    } else {
      return typeResult;
    }
  } else if (jsonIsArray(value)){
    VResult typeResult = validateType(validator,JSTYPE_ARRAY,valueSpec,depth+1);
    if (vStatusValid(typeResult.status)){
      return validateJSONArray(validator,jsonAsArray(value),valueSpec,depth+1,evalSetList);
    } else {
      return typeResult;
    }
  } else if (jsonIsString(value)){
    VResult typeResult = validateType(validator,JSTYPE_STRING,valueSpec,depth+1);
    if (vStatusValid(typeResult.status)){
      return validateJSONString(validator,value,valueSpec,depth+1);
    } else {
      return typeResult;
    }
  } else if (jsonIsNumber(value)){
    if (!jsonIsInt64(value)){
      VResult typeResult = validateType(validator,JSTYPE_NUMBER,valueSpec,depth+1);
      if (vStatusValid(typeResult.status)){
        return validateJSONNumber(validator,value,valueSpec,depth+1); /* general, comparisons done as doubles */
      } else {
        return typeResult;
      }
    } else {
      VResult typeResult = validateType(validator,JSTYPE_INTEGER,valueSpec,depth+1);
      if (vStatusValid(typeResult.status)){
        return validateJSONInteger(validator,value,valueSpec,depth+1);
      } else {
        return typeResult;
      }
    }
    
  } else {
    trace(validator,1,"*** PANIC *** unhandled JS Value with type = %d\n",value->type);
    return simpleFailure(validator,"*** PANIC *** unhandled JS Value with type = %d\n",value->type);
  }
}

#define QUANT_ALL 1
#define QUANT_ANY 2
#define QUANT_ONE 3

static char *vqsTypeName(int x){
  switch (x){
  case QUANT_ALL: return "allOf";
  case QUANT_ANY: return "anyOf";
  case QUANT_ONE: return "oneOf";
  default: return "unknownQuantifier";
  }
}

static VResult validateQuantifiedSpecs(JsonValidator *validator,
                                       Json *value, JSValueSpec **valueSpecs,
                                       int specCount, int quantType, int depth, EvalSet *evalSetList){
  int validCount = 0;
  int firstValidIndex = -1;
  int secondValidIndex = -1;
  ValidityException *pendingException = makeValidityException(validator,NULL);
  for (int i=0; i<specCount; i++){
    JSValueSpec *valueSpec = valueSpecs[i];
    if (validator->traceLevel >= 1){
      trace(validator,depth,"vQS start i=%d type=%s vCount=%d\n",i,vqsTypeName(quantType),validCount);
    }
    VResult eltResult = validateJSON(validator,value,valueSpec,depth+1,evalSetList);
    if (vStatusValid(eltResult.status)){
      validCount++;
      switch (validCount){
      case 1: firstValidIndex = i;
      case 2: secondValidIndex = i;
      }
    } else {
      addValidityChild(pendingException,eltResult.exception);
    }
    switch (quantType){
    case QUANT_ALL:
      if (!vStatusValid(eltResult.status)){
        pendingException->message = validityMessage(validator,"not allOf schemas at '%s' are valid",
                                                    validatorAccessPath(validator));
        return failureWithException(pendingException);
      }
      break;
    case QUANT_ANY:
      if (validCount > 0){
        return simpleSuccess();
      }
    case QUANT_ONE:
      if (validCount > 1){
        return simpleFailure(validator,"more than oneOf schemas at '%s' are valid, at least subschemas %d and %d",
                             firstValidIndex,secondValidIndex,
                             validatorAccessPath(validator));
      } 
      break;
    }
    if (validator->traceLevel >= 1){
      trace(validator,depth,"vQS end i=%d type=%s valid=%d vCount=%d\n",i,vqsTypeName(quantType),eltResult.status,validCount);
    }
  }
  
  switch (quantType){
  case QUANT_ALL:
    return simpleSuccess();
  case QUANT_ANY:
    {
      pendingException->message = validityMessage(validator,"not anyOf schemas at '%s' are valid",
                                                  validatorAccessPath(validator));
      return failureWithException(pendingException);
    }
  case QUANT_ONE:
    /* trace(validator,"quantOne vCount=%d\n",validCount); */
    if (validCount == 0){
      pendingException->message = validityMessage(validator,"not oneOf schemas at '%s' are valid, 0 are",
                                                  validatorAccessPath(validator));
      return failureWithException(pendingException);
    } else {
      return simpleSuccess();
    }
  default:
    /* can't really get here */
    return simpleFailure(validator,"Internal Failure on quantifier");
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

static void addChildIfFailure(VResult failure, VResult r){
  if (!vStatusValid(r.status)){
    addValidityChild(failure.exception,r.exception);
  }
}

static VResult validateJSON(JsonValidator *validator, 
                            Json *value, JSValueSpec *valueSpec, int depth,
                            EvalSet *evalSetList){
  evalSetList = extendEvalSets(validator,evalSetList);
  if (validator->traceLevel >= 2){
    trace(validator,depth,"validateJSON top, value=0x%p, spec=0x%p value->type=%d, spec->mask=0x%x, ref=0x%p pthsz=%d\n",
	  value,valueSpec,value->type,valueSpec->typeMask,valueSpec->ref,validator->accessPath->currentSize);
    trace(validator,depth,"path: %s\n",validatorAccessPath(validator));
  }
  ValidityException *pendingException = makeValidityException(validator,NULL);
  while (valueSpec->ref){
    valueSpec = resolveRef(validator,pendingException,valueSpec,depth);
    if (valueSpec == NULL){
      VResult result;
      result.status = InvalidStop;
      result.exception = pendingException;
      return result;
    }
  }
  if (validator->traceLevel >= 1){
    trace(validator,depth,"validate JSON value->type=%d specTypeMask=0x%x\n",value->type,valueSpec->typeMask);
  }
  if (valueSpec->constValue){
    bool eq = jsonEquals(value,valueSpec->constValue);
    if (!eq){
      if (jsonIsString(valueSpec->constValue)) {
        return simpleFailure(validator,"unequal constant value at %s expecting value '%s' of type '%s'",
                             validatorAccessPath(validator), jsonAsString(valueSpec->constValue), getJSTypeName(valueSpec->type));
      } else if (jsonIsNumber(valueSpec->constValue)) {
        return simpleFailure(validator,"unequal constant value at %s expecting value '%d' of type '%s'",
                             validatorAccessPath(validator), jsonAsNumber(valueSpec->constValue), getJSTypeName(valueSpec->type));
      } else {
        return simpleFailure(validator,"unequal constant value at %s",
                             validatorAccessPath(validator));
      }
    } else {
      return simpleSuccess();
    }
  } else if (valueSpec->enumeratedValues){
    bool matched = false;
    /* it should at most require this much space... */
    char *validValues = SLHAlloc(validator->evalHeap, (valueSpec->enumeratedValuesCount * (sizeof(Json*) + 2)) + 1);
    for (int ee=0; ee<valueSpec->enumeratedValuesCount; ee++){
      Json *enumValue = valueSpec->enumeratedValues[ee];
      if (jsonEquals(value,enumValue)){
        matched = true;
        break;
      }
      if (jsonIsString(enumValue)) {
        strcat(validValues, jsonAsString(enumValue));
      } else if (jsonIsNumber(enumValue)) {
        int numberOfDigits = snprintf(NULL, 0, "%d", jsonAsNumber(enumValue));
        char *numberAsString = SLHAlloc(validator->evalHeap, numberOfDigits + 1);
        memset(numberAsString, 0, numberOfDigits + 1);
        snprintf(numberAsString, numberOfDigits + 1, "%d", jsonAsNumber(enumValue));
        strcat(validValues, jsonAsString(enumValue));
      }
      if (strlen(validValues) > 0 && ee < valueSpec->enumeratedValuesCount - 1) {
        strcat(validValues, ", ");
      }
    }
    if (!matched){
      if (strlen(validValues) > 0) {
        return simpleFailure(validator,"no matching enum value at %s expecting one of '[%s]' of type '%s'",
                             validatorAccessPath(validator), validValues, getJSTypeName(valueSpec->type));
      } else {
        return simpleFailure(validator,"no matching enum value at %s",
                             validatorAccessPath(validator));
      }
    } else {
      return simpleSuccess();
    }
  } else {
    /* type is string or array, does this mean to collapse ValueSpecTypes
       should we flag errors on out-of-bounds validation keywords for type keyword */
    /* here, mix in the quantified and compound */
    VResult allOfValidity = simpleSuccess();
    VResult anyOfValidity = simpleSuccess();
    VResult oneOfValidity = simpleSuccess();
    VResult notValidity = simpleSuccess();
    
    if (valueSpec->allOf){
      allOfValidity = validateQuantifiedSpecs(validator,
                                              value,valueSpec->allOf,valueSpec->allOfCount,QUANT_ALL,
                                              depth+1,evalSetList);
    }
    
    if (valueSpec->anyOf){
      anyOfValidity = validateQuantifiedSpecs(validator,
                                              value,valueSpec->anyOf,valueSpec->anyOfCount,QUANT_ANY,
                                              depth+1,evalSetList);
    }
    if (valueSpec->oneOf){
      oneOfValidity = validateQuantifiedSpecs(validator,
                                              value,valueSpec->oneOf,valueSpec->oneOfCount,QUANT_ONE,
                                              depth+1,evalSetList);
    }
    if (valueSpec->not){
      VResult subValidity = validateJSON(validator,value,valueSpec->not,depth+1,evalSetList);
      if (vStatusValid(subValidity.status)){
        notValidity = simpleFailure(validator,
                                    "negated schema at %s is valid",
                                    validatorAccessPath(validator));
      }
    }
    VResult simpleValidity = validateJSONSimple(validator,value,valueSpec,depth+1,evalSetList);
    VStatus totalValidity = vStatusMin(allOfValidity.status,
                                       vStatusMin(anyOfValidity.status,
                                                  vStatusMin(oneOfValidity.status,
                                                             vStatusMin(notValidity.status,simpleValidity.status))));
    if (validator->traceLevel >= 1){
      trace(validator,depth,"summing validity all=%s, any=%s, one=%s, not=%s simple=%s -> combined=%s\n",
            vStatusName(allOfValidity.status),
            vStatusName(anyOfValidity.status),
            vStatusName(oneOfValidity.status),
            vStatusName(notValidity.status),
            vStatusName(simpleValidity.status),
            vStatusName(totalValidity));
    }
    if (totalValidity == ValidContinue){
      return simpleSuccess();
    } else if ((valueSpec->allOf == NULL) &&
               (valueSpec->anyOf == NULL) &&
               (valueSpec->oneOf == NULL) &&
               (valueSpec->not == NULL) ){
      /* This is a special case to not create complex output for "simple" levels in the schema */
      return simpleValidity;
    } else {
      VResult failure = simpleFailure(validator,
                                      validityMessage(validator,"Schema at '%s' invalid",
                                                      validatorAccessPath(validator)));
      addChildIfFailure(failure,allOfValidity);
      addChildIfFailure(failure,anyOfValidity);
      addChildIfFailure(failure,oneOfValidity);
      addChildIfFailure(failure,notValidity);
      addChildIfFailure(failure,simpleValidity);
      return failure;
    }
  }
}

int jsonValidateSchema(JsonValidator *validator, Json *value, JsonSchema *topSchema,
                       JsonSchema **otherSchemas, int otherSchemaCount){
  int result = 0;
  if (setjmp(validator->recoveryData) == 0) {  /* normal execution */
    validator->topSchema = topSchema;
    validator->otherSchemas = otherSchemas;
    validator->otherSchemaCount = otherSchemaCount;
    if (validator->evalHeap){
      SLHFree(validator->evalHeap);
    }
    validator->evalHeap = makeShortLivedHeap(0x10000, 100);
    VResult validity = validateJSON(validator,value,topSchema->topValueSpec,0,NULL);
    if (vStatusValid(validity.status)){
      result = JSON_VALIDATOR_NO_EXCEPTIONS;
    } else {
      result = JSON_VALIDATOR_HAS_EXCEPTIONS;
      validator->topValidityException = validity.exception;
    }
  } else {
    trace(validator,0,"validation failed '%s'\n",validator->errorMessage);
    result = JSON_VALIDATOR_INTERNAL_FAILURE;
  }
  return result;
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
    spec->type = getJSTypeForName(builder,typeNameArray[i]);
    spec->typeMask |= (1 << spec->type);
    if (spec->type == JSTYPE_NUMBER){
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
    valueSpec->unevaluatedProperties = getBooleanValue(builder,object,"unevaluatedProperties",true);
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
