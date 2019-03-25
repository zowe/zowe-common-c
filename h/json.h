

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __JSON__
#define	__JSON__ 1

#ifdef METTLE
#  include <metal/metal.h>
#  include <metal/stdlib.h>
#else
#  include <stdlib.h>
#endif /* METTLE */
#include "zowetypes.h"
#include "utils.h"

/** \file
 *  \brief json.h is an implementation of an efficient low-level JSON writer and parser.
 *
 *  The JSON handling here is designed for high-speed streaming output behavior
 *  and chunked input and output.   
 */

/**
 *   \brief  jsonPrinter represents a high-level stream to write JSON.
 *
 *   It is not recommended to access the internal members of this struct.  Use the public functions of this
 *   file or unpredictable results will occur.  
 */

typedef struct jsonPrinter_tag {
  enum JsonCharsetConversion {
    JSON_MODE_NATIVE_CHARSET,
    JSON_MODE_CONVERT_TO_UTF8
  } mode;
  int fd; /* file or stream descriptor */
  int depth;
  int isStart;
  int isEnd;
  int isCustom;
  void (*customWrite)(struct jsonPrinter_tag *p, char *text, int len);
  void *customObject;
  char *indentString;
  int isFirstLine;
  int prettyPrint;
  int inputCCSID;
  //private
  size_t _conversionBufferSize;
  char *_conversionBuffer;
  int ioErrorFlag;
} jsonPrinter;

/** \fn makeJsonPrinter()
    makeJsonPrinter is the principal means of creating a logical stream for generating JSON output to a 
    lower level C stream.   This calls the generate output in this printer must be done in the order of generation, i.e. 
    this printer does not buffer a JSON "object" in memory and then flush when done.   It flushes output as it goes
    to whatever underlying stream (fd, really) that it is built upon.   It is mean to be fast and low-overhead supporting generating
    large objects of unknown size.
*/

jsonPrinter *makeJsonPrinter(int fd);
jsonPrinter *makeUtf8JsonPrinter(int fd, int inputCCSID);
jsonPrinter *makeCustomJsonPrinter(void (*writeMethod)(jsonPrinter *,char *,int), void *object);
jsonPrinter *makeCustomUtf8JsonPrinter(
    void (*writeMethod)(jsonPrinter *, char *, int),  void *object,
    int inputCCSID);

/** 
 *   \brief   This will change the JSON printing to generate newlines and indentation to make the output human-friendly.
 *
 *   This function is normally used for testing or low volume work.   It wastes memory and IO resources. 
 */

void jsonEnablePrettyPrint(jsonPrinter *p);

/** 
 * \brief  This will reclaim the memory of the internals of the jsonPrinter.  
 *
 * Note that this function does not change the state of underlying byte/character streams/fd/whatever.   
 */

void freeJsonPrinter(jsonPrinter *p);

/**  
 *   \brief This must be called at the beginning of JSON printing of a top-level object.
 */
void jsonStart(jsonPrinter *p);
/**
 *   \brief This must be called at the end of JSON printing of a top-level object.
 */
void jsonEnd(jsonPrinter *p);

/** 
 * \brief This will start a new object in the enclsing array or object.  
 *
 * The key should be NULL for array members.
 */

void jsonStartObject(jsonPrinter *p, char *keyOrNull);

/** 
 * \brief This will end object within the enclosing object or array.
 */

void jsonEndObject(jsonPrinter *p);

/**
 *  \brief   This will start a new array within the enclosing object or array.
 */

void jsonStartArray(jsonPrinter *p, char *keyOrNull);

/**
 *  \brief   This will end an array within the enclosing object or array.
 */

void jsonEndArray(jsonPrinter *p);

/**
 *  \brief   Adds a string to the enclosing object or array.
 *
 *  The key should be NULL for array members.
 */

void jsonAddString(jsonPrinter *p, char *keyOrNull, char *value);

/** 
 * \brief allows the caller to place well-formed JSON in the output stream into an enclosing object or array 
 *
 *  keyOrValue should be NULL when adding to an array
 */
void jsonAddJSONString(jsonPrinter *p, char *keyOrNull, char *validJsonValue);

