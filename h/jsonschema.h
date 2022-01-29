#ifndef __ZOWE_JSONSCHEMA__
#define __ZOWE_JSONSCHEMA__ 1

#ifdef METTLE

#else
#include <setjmp.h>
#endif

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

#define MAX_ACCESS_PATH 1024

typedef struct JsonValidator_tag {
  JsonSchema *schema;
  int         errorCode;
  char       *errorMessage;
  int         errorMessageLength;
  char        accessPathBuffer[MAX_ACCESS_PATH];
  AccessPath *accessPath;
  jmp_buf     recoveryData;
} JsonValidator;

#define JSON_SCHEMA_DRAFT_4 400
#define DEFAULT_JSON_SCHEMA_VERSION JSON_SCHEMA_DRAFT_4


JsonSchemaBuilder *makeJsonSchemaBuilder(int version);
void freeJsonSchemaBuilder(JsonSchemaBuilder *builder);
JsonSchema *jsonBuildSchema(JsonSchemaBuilder *builder, Json *jsValue);

JsonValidator *makeJsonValidator();
void freeJsonValidator(JsonValidator *validator);
int jsonValidateSchema(JsonValidator *validator, Json *value, JsonSchema *schema);


#endif 