/** 
 * \brief This adds a string to a streaming JSON object or array from a string that is not terminated.
 *
 * Many data structures have fixed length strings that need to be exported to JSON.  Use this function for those
 * Otherwise data would have to be copied to a secondary null-terminated string.  And that would be wrong.
 *
 * keyOrValue should be NULL when adding to an array
 */

void jsonAddUnterminatedString(jsonPrinter *p, char *keyOrNull, char *value, int valueLength);

/** 
 * \brief This adds a string to a streaming JSON object or array from a string that is not terminated.
 *
 * This function adds to jsonAddUnterminated string the feature that non-printing characters are pruned from the right
 * as it is written to the stream.
 *
 * keyOrValue should be NULL when adding to an array
 */


void jsonAddLimitedString(jsonPrinter *p, char *keyOrNull, char *value, int lengthLimit);

/** 
 * \brief This adds a boolean to a streaming JSON object or array from a string that is not terminated.
 *
 * keyOrValue should be NULL when adding to an array
 */

void jsonAddBoolean(jsonPrinter *p, char *keyOrNull, int value);

/** 
 * \brief This adds an int to a streaming JSON object or array from a string that is not terminated.
 *
 */


void jsonAddInt(jsonPrinter *p, char *keyOrNull, int value);

/**
 * \brief This adds an unsigned int to a streaming JSON object or array from a string that is not terminated.
 *
 */

void jsonAddUInt(jsonPrinter *p, char *keyOrNull, unsigned int value);

/** 
 * \brief This adds a signed 64 bit integer to a streaming JSON object or array from a string that is not terminated.
 *
 * keyOrValue should be NULL when adding to an array
 */


void jsonAddInt64(jsonPrinter *p, char *keyOrNull, int64 value);

/** 
 * \brief This adds a NULL to a streaming JSON object or array from a string that is not terminated.
 *
 * keyOrValue should be NULL when adding to an array
 */
void jsonAddNull(jsonPrinter *p, char *keyOrNull);

/** 
 * \brief This returns 1(true) if an I/O error occurred during writing into
 * the output stream.
 * 
 */
int jsonCheckIOErrorFlag(jsonPrinter *p);
/** 
 * \brief This sets I/O error flag in JSON printer.
 * 
 * The function is suppposed to be used in writeMethod with a custom JSON
 * printer when an I/O error occurred during writing into the output stream.
 * 
 */
void jsonSetIOErrorFlag(jsonPrinter *p);

typedef struct Json_tag Json;
typedef struct JsonObject_tag JsonObject;
typedef struct JsonArray_tag JsonArray;
typedef struct JsonProperty_tag JsonProperty;
typedef struct JsonError_tag JsonError;

struct Json_tag {
#define JSON_TYPE_NUMBER 0
#define JSON_TYPE_STRING 1
#define JSON_TYPE_BOOLEAN 2
#define JSON_TYPE_NULL 3
#define JSON_TYPE_OBJECT 4
#define JSON_TYPE_ARRAY 5
#define JSON_TYPE_ERROR 666
  int type;
  union {
    int number;
    char *string;
    int boolean;
    JsonObject *object;
    JsonArray *array;
    JsonError *error;
  } data;
};

struct JsonObject_tag {
  JsonProperty *firstProperty;
  JsonProperty *lastProperty;
};

struct JsonProperty_tag {
  char *key;
  Json *value;
  JsonProperty *next;
};

struct JsonArray_tag {
  Json **elements;
  int count;
  int capacity;
};

struct JsonError_tag {
  char *message;
};

Json *jsonParseString(ShortLivedHeap *slh, char *jsonString, char* errorBufferOrNull, int errorBufferSize);
Json *jsonParseUnterminatedString(ShortLivedHeap *slh, char *jsonString, int len, char* errorBufferOrNull, int errorBufferSize);
Json *jsonParseFile(ShortLivedHeap *slh, const char *filename , char* errorBufferOrNull, int errorBufferSize);

void jsonPrint(jsonPrinter *printer, Json *json);
void jsonPrintObject(jsonPrinter* printer, JsonObject *object);
void jsonPrintProperty(jsonPrinter* printer, JsonProperty *property);
void jsonPrintArray(jsonPrinter* printer, JsonArray *array);

/************ Cast-ish operators *********************/

int         jsonAsBoolean(Json *json);
int         jsonAsNumber(Json *json);
char       *jsonAsString(Json *json);
JsonArray  *jsonAsArray(Json *json);
JsonObject *jsonAsObject(Json *json);

/************* JSON Type Predicates ******************/

int jsonIsArray(Json *json);
int jsonIsBoolean(Json *json);
int jsonIsNull(Json *json);
int jsonIsNumber(Json *json);
int jsonIsObject(Json *json);
int jsonIsString(Json *json);

/************** Array Accessors ***********************/

int          jsonArrayGetCount(JsonArray *array);
Json        *jsonArrayGetItem(JsonArray *array, int n);
JsonArray   *jsonArrayGetArray(JsonArray *array, int i);
int          jsonArrayGetBoolean(JsonArray *array, int i);
int          jsonArrayGetNumber(JsonArray *array, int i);
JsonObject  *jsonArrayGetObject(JsonArray *array, int i);
char        *jsonArrayGetString(JsonArray *array, int i);

/* predicate to see if an array is really of a single JSON type */
int          jsonVerifyHomogeneity(JsonArray *array, int type);
/* predicate to test array containing string */
int jsonArrayContainsString(JsonArray *array, const char *s);

/************** Object Accessors *********************/

/**
 *   \brief  This is an accessor of a parsed JSON object that allows key/value iteration of an object.
 */

JsonProperty *jsonObjectGetFirstProperty(JsonObject *object);

/**
 *   \brief  This is an accessor of a parsed JSON object that allows key/value iteration of an object.
 */

JsonProperty *jsonObjectGetNextProperty(JsonProperty *property);

/**
 *   \brief  This is an accessor of a parsed JSON object property that gets the value.
 */

Json         *jsonPropertyGetValue(JsonProperty *property);

/**
 *   \brief  This is an accessor of a parsed JSON object property that gets the key.
 */


char         *jsonPropertyGetKey(JsonProperty *property);

/**
 *   \brief  This is an accessor of a parsed JSON object that gets a value based upon a key.
 */


Json         *jsonObjectGetPropertyValue(JsonObject *object, const char *key);

/**
 *   \brief  This is an accessor of a parsed JSON object that tests the presence of a given key 
 */

int           jsonObjectHasKey(JsonObject *object, const char *key);

/**
 *   \brief  This is an acccessor that gets an member of an object that is known to be an array
 *
 *   Results are unpredictable if it is not an array.
 */


JsonArray    *jsonObjectGetArray(JsonObject *object, const char *key);

/**
 *   \brief  This is an acccessor that gets an member of an object that is known to be a boolean
 *
 *   Results are unpredictable if it is not a boolean.
 */


int           jsonObjectGetBoolean(JsonObject *object, const char *key);

/**
 *   \brief  This is an acccessor that gets an member of an object that is known to be a JSON number
 *
 *   Results are unpredictable if it is not a JSON number.  It will be cast and truncated to an int
 */

int           jsonObjectGetNumber(JsonObject *object, const char *key);

/**
 *   \brief  This is an acccessor that gets an member of an object that is known to be a JSON object
 *
 *   Results are unpredictable if it is not a JSON object
 */

JsonObject   *jsonObjectGetObject(JsonObject *object, const char *key);

/**
 *   \brief  This is an acccessor that gets an member of an object that is known to be a JSON object
 *
 *   Results are unpredictable if it is not a string 
 */

char         *jsonObjectGetString(JsonObject *object, const char *key);

/**** COMMON CONVENIENCES FOR HANDLING JSON INPUT ******/

#define JSON_PROPERTY_OK              0
#define JSON_PROPERTY_NOT_FOUND       1
#define JSON_PROPERTY_UNEXPECTED_TYPE 2

JsonArray  *jsonArrayProperty(JsonObject *object, char *propertyName, int *status);
int         jsonIntProperty(JsonObject *object, char *propertyName, int *status, int defaultValue);
char       *jsonStringProperty(JsonObject *object, char *propertyName, int *status);
JsonObject *jsonObjectProperty(JsonObject *object, char *propertyName, int *status);
void        reportJSONDataProblem(void *jsonObject, int status, char *propertyName);

#endif	/* __JSON__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

